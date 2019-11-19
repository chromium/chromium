// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEVICE_DESCRIPTION_H_
#define MEDIA_AUDIO_AUDIO_DEVICE_DESCRIPTION_H_

#include <string>
#include <vector>

#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

// Provides common information on audio device names and ids.
struct MEDIA_EXPORT AudioDeviceDescription {
  // Unique Id of the generic "default" device. Associated with the localized
  // name returned from GetDefaultDeviceName().
  static const char kDefaultDeviceId[];

  // Unique Id of the generic default communications device. Associated with
  // the localized name returned from GetCommunicationsDeviceName().
  static const char kCommunicationsDeviceId[];

  // Input device ID used to capture the default system playback stream. When
  // this device ID is passed to MakeAudioInputStream() the returned
  // AudioInputStream will be capturing audio currently being played on the
  // default playback device. At the moment this feature is supported only on
  // some platforms. AudioInputStream::Intialize() will return an error on
  // platforms that don't support it. GetInputStreamParameters() must be used
  // to get the parameters of the loopback device before creating a loopback
  // stream, otherwise stream initialization may fail.
  static const char kLoopbackInputDeviceId[];

  // Similar to |kLoopbackInputDeviceId|, with only difference that this ID
  // will mute system audio during capturing.
  static const char kLoopbackWithMuteDeviceId[];

  // Returns true if |device_id| represents the default device.
  static bool IsDefaultDevice(const std::string& device_id);

  // Returns true if |device_id| represents the communications device.
  static bool IsCommunicationsDevice(const std::string& device_id);

  // Returns true if |device_id| represents a loopback audio capture device.
  static bool IsLoopbackDevice(const std::string& device_id);

  // If |device_id| is not empty, |session_id| should be ignored and the output
  // device should be selected basing on |device_id|.
  // If |device_id| is empty and |session_id| is nonzero, output device
  // associated with the opened input device designated by |session_id| should
  // be used.
  static bool UseSessionIdToSelectDevice(
      const base::UnguessableToken& session_id,
      const std::string& device_id);

  // The functions dealing with localization are not reliable in the audio
  // service, and should be avoided there.
  // Returns the localized name of the generic "default" device.
  static std::string GetDefaultDeviceName();

  // Returns a localized version of name of the generic "default" device that
  // includes the given |real_device_name|.
  static std::string GetDefaultDeviceName(const std::string& real_device_name);

  // Returns the localized name of the generic default communications device.
  // This device is not supported on all platforms.
  static std::string GetCommunicationsDeviceName();

  // Returns a localized version of name of the generic communications device
  // that includes the given |real_device_name|.
  static std::string GetCommunicationsDeviceName(
      const std::string& real_device_name);

  // This prepends localized "Default" or "Communications" strings to
  // default and communications device names in |device_descriptions|.
  static void LocalizeDeviceDescriptions(
      std::vector<AudioDeviceDescription>* device_descriptions);

  AudioDeviceDescription() = default;
  AudioDeviceDescription(const AudioDeviceDescription& other) = default;
  AudioDeviceDescription(std::string device_name,
                         std::string unique_id,
                         std::string group_id);

  ~AudioDeviceDescription() = default;

  std::string device_name;  // Friendly name of the device.
  std::string unique_id;    // Unique identifier for the device.
  std::string group_id;     // Group identifier.
};

typedef std::vector<AudioDeviceDescription> AudioDeviceDescriptions;

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEVICE_DESCRIPTION_H_
