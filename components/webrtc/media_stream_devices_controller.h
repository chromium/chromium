// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace permissions {
enum class PermissionStatusSource;
}

namespace content {
class WebContents;
}

namespace webrtc {

class MediaStreamDeviceEnumerator;

// A class that provides logic for microphone/camera requests originating in the
// renderer.
class MediaStreamDevicesController {
 public:
  typedef base::OnceCallback<void(const blink::MediaStreamDevices& devices,
                                  blink::mojom::MediaStreamRequestResult result,
                                  bool blocked_by_feature_policy,
                                  ContentSetting audio_setting,
                                  ContentSetting video_setting)>
      ResultCallback;

  // Requests the mic/camera permissions described in |request|, using
  // |enumerator| to list the system's devices. The result of the request is
  // synchronously or asynchronously returned via |callback|.
  static void RequestPermissions(const content::MediaStreamRequest& request,
                                 MediaStreamDeviceEnumerator* enumerator,
                                 ResultCallback callback);

  ~MediaStreamDevicesController();

 private:
  MediaStreamDevicesController(content::WebContents* web_contents,
                               MediaStreamDeviceEnumerator* enumerator,
                               const content::MediaStreamRequest& request,
                               ResultCallback callback);
  MediaStreamDevicesController(const MediaStreamDevicesController&) = delete;
  MediaStreamDevicesController& operator=(MediaStreamDevicesController&) =
      delete;

  static void RequestAndroidPermissionsIfNeeded(
      content::WebContents* web_contents,
      std::unique_ptr<MediaStreamDevicesController> controller,
      bool did_prompt_for_audio,
      bool did_prompt_for_video,
      const std::vector<ContentSetting>& responses);

#if defined(OS_ANDROID)
  // Called when the Android OS-level prompt is answered.
  static void AndroidOSPromptAnswered(
      std::unique_ptr<MediaStreamDevicesController> controller,
      std::vector<ContentSetting> responses,
      bool android_prompt_granted);
#endif  // defined(OS_ANDROID)

  // Returns true if audio/video should be requested through the
  // PermissionManager. We won't try to request permission if the request is
  // already blocked for some other reason, e.g. there are no devices available.
  bool ShouldRequestAudio() const;
  bool ShouldRequestVideo() const;

  // Returns a list of devices available for the request for the given
  // audio/video permission settings.
  blink::MediaStreamDevices GetDevices(ContentSetting audio_setting,
                                       ContentSetting video_setting);

  // Runs |callback_| with the current audio/video permission settings.
  void RunCallback(bool blocked_by_feature_policy);

  // Returns the content settings for the given content type and request.
  ContentSetting GetContentSetting(
      ContentSettingsType content_type,
      const content::MediaStreamRequest& request,
      blink::mojom::MediaStreamRequestResult* denial_reason) const;

  // Returns true if clicking allow on the dialog should give access to the
  // requested devices.
  bool IsUserAcceptAllowed(ContentSettingsType content_type) const;

  bool PermissionIsBlockedForReason(
      ContentSettingsType content_type,
      permissions::PermissionStatusSource reason) const;

  // Called when a permission prompt is answered through the PermissionManager.
  void PromptAnsweredGroupedRequest(
      const std::vector<ContentSetting>& responses);

  bool HasAvailableDevices(ContentSettingsType content_type,
                           const std::string& device_id) const;

  // The current state of the audio/video content settings which may be updated
  // through the lifetime of the request.
  ContentSetting audio_setting_;
  ContentSetting video_setting_;
  blink::mojom::MediaStreamRequestResult denial_reason_;

  content::WebContents* web_contents_;

  // The object which lists available devices.
  MediaStreamDeviceEnumerator* enumerator_;

  // This enumerator is used as |enumerator_| when the instance passed into the
  // constructor is null.
  MediaStreamDeviceEnumeratorImpl owned_enumerator_;

  // The original request for access to devices.
  const content::MediaStreamRequest request_;

  // The callback that needs to be run to notify WebRTC of whether access to
  // audio/video devices was granted or not.
  ResultCallback callback_;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_
