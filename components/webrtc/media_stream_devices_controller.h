// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/webrtc/media_stream_device_enumerator_impl.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace blink {
enum class PermissionType;
}

namespace content {
enum class PermissionStatusSource;
class WebContents;
}

namespace webrtc {

class MediaStreamDeviceEnumerator;

// A class that provides logic for microphone/camera requests originating in the
// renderer.
class MediaStreamDevicesController {
 public:
  typedef base::OnceCallback<void(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      bool blocked_by_permissions_policy,
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
      const std::vector<blink::mojom::PermissionStatus>& responses);

  // Returns true if audio/video should be requested through the
  // PermissionManager. We won't try to request permission if the request is
  // already blocked for some other reason, e.g. there are no devices available.
  bool ShouldRequestAudio() const;
  bool ShouldRequestVideo() const;

  // Returns a list of devices available for the request for the given
  // audio/video permission settings.
  blink::mojom::StreamDevicesSetPtr GetDevices(ContentSetting audio_setting,
                                               ContentSetting video_setting);

  // Runs |callback_| with the current audio/video permission settings.
  void RunCallback(bool blocked_by_permissions_policy);

  // Returns the content settings for the given permission type and request.
  ContentSetting GetContentSetting(
      blink::PermissionType permission,
      const content::MediaStreamRequest& request,
      blink::mojom::MediaStreamRequestResult* denial_reason) const;

  // Returns true if clicking allow on the dialog should give access to the
  // requested devices.
  bool IsUserAcceptAllowed(blink::PermissionType permission) const;

  bool PermissionIsBlockedForReason(
      blink::PermissionType permission,
      content::PermissionStatusSource reason) const;

  // Called when a permission prompt is answered through the PermissionManager.
  void PromptAnsweredGroupedRequest(
      const std::vector<blink::mojom::PermissionStatus>& permissions_status);

  bool HasAvailableDevices(blink::PermissionType permission,
                           const std::vector<std::string>& device_ids) const;

  // The current state of the audio/video content settings which may be updated
  // through the lifetime of the request.
  ContentSetting audio_setting_;
  ContentSetting video_setting_;
  blink::mojom::MediaStreamRequestResult denial_reason_;

  raw_ptr<content::WebContents> web_contents_;

  // The object which lists available devices.
  raw_ptr<MediaStreamDeviceEnumerator> enumerator_;

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
