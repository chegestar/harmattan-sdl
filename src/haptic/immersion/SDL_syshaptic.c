/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL.h"
#include "SDL_haptic.h"
#include "../SDL_syshaptic.h"

#include <ImmVibe.h>

#define GAME_PRIORITY         (VIBE_MAX_DEV_DEVICE_PRIORITY - 2)

struct haptic_hwdata
{
	VibeInt32 handle;
};

struct haptic_hweffect
{
	VibeInt32 handle;
};

int
SDL_SYS_HapticInit(void)
{
	if (VIBE_FAILED(ImmVibeInitialize(VIBE_CURRENT_VERSION_NUMBER))) {
		SDL_SetError("Failed to initialize the immersion library");
		return -1;
	}

	return ImmVibeGetDeviceCount();
}


const char *
SDL_SYS_HapticName(int index)
{
	static char buffer[VIBE_MAX_DEVICE_NAME_LENGTH];

	if (VIBE_SUCCEEDED(ImmVibeGetDeviceCapabilityString(index,
		VIBE_DEVCAPTYPE_DEVICE_NAME, VIBE_MAX_DEVICE_NAME_LENGTH, buffer))) {
		return buffer;
	}

	return NULL;
}


int
SDL_SYS_HapticOpen(SDL_Haptic * haptic)
{
	VibeInt32 flags;

	haptic->hwdata = (struct haptic_hwdata *)
		SDL_malloc(sizeof(*haptic->hwdata));
	if (!haptic->hwdata) {
		SDL_OutOfMemory();
		return -1;
	}

	if (VIBE_FAILED(ImmVibeOpenDevice(haptic->index, &haptic->hwdata->handle))) {
		SDL_SetError("Unable to open the Vibe device");
		goto open_err;
	}

	/* Set the proper device priority */
	if (VIBE_FAILED(ImmVibeSetDevicePropertyInt32(haptic->hwdata->handle,
		VIBE_DEVPROPTYPE_PRIORITY, GAME_PRIORITY))) {
		SDL_SetError("Unable to set game priority to the Vibe device");
		goto open_err;
	}

	/* Start filling the device capabilities. */
	if (VIBE_SUCCEEDED(ImmVibeGetDeviceCapabilityInt32(haptic->index,
		VIBE_DEVCAPTYPE_NUM_EFFECT_SLOTS, &flags))) {
		haptic->nplaying = flags;
	}

	if (haptic->nplaying <= 0) {
		SDL_SetError("Vibe device supports no effects");
		goto open_err;
	}

	/* We could technically support more non-playing effects, but even the
	 * Win32 backend is limited to 4, so I guess it does not matter. */
	haptic->neffects = haptic->nplaying;

	haptic->min_duration = 0;
	ImmVibeGetDeviceCapabilityInt32(haptic->index,
		VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION, &haptic->max_duration);

	/* Gain = Device Strength, Pause = Device Disabled */
	haptic->supported = SDL_HAPTIC_GAIN | SDL_HAPTIC_PAUSE | SDL_HAPTIC_STATUS;

	char periodic_support = 0;
	if (VIBE_SUCCEEDED(ImmVibeGetDeviceCapabilityInt32(haptic->index,
		VIBE_DEVCAPTYPE_SUPPORTED_EFFECTS, &flags))) {
		if (flags & VIBE_MAGSWEEP_EFFECT_SUPPORT)
			haptic->supported |= SDL_HAPTIC_CONSTANT;
		if (flags & VIBE_PERIODIC_EFFECT_SUPPORT)
			periodic_support = 1;
	}

	if (periodic_support &&
		VIBE_SUCCEEDED(ImmVibeGetDeviceCapabilityInt32(haptic->index,
			VIBE_DEVCAPTYPE_SUPPORTED_WAVE_TYPES, &flags))) {
		if (flags & VIBE_WAVETYPE_SQUARE_SUPPORT)
			haptic->supported |= SDL_HAPTIC_SQUARE;
		if (flags & VIBE_WAVETYPE_TRIANGLE_SUPPORT)
			haptic->supported |= SDL_HAPTIC_TRIANGLE;
		if (flags & VIBE_WAVETYPE_SINE_SUPPORT)
			haptic->supported |= SDL_HAPTIC_SINE;
		if (flags & VIBE_WAVETYPE_SAWTOOTHUP_SUPPORT)
			haptic->supported |= SDL_HAPTIC_SAWTOOTHUP;
		if (flags & VIBE_WAVETYPE_SAWTOOTHDOWN_SUPPORT)
			haptic->supported |= SDL_HAPTIC_SAWTOOTHDOWN;
	}

	/* Allocate memory for the effect structures. */
	haptic->effects = (struct haptic_effect *)
		SDL_malloc(sizeof(struct haptic_effect) * haptic->neffects);
	if (haptic->effects == NULL) {
		SDL_OutOfMemory();
		goto open_err;
	}
	SDL_memset(haptic->effects, 0,
	           sizeof(struct haptic_effect) * haptic->neffects);

	return 0;

open_err:
	if (haptic->hwdata) {
		if (VIBE_IS_VALID_DEVICE_HANDLE(haptic->hwdata->handle)) {
			ImmVibeCloseDevice(haptic->hwdata->handle);
		}
		SDL_free(haptic->hwdata);
		haptic->hwdata = NULL;
	}

	return -1;
}


