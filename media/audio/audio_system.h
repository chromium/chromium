// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Provides asynchronous interface to access audio device information
class MEDIA_EXPORT AudioSystem {
 public:
  // Replies are sent asynchronously to the thread the calls are issued on.
  // Instance is bound to the thread it's called on the first time.
  // Attention! Audio system thread may outlive the client
  // objects; bind callbacks with care.

  // Non-empty optional AudioParameters are guaranteed to be valid.
  // If optional AudioParameters are empty, it means the specified device is not
  // found. This is best-effort: non-empty parameters do not guarantee existence
  // of the device.
  // TODO(olka,tommi): fix all AudioManager implementations to always report
  // when a device is not found, instead of returning sub parameter values.
  // Non-empty optional matched output device id is guaranteed to be a non-empty
  // std::string. If optional matched output device id is empty, it means there
  // is no associated output device.
  using OnAudioParamsCallback =
      base::OnceCallback<void(const std::optional<AudioParameters>&)>;
  using OnDeviceIdCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;
  using OnInputDeviceInfoCallback =
      base::OnceCallback<void(const std::optional<AudioParameters>&,
                              const std::optional<std::string>&)>;

  using OnBoolCallback = base::OnceCallback<void(bool)>;
  using OnDeviceDescriptionsCallback =
      base::OnceCallback<void(AudioDeviceDescriptions)>;

  virtual ~AudioSystem() = default;

  virtual void GetInputStreamParameters(const std::string& device_id,
                                        OnAudioParamsCallback on_params_cb) = 0;

  virtual void GetOutputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) = 0;

  virtual void HasInputDevices(OnBoolCallback on_has_devices_cb) = 0;

  virtual void HasOutputDevices(OnBoolCallback on_has_devices_cb) = 0;

  // Replies with device descriptions of input audio devices if |for_input| is
  // true, and of output audio devices otherwise.
  virtual void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cb) = 0;

  // Replies with an empty optional if there is no associated output device
  // found and a non-empty string otherwise.
  virtual void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_cb) = 0;

  // Replies with audio parameters for the specified input device and
  // device ID of the associated output device, if any (otherwise
  // the associated output device ID is an empty optional).
  virtual void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_cb) = 0;

  // This function wraps |callback| with a call to
  // AudioDeviceDescription::LocalizeDeviceDescriptions for convenience. This is
  // typically used by AudioSystem implementations, not AudioSystem clients.
  static OnDeviceDescriptionsCallback WrapCallbackWithDeviceNameLocalization(
      OnDeviceDescriptionsCallback callback);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_H_
