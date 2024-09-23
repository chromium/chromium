// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_devices_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#include "ui/android/window_android.h"
#endif

using blink::MediaStreamDevices;

namespace webrtc {

namespace {

// Returns true if the given PermissionType is being requested in
// |request|.
bool PermissionIsRequested(blink::PermissionType permission,
                           const content::MediaStreamRequest& request) {
  if (permission == blink::PermissionType::AUDIO_CAPTURE)
    return request.audio_type ==
           blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;

  if (permission == blink::PermissionType::VIDEO_CAPTURE)
    return request.video_type ==
           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;

  return false;
}

}  // namespace

// static
void MediaStreamDevicesController::RequestPermissions(
    const content::MediaStreamRequest& request,
    MediaStreamDeviceEnumerator* enumerator,
    ResultCallback callback) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  // The RFH may have been destroyed by the time the request is processed.
  if (!rfh) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, false,
        {}, {});
    return;
  }

  if (rfh->GetLastCommittedOrigin().GetURL().is_empty()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, false, {},
        {});
    return;
  }

  if (rfh->GetLastCommittedOrigin().GetURL() != request.security_origin) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN, false,
        {}, {});
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  std::unique_ptr<MediaStreamDevicesController> controller(
      new MediaStreamDevicesController(web_contents, enumerator, request,
                                       std::move(callback)));

  std::vector<blink::PermissionType> permission_types;

  content::PermissionController* permission_controller =
      web_contents->GetBrowserContext()->GetPermissionController();

  std::vector<std::string> requested_audio_capture_device_ids;
  std::vector<std::string> requested_video_capture_device_ids;

  if (controller->ShouldRequestAudio()) {
    content::PermissionResult permission_status =
        permission_controller->GetPermissionResultForCurrentDocument(
            blink::PermissionType::AUDIO_CAPTURE, rfh);
    if (permission_status.status == blink::mojom::PermissionStatus::DENIED) {
      controller->denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      // If `rfh` is a fenced frame, it will have no permission policy as well.
      controller->RunCallback(
          permission_status.source ==
              content::PermissionStatusSource::FEATURE_POLICY ||
          permission_status.source ==
              content::PermissionStatusSource::FENCED_FRAME);
      return;
    }

    permission_types.push_back(blink::PermissionType::AUDIO_CAPTURE);
    requested_audio_capture_device_ids = request.requested_audio_device_ids;
  }
  if (controller->ShouldRequestVideo()) {
    content::PermissionResult permission_status =
        permission_controller->GetPermissionResultForCurrentDocument(
            blink::PermissionType::VIDEO_CAPTURE, rfh);
    if (permission_status.status == blink::mojom::PermissionStatus::DENIED) {
      controller->denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      // If `rfh` is a fenced frame, it will have no permission policy as well.
      controller->RunCallback(
          permission_status.source ==
              content::PermissionStatusSource::FEATURE_POLICY ||
          permission_status.source ==
              content::PermissionStatusSource::FENCED_FRAME);
      return;
    }

    permission_types.push_back(blink::PermissionType::VIDEO_CAPTURE);
    requested_video_capture_device_ids = request.requested_video_device_ids;

    bool has_pan_tilt_zoom_camera = controller->HasAvailableDevices(
        blink::PermissionType::CAMERA_PAN_TILT_ZOOM,
        request.requested_video_device_ids);

    // Request CAMERA_PAN_TILT_ZOOM only if the website requested the
    // pan-tilt-zoom permission and there are suitable PTZ capable devices
    // available.
    if (request.request_pan_tilt_zoom_permission && has_pan_tilt_zoom_camera) {
      permission_status =
          permission_controller->GetPermissionResultForCurrentDocument(
              blink::PermissionType::CAMERA_PAN_TILT_ZOOM, rfh);
      if (permission_status.status == blink::mojom::PermissionStatus::DENIED) {
        controller->denial_reason_ =
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
        controller->RunCallback(/*blocked_by_permissions_policy=*/false);
        return;
      }

      permission_types.push_back(blink::PermissionType::CAMERA_PAN_TILT_ZOOM);
    }
  }

  content::PermissionRequestDescription permission_request_description{
      permission_types, request.user_gesture};
  permission_request_description.requested_audio_capture_device_ids =
      requested_audio_capture_device_ids;
  permission_request_description.requested_video_capture_device_ids =
      requested_video_capture_device_ids;

  // It is OK to ignore `request.security_origin` because it will be calculated
  // from `render_frame_host` and we always ignore `requesting_origin` for
  // `AUDIO_CAPTURE` and `VIDEO_CAPTURE`.
  // `render_frame_host->GetMainFrame()->GetLastCommittedOrigin()` will be used
  // instead.
  rfh->GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionsFromCurrentDocument(
          rfh, permission_request_description,
          base::BindOnce(
              &MediaStreamDevicesController::PromptAnsweredGroupedRequest,
              std::move(controller)));
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, false,
        {}, {});
  }
}

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    MediaStreamDeviceEnumerator* enumerator,
    const content::MediaStreamRequest& request,
    ResultCallback callback)
    : web_contents_(web_contents),
      enumerator_(enumerator),
      request_(request),
      callback_(std::move(callback)) {
  DCHECK(network::IsUrlPotentiallyTrustworthy(request_.security_origin) ||
         request_.request_type == blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY);

  if (!enumerator_)
    enumerator_ = &owned_enumerator_;

  denial_reason_ = blink::mojom::MediaStreamRequestResult::OK;
  audio_setting_ = GetContentSetting(blink::PermissionType::AUDIO_CAPTURE,
                                     request, &denial_reason_);
  video_setting_ = GetContentSetting(blink::PermissionType::VIDEO_CAPTURE,
                                     request, &denial_reason_);
}