int
SDL_SYS_HapticMouse(void)
{
	/* No idea how we can associate VibeTonz with mouse devices. */
	return -1;
}

#if 0 /* No Joystick feedback support in hsdl. */
int
SDL_SYS_JoystickIsHaptic(SDL_Joystick * joystick)
{
    return 0;
}


int
SDL_SYS_HapticOpenFromJoystick(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    SDL_SYS_LogicError();
    return -1;
}


int
SDL_SYS_JoystickSameHaptic(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    return 0;
}
#endif

void
SDL_SYS_HapticClose(SDL_Haptic * haptic)
{
	if (haptic->hwdata) {
		SDL_free(haptic->effects);
		haptic->effects = NULL;
		haptic->neffects = 0;
		ImmVibeCloseDevice(haptic->hwdata->handle);
		SDL_free(haptic->hwdata);
	}
	haptic->hwdata = NULL;
}


void
SDL_SYS_HapticQuit(void)
{
    ImmVibeTerminate();
}


int
SDL_SYS_HapticNewEffect(SDL_Haptic * haptic,
                        struct haptic_effect *effect, SDL_HapticEffect * base)
{
	effect->hweffect = (struct haptic_hweffect *)
		SDL_malloc(sizeof(struct haptic_hweffect));
	if (effect->hweffect == NULL) {
		SDL_OutOfMemory();
		return -1;
	}

	/* Do not actually allocate a VibeTonz effect until we are told to play */
	effect->hweffect->handle = VIBE_INVALID_EFFECT_HANDLE_VALUE;

	return 0;
}

static inline VibeInt32
SDL_SYS_ToVibeMagnitude(Sint16 level)
{
	long l = level;
	if (l < 0) {
		return VIBE_MAX_MAGNITUDE;
	} else {
		return (l * VIBE_MAX_MAGNITUDE) / 0x7FFF;
	}
}

static inline VibeInt32
SDL_SYS_ToVibeStyle(SDL_HapticPeriodic * data)
{
	switch (data->type) {
		case SDL_HAPTIC_SINE:
			return VIBE_WAVETYPE_SINE;
		case SDL_HAPTIC_SQUARE:
			return VIBE_WAVETYPE_SQUARE;
		case SDL_HAPTIC_TRIANGLE:
			return VIBE_WAVETYPE_TRIANGLE;
		case SDL_HAPTIC_SAWTOOTHUP:
			return VIBE_WAVETYPE_SAWTOOTHUP;
		case SDL_HAPTIC_SAWTOOTHDOWN:
			return VIBE_WAVETYPE_SAWTOOTHDOWN;
		default:
			return VIBE_DEFAULT_WAVETYPE;
	}
}

