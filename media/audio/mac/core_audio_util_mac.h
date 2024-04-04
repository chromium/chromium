// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_
#define MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"

namespace media {
namespace core_audio_mac {

// Returns a vector with the IDs of all audio devices in the system.
// The vector is empty if there are no devices or if there is an error.
std::vector<AudioObjectID> GetAllAudioDeviceIDs();

// Returns a vector with the IDs of all devices related to the given
// |device_id|. The vector is empty if there are no related devices or
// if there is an error.
std::vector<AudioObjectID> GetRelatedDeviceIDs(AudioObjectID device_id);

// Returns a string with a unique device ID for the given |device_id|, or no
// value if there is an error.
std::optional<std::string> GetDeviceUniqueID(AudioObjectID device_id);

// Returns a string with a descriptive label for the given |device_id|, or no
// value if there is an error. The returned label is based on several
// characteristics of the device.
std::optional<std::string> GetDeviceLabel(AudioObjectID device_id,
                                          bool is_input);

// Returns the number of input or output streams associated with the given
// |device_id|. Returns zero if there are no streams or if there is an error.
uint32_t GetNumStreams(AudioObjectID device_id, bool is_input);

// Returns the source associated with the given |device_id|, or no value if
// |device_id| has no source or if there is an error.
std::optional<uint32_t> GetDeviceSource(AudioObjectID device_id, bool is_input);

// Returns the transport type of the given |device_id|, or no value if
// |device_id| has no source or if there is an error.
std::optional<uint32_t> GetDeviceTransportType(AudioObjectID device_id);

// Returns whether or not the |device_id| corresponds to a private, aggregate
// device. Such a device gets created by instantiating a VoiceProcessingIO
// AudioUnit.
bool IsPrivateAggregateDevice(AudioObjectID device_id);

// Returns whether or not the |device_id| corresponds to a device that has valid
// input streams. When the VoiceProcessing AudioUnit is active, some output
// devices get an input stream as well. This function tries to filter those out,
// based on the value of the stream's kAudioStreamPropertyTerminalType value.
bool IsInputDevice(AudioObjectID device_id);

// Returns whether or not the |device_id| corresponds to a device with output
// streams.
bool IsOutputDevice(AudioObjectID device_id);

// Returns the latency for the given audio unit and device. Total latency is
// the sum of the latency of the AudioUnit, device, and stream. If any one
// component of the latency can't be retrieved it is considered as zero.
base::TimeDelta GetHardwareLatency(AudioUnit audio_unit,
                                   AudioDeviceID device_id,
                                   AudioObjectPropertyScope scope,
                                   int sample_rate,
                                   bool is_input);

}  // namespace core_audio_mac
}  // namespace media

#endif  // MEDIA_AUDIO_MAC_CORE_AUDIO_UTIL_MAC_H_