bool MediaStreamDevicesController::ShouldRequestAudio() const {
  return audio_setting_ == CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::ShouldRequestVideo() const {
  return video_setting_ == CONTENT_SETTING_ASK;
}

blink::mojom::StreamDevicesSetPtr MediaStreamDevicesController::GetDevices(
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  bool audio_allowed = audio_setting == CONTENT_SETTING_ALLOW;
  bool video_allowed = video_setting == CONTENT_SETTING_ALLOW;

  blink::mojom::StreamDevicesSetPtr stream_devices_set =
      blink::mojom::StreamDevicesSet::New();
  if (!audio_allowed && !video_allowed)
    return nullptr;

  // TODO(crbug.com/40216442): Generalize to multiple streams.
  stream_devices_set->stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set->stream_devices[0];
  switch (request_.request_type) {
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      // For open device request, when requested device_id is empty, pick
      // the first available of the given type. If requested device_id is
      // not empty, return the desired device if it's available. Otherwise,
      // return no device.
      if (audio_allowed &&
          request_.audio_type ==
              blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
        DCHECK_EQ(blink::mojom::MediaStreamType::NO_SERVICE,
                  request_.video_type);
        devices.audio_device =
            enumerator_->GetPreferredAudioDeviceForBrowserContext(
                web_contents_->GetBrowserContext(),
                request_.requested_audio_device_ids);
      } else if (video_allowed &&
                 request_.video_type ==
                     blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        DCHECK_EQ(blink::mojom::MediaStreamType::NO_SERVICE,
                  request_.audio_type);
        // Pepper API opens only one device at a time.
        devices.video_device =
            enumerator_->GetPreferredVideoDeviceForBrowserContext(
                web_contents_->GetBrowserContext(),
                request_.requested_video_device_ids);
      }
      break;
    }
    case blink::MEDIA_GENERATE_STREAM: {
      // Get the exact audio or video device if an id is specified.
      if (audio_allowed && !request_.requested_audio_device_ids.empty()) {
        devices.audio_device =
            enumerator_->GetPreferredAudioDeviceForBrowserContext(
                web_contents_->GetBrowserContext(),
                request_.requested_audio_device_ids);
      }
      if (video_allowed && !request_.requested_video_device_ids.empty()) {
        devices.video_device =
            enumerator_->GetPreferredVideoDeviceForBrowserContext(
                web_contents_->GetBrowserContext(),
                request_.requested_video_device_ids);
      }

      break;
    }
    case blink::MEDIA_GET_OPEN_DEVICE: {
      // Transferred tracks, that use blink::MEDIA_GET_OPEN_DEVICE type, do not
      // need to get permissions for MediaStreamDevice as those are controlled
      // by the original context.
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case blink::MEDIA_DEVICE_ACCESS: {
      // Get the preferred devices for the request.
      if (audio_allowed) {
        // `SpeechRecognitionManagerImpl` doesn't provide a list of requested
        // devices, so just get the most preferred device without filtering.
        CHECK(request_.requested_audio_device_ids.empty());
        devices.audio_device =
            enumerator_->GetPreferredAudioDeviceForBrowserContext(
                web_contents_->GetBrowserContext(), {});
      }
      // MEDIA_DEVICE_ACCESS is only used for speech recognition, so it never
      // requests video access.
      CHECK(!video_allowed);
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }  // switch

  return stream_devices_set;
}

void MediaStreamDevicesController::RunCallback(
    bool blocked_by_permissions_policy) {
  CHECK(callback_);

  blink::mojom::StreamDevicesSetPtr stream_devices_set;
  // If all requested permissions are allowed then the callback should report
  // success, otherwise we report |denial_reason_|.
  blink::mojom::MediaStreamRequestResult request_result =
      blink::mojom::MediaStreamRequestResult::OK;
  if ((audio_setting_ == CONTENT_SETTING_ALLOW ||
       audio_setting_ == CONTENT_SETTING_DEFAULT) &&
      (video_setting_ == CONTENT_SETTING_ALLOW ||
       video_setting_ == CONTENT_SETTING_DEFAULT)) {
    stream_devices_set = GetDevices(audio_setting_, video_setting_);
    if (stream_devices_set->stream_devices.empty()) {
      // Even if all requested permissions are allowed, if there are no devices
      // at this point we still report a failure.
      request_result = blink::mojom::MediaStreamRequestResult::NO_HARDWARE;
    }
  } else {
    DCHECK_NE(blink::mojom::MediaStreamRequestResult::OK, denial_reason_);
    request_result = denial_reason_;
    stream_devices_set = blink::mojom::StreamDevicesSet::New();
  }

  std::move(callback_).Run(*stream_devices_set, request_result,
                           blocked_by_permissions_policy, audio_setting_,
                           video_setting_);
}

ContentSetting MediaStreamDevicesController::GetContentSetting(
    blink::PermissionType permission,
    const content::MediaStreamRequest& request,
    blink::mojom::MediaStreamRequestResult* denial_reason) const {
  DCHECK(permission == blink::PermissionType::AUDIO_CAPTURE ||
         permission == blink::PermissionType::VIDEO_CAPTURE);
  DCHECK(!request_.security_origin.is_empty());
  DCHECK(network::IsUrlPotentiallyTrustworthy(request_.security_origin) ||
         request_.request_type == blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY);
  if (!PermissionIsRequested(permission, request)) {
    // No denial reason set as it will have been previously set.
    return CONTENT_SETTING_DEFAULT;
  }

  std::vector<std::string> device_ids;
  if (permission == blink::PermissionType::AUDIO_CAPTURE)
    device_ids = request.requested_audio_device_ids;
  else
    device_ids = request.requested_video_device_ids;
  if (!HasAvailableDevices(permission, device_ids)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::NO_HARDWARE;
    return CONTENT_SETTING_BLOCK;
  }

  if (!IsUserAcceptAllowed(permission)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  // Don't request if the kill switch is on.
  if (PermissionIsBlockedForReason(
          permission, content::PermissionStatusSource::KILL_SWITCH)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

  return CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::IsUserAcceptAllowed(
    blink::PermissionType permission) const {
#if BUILDFLAG(IS_ANDROID)
  ui::WindowAndroid* window_android =
      web_contents_->GetNativeView()->GetWindowAndroid();
  if (!window_android)
    return false;

  ContentSettingsType content_type;

  switch (permission) {
    case blink::PermissionType::AUDIO_CAPTURE:
      content_type = ContentSettingsType::MEDIASTREAM_MIC;
      break;
    case blink::PermissionType::VIDEO_CAPTURE:
      content_type = ContentSettingsType::MEDIASTREAM_CAMERA;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  std::vector<std::string> required_android_permissions;
  permissions::AppendRequiredAndroidPermissionsForContentSetting(
      content_type, &required_android_permissions);
  for (const auto& android_permission : required_android_permissions) {
    if (!window_android->HasPermission(android_permission) &&
        !window_android->CanRequestPermission(android_permission)) {
      return false;
    }
  }

  // Don't approve device requests if the tab was hidden.
  // TODO(qinmin): Add a test for this. http://crbug.com/396869.
  // TODO(raymes): Shouldn't this apply to all permissions not just audio/video?
  return web_contents_->GetRenderWidgetHostView()->IsShowing();
#else
  return true;
#endif
}

bool MediaStreamDevicesController::PermissionIsBlockedForReason(
    blink::PermissionType permission,
    content::PermissionStatusSource reason) const {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_.render_process_id, request_.render_frame_id);
  if (rfh->GetLastCommittedOrigin().GetURL() != request_.security_origin) {
    return false;
  }

  // TODO(raymes): This function wouldn't be needed if
  // PermissionManager::RequestPermissions returned a denial reason.
  content::PermissionResult result =
      web_contents_->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionResultForCurrentDocument(permission, rfh);
  if (result.source == reason) {
    DCHECK_EQ(blink::mojom::PermissionStatus::DENIED, result.status);
    return true;
  }
  return false;
}

void MediaStreamDevicesController::PromptAnsweredGroupedRequest(
    const std::vector<blink::mojom::PermissionStatus>& permissions_status) {
  if (content::RenderFrameHost::FromID(request_.render_process_id,
                                       request_.render_frame_id) == nullptr) {
    // The frame requesting media devices was removed while we were waiting for
    // a user response on permissions. Nothing more to do.
    return;
  }

  std::vector<ContentSetting> responses;
  base::ranges::transform(
      permissions_status, back_inserter(responses),
      permissions::PermissionUtil::PermissionStatusToContentSetting);

  bool need_audio = ShouldRequestAudio();
  bool need_video = ShouldRequestVideo();
  bool blocked_by_permissions_policy = need_audio || need_video;
  // The audio setting will always be the first one in the vector, if it was
  // requested.
  if (need_audio) {
    audio_setting_ = responses.front();
    blocked_by_permissions_policy &=
        audio_setting_ == CONTENT_SETTING_BLOCK &&
        PermissionIsBlockedForReason(
            blink::PermissionType::AUDIO_CAPTURE,
            content::PermissionStatusSource::FEATURE_POLICY);
  }

  if (need_video) {
    video_setting_ = responses.at(need_audio ? 1 : 0);
    blocked_by_permissions_policy &=
        video_setting_ == CONTENT_SETTING_BLOCK &&
        PermissionIsBlockedForReason(
            blink::PermissionType::VIDEO_CAPTURE,
            content::PermissionStatusSource::FEATURE_POLICY);
  }

  for (ContentSetting response : responses) {
    if (response == CONTENT_SETTING_BLOCK)
      denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    else if (response == CONTENT_SETTING_ASK)
      denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED;
  }

  RunCallback(blocked_by_permissions_policy);
}

bool MediaStreamDevicesController::HasAvailableDevices(
    blink::PermissionType permission,
    const std::vector<std::string>& device_ids) const {
  const MediaStreamDevices* devices = nullptr;
  if (permission == blink::PermissionType::AUDIO_CAPTURE) {
    devices = &enumerator_->GetAudioCaptureDevices();
  } else if (permission == blink::PermissionType::VIDEO_CAPTURE ||
             permission == blink::PermissionType::CAMERA_PAN_TILT_ZOOM) {
    devices = &enumerator_->GetVideoCaptureDevices();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  // TODO(tommi): It's kind of strange to have this here since if we fail this
  // test, there'll be a UI shown that indicates to the user that access to
  // non-existing audio/video devices has been denied.  The user won't have
  // any way to change that but there will be a UI shown which indicates that
  // access is blocked.
  if (devices->empty())
    return false;

  // If there are no particular device requirements, all devices will do.
  if (device_ids.empty() &&
      permission != blink::PermissionType::CAMERA_PAN_TILT_ZOOM) {
    return true;
  }

  // Try to find a device which fulfils all device requirements.
  for (const blink::MediaStreamDevice& device : *devices) {
    if (!device_ids.empty() && std::find(device_ids.begin(), device_ids.end(),
                                         device.id) == device_ids.end()) {
      continue;
    }
    if (permission == blink::PermissionType::CAMERA_PAN_TILT_ZOOM &&
        !device.video_control_support.pan &&
        !device.video_control_support.tilt &&
        !device.video_control_support.zoom) {
      continue;
    }
    return true;
  }

  return false;
}

}  // namespace webrtc
