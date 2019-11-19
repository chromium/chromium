// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/output_device_info.h"

namespace media {
class AudioSystem;
}

namespace content {

// This class, which lives on the IO thread, handles the logic of an IPC device
// request from the renderer. It checks which device to use (in case of using
// |session_id| to select device), verifies that the renderer is authorized to
// use the device, and gets the default device parameters for the selected audio
// device.
class CONTENT_EXPORT AudioOutputAuthorizationHandler {
 public:
  // Convention: Something named |device_id| is hashed and something named
  // |raw_device_id| is not hashed.

  // The result of an authorization check. In addition to the status, it
  // provides the default parameters of the device and the raw device id.
  // |device_id_for_renderer| is either the hashed device id, if it should be
  // sent to the renderer, or "", if it shouldn't.
  using AuthorizationCompletedCallback =
      base::OnceCallback<void(media::OutputDeviceStatus status,
                              const media::AudioParameters& params,
                              const std::string& raw_device_id,
                              const std::string& device_id_for_renderer)>;

  AudioOutputAuthorizationHandler(media::AudioSystem* audio_system,
                                  MediaStreamManager* media_stream_manager,
                                  int render_process_id_);

  ~AudioOutputAuthorizationHandler();

  // Checks authorization of the device with the hashed id |device_id| for the
  // given render frame id, or uses |session_id| for authorization. Looks up
  // device id (if |session_id| is used for device selection) and default
  // device parameters. This function will always call |cb|.
  void RequestDeviceAuthorization(int render_frame_id,
                                  const base::UnguessableToken& session_id,
                                  const std::string& device_id,
                                  AuthorizationCompletedCallback cb) const;

  // Calling this method will make the checks for permission from the user
  // always return |override_value|.
  void OverridePermissionsForTesting(bool override_value);

  static void UMALogDeviceAuthorizationTime(base::TimeTicks auth_start_time);

 private:
  // Helper class for recording traces.
  class TraceScope;

  void HashDeviceId(std::unique_ptr<TraceScope> trace_scope,
                    AuthorizationCompletedCallback cb,
                    const std::string& raw_device_id,
                    const MediaDeviceSaltAndOrigin& salt_and_origin) const;

  void AccessChecked(std::unique_ptr<TraceScope> trace_scope,
                     AuthorizationCompletedCallback cb,
                     const std::string& device_id,
                     std::string salt,
                     url::Origin security_origin,
                     bool has_access) const;

  void TranslateDeviceID(std::unique_ptr<TraceScope> trace_scope,
                         AuthorizationCompletedCallback cb,
                         const std::string& device_id,
                         const std::string& salt,
                         const url::Origin& security_origin,
                         const MediaDeviceEnumeration& enumeration) const;

  void GetDeviceParameters(std::unique_ptr<TraceScope> trace_scope,
                           AuthorizationCompletedCallback cb,
                           const std::string& raw_device_id) const;

  void DeviceParametersReceived(
      std::unique_ptr<TraceScope> trace_scope,
      AuthorizationCompletedCallback cb,
      const std::string& device_id_for_renderer,
      const std::string& raw_device_id,
      const base::Optional<media::AudioParameters>& params) const;

  media::AudioSystem* const audio_system_;
  MediaStreamManager* const media_stream_manager_;
  const int render_process_id_;
  bool override_permissions_ = false;
  bool permissions_override_value_ = false;

  // All access is on the IO thread, and taking a weak pointer to const looks
  // const, so this can be mutable.
  mutable base::WeakPtrFactory<const AudioOutputAuthorizationHandler>
      weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioOutputAuthorizationHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_AUTHORIZATION_HANDLER_H_
