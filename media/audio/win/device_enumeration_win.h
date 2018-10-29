// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_
#define MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_

#include <string>

#include "media/audio/audio_device_name.h"
#include "media/base/media_export.h"

namespace media {

// Returns a list of audio input or output device structures (name and
// unique device ID) using the MMDevice API which is supported on
// Windows Vista and higher.
// Example record in the output list:
// - device_name: "Microphone (Realtek High Definition Audio)".
// - unique_id: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}"
// This method must be called from a COM thread using MTA.
bool GetInputDeviceNamesWin(media::AudioDeviceNames* device_names);
bool GetOutputDeviceNamesWin(media::AudioDeviceNames* device_names);

// Returns a list of audio output device structures (name and
// unique device ID) using the WaveIn API which is supported on
// Windows XP and higher.
// Example record in the output list:
// - device_name: "Microphone (Realtek High Defini".
// - unique_id: "Microphone (Realtek High Defini" (same as friendly name).
bool GetOutputDeviceNamesWinXP(media::AudioDeviceNames* device_names);

// Given a string |controller_id| with the controller ID of an audio device,
// returns a string containing extra information about the device.
// If the device is a USB device, the format of the returned string is
// " (vid:pid)", with vid and pid being 4-character lowercase hexadecimal
// numbers.
// If the device is a Bluetooth device, the returned string is " (Bluetooth)".
// This string is intended to be appended to a device-name string without any
// further formatting.
// If |controller_id| does not refer to a USB or Bluetooth device, this
// function returns an empty string.
MEDIA_EXPORT std::string GetDeviceSuffixWin(const std::string& controller_id);

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_