int
SDL_SYS_HapticRunEffect(SDL_Haptic * haptic, struct haptic_effect *effect,
                        Uint32 iterations)
{
	VibeInt32 status;

	if (VIBE_IS_VALID_EFFECT_HANDLE(effect->hweffect->handle)) {
		/* The effect is already there, stop it and restart */
		if (VIBE_FAILED(ImmVibeStopPlayingEffect(haptic->hwdata->handle,
			effect->hweffect->handle))) {
			return -1;
		}
		effect->hweffect->handle = VIBE_INVALID_EFFECT_HANDLE_VALUE;
	}

	if (iterations != 1) {
		SDL_SetError("Repeating constant effects are not supported.");
		return -1;
	}

	switch (effect->effect.type) {
		case SDL_HAPTIC_CONSTANT: {
			SDL_HapticConstant *data = &effect->effect.constant;
			status = ImmVibePlayMagSweepEffect(haptic->hwdata->handle,
				data->length, SDL_SYS_ToVibeMagnitude(data->level),
				VIBE_DEFAULT_STYLE,
				data->attack_length, SDL_SYS_ToVibeMagnitude(data->attack_level),
				data->fade_length, SDL_SYS_ToVibeMagnitude(data->fade_level),
				&effect->hweffect->handle);
			}
			break;
		case SDL_HAPTIC_SINE:
		case SDL_HAPTIC_SQUARE:
		case SDL_HAPTIC_TRIANGLE:
		case SDL_HAPTIC_SAWTOOTHUP:
		case SDL_HAPTIC_SAWTOOTHDOWN: {
			SDL_HapticPeriodic *data = &effect->effect.periodic;
			status = ImmVibePlayPeriodicEffect(haptic->hwdata->handle,
				data->length, SDL_SYS_ToVibeMagnitude(data->magnitude),
				data->period,
				VIBE_DEFAULT_STYLE | SDL_SYS_ToVibeStyle(data),
				data->attack_length, SDL_SYS_ToVibeMagnitude(data->attack_level),
				data->fade_length, SDL_SYS_ToVibeMagnitude(data->fade_level),
				&effect->hweffect->handle);
			}
			break;
		default:
		    SDL_SetError("Haptic: Unknown or unsupported effect type.");
		    return -1;
	}

	if (VIBE_FAILED(status)) {
		SDL_SetError("Haptic: Unable to play effect.");
		return -1;
	}

	return 0;
}

int
SDL_SYS_HapticUpdateEffect(SDL_Haptic * haptic,
                           struct haptic_effect *effect,
                           SDL_HapticEffect * data)
{
	VibeInt32 status;

	if (VIBE_IS_INVALID_EFFECT_HANDLE(effect->hweffect->handle)) {
		/* The effect was never uploaded to VibeTonz; nothing to do. */
		return 0;
	}

	switch (effect->effect.type) {
		case SDL_HAPTIC_CONSTANT: {
			SDL_HapticConstant *data = &effect->effect.constant;
			status = ImmVibeModifyPlayingMagSweepEffect(
				haptic->hwdata->handle, effect->hweffect->handle,
				data->length, SDL_SYS_ToVibeMagnitude(data->level),
				VIBE_DEFAULT_STYLE,
				data->attack_length, SDL_SYS_ToVibeMagnitude(data->attack_level),
				data->fade_length, SDL_SYS_ToVibeMagnitude(data->fade_level));
			}
			break;
		case SDL_HAPTIC_SINE:
		case SDL_HAPTIC_SQUARE:
		case SDL_HAPTIC_TRIANGLE:
		case SDL_HAPTIC_SAWTOOTHUP:
		case SDL_HAPTIC_SAWTOOTHDOWN: {
			SDL_HapticPeriodic *data = &effect->effect.periodic;
			status = ImmVibeModifyPlayingPeriodicEffect(
				haptic->hwdata->handle, effect->hweffect->handle,
				data->length, SDL_SYS_ToVibeMagnitude(data->magnitude),
				data->period,
				VIBE_DEFAULT_STYLE | SDL_SYS_ToVibeStyle(data),
				data->attack_length, SDL_SYS_ToVibeMagnitude(data->attack_level),
				data->fade_length, SDL_SYS_ToVibeMagnitude(data->fade_level));
			}
			break;
		default:
		    SDL_SetError("Haptic: Unknown or unsupported effect type.");
		    return -1;
	}

	if (VIBE_FAILED(status)) {
		SDL_SetError("Haptic: Unable to play effect.");
		return -1;
	}

	return 0;
}

int
SDL_SYS_HapticStopEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
	if (VIBE_IS_VALID_EFFECT_HANDLE(effect->hweffect->handle)) {
		if (VIBE_FAILED(ImmVibeStopPlayingEffect(haptic->hwdata->handle,
			effect->hweffect->handle))) {
			return -1;
		}
		effect->hweffect->handle = VIBE_INVALID_EFFECT_HANDLE_VALUE;
		/* Handle is now free to reuse. */
	}

	return 0;
}


void
SDL_SYS_HapticDestroyEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
	/* SDL does not stop the effect for us. */
	if (VIBE_IS_VALID_EFFECT_HANDLE(effect->hweffect->handle)) {
		ImmVibeStopPlayingEffect(haptic->hwdata->handle,
			effect->hweffect->handle);
	}
	SDL_free(effect->hweffect);
	effect->hweffect = NULL;
}


int
SDL_SYS_HapticGetEffectStatus(SDL_Haptic * haptic,
                              struct haptic_effect *effect)
{
	VibeInt32 state;

	if (VIBE_IS_INVALID_EFFECT_HANDLE(effect->hweffect->handle)) {
		/* If it was not yet sent to VibeTonz, then it is clearly not running */
		return 0;
	}

	if (VIBE_FAILED(ImmVibeGetEffectState(haptic->hwdata->handle,
		effect->hweffect->handle, &state))) {
		SDL_SetError("Failed to get VibeTonz state");
		return -1;
	}

	return state == VIBE_EFFECT_STATE_PLAYING;
}


int
SDL_SYS_HapticSetGain(SDL_Haptic * haptic, int gain)
{
	VibeInt32 strength = (gain * VIBE_MAX_MAGNITUDE) / 100;

	if (VIBE_FAILED(ImmVibeSetDevicePropertyInt32(haptic->hwdata->handle,
		VIBE_DEVPROPTYPE_STRENGTH, strength))) {
		SDL_SetError("Failed to set VibeTonz strength");
		return -1;
	}

	return 0;
}


int
SDL_SYS_HapticSetAutocenter(SDL_Haptic * haptic, int autocenter)
{
	SDL_SetError("Autocentering not supported");
	return -1;
}

int
SDL_SYS_HapticPause(SDL_Haptic * haptic)
{
	if (VIBE_FAILED(ImmVibeSetDevicePropertyBool(haptic->hwdata->handle,
		VIBE_DEVPROPTYPE_DISABLE_EFFECTS, VIBE_TRUE))) {
		SDL_SetError("Failed to disable VibeTonz");
		return -1;
	}

	return 0;
}

int
SDL_SYS_HapticUnpause(SDL_Haptic * haptic)
{
	if (VIBE_FAILED(ImmVibeSetDevicePropertyBool(haptic->hwdata->handle,
		VIBE_DEVPROPTYPE_DISABLE_EFFECTS, VIBE_FALSE))) {
		SDL_SetError("Failed to reenable VibeTonz");
		return -1;
	}

	return 0;
}

int
SDL_SYS_HapticStopAll(SDL_Haptic * haptic)
{
	int i, ret;

	if (VIBE_FAILED(ImmVibeStopAllPlayingEffects(haptic->hwdata->handle))) {
		SDL_SetError("Haptic: Error while trying to stop all playing effects.");
		return -1;
	}

	/* Invalidate all handles. */
	for (i = 0; i < haptic->neffects; i++) {
		struct haptic_hweffect *hweffect = haptic->effects[i].hweffect;
		if (hweffect != NULL) {
			hweffect->handle = VIBE_INVALID_EFFECT_HANDLE_VALUE;
        }
    }

    return 0;
}

