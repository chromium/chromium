// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_devices_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#include "ui/android/window_android.h"
#endif

using blink::MediaStreamDevices;

namespace webrtc {

namespace {

// Returns true if the given ContentSettingsType is being requested in
// |request|.
bool ContentTypeIsRequested(ContentSettingsType type,
                            const content::MediaStreamRequest& request) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC)
    return request.audio_type ==
           blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;

  if (type == ContentSettingsType::MEDIASTREAM_CAMERA)
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
        MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, false,
        {}, {});
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  std::unique_ptr<MediaStreamDevicesController> controller(
      new MediaStreamDevicesController(web_contents, enumerator, request,
                                       std::move(callback)));

  std::vector<ContentSettingsType> content_settings_types;

  permissions::PermissionManager* permission_manager =
      permissions::PermissionsClient::Get()->GetPermissionManager(
          web_contents->GetBrowserContext());
  bool will_prompt_for_audio = false;
  bool will_prompt_for_video = false;

  if (controller->ShouldRequestAudio()) {
    permissions::PermissionResult permission_status =
        permission_manager->GetPermissionStatusForFrame(
            ContentSettingsType::MEDIASTREAM_MIC, rfh, request.security_origin);
    if (permission_status.content_setting == CONTENT_SETTING_BLOCK) {
      controller->denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      controller->RunCallback(
          permission_status.source ==
          permissions::PermissionStatusSource::FEATURE_POLICY);
      return;
    }

    content_settings_types.push_back(ContentSettingsType::MEDIASTREAM_MIC);
    will_prompt_for_audio =
        permission_status.content_setting == CONTENT_SETTING_ASK;
  }
  if (controller->ShouldRequestVideo()) {
    permissions::PermissionResult permission_status =
        permission_manager->GetPermissionStatusForFrame(
            ContentSettingsType::MEDIASTREAM_CAMERA, rfh,
            request.security_origin);
    if (permission_status.content_setting == CONTENT_SETTING_BLOCK) {
      controller->denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
      controller->RunCallback(
          permission_status.source ==
          permissions::PermissionStatusSource::FEATURE_POLICY);
      return;
    }

    content_settings_types.push_back(ContentSettingsType::MEDIASTREAM_CAMERA);
    will_prompt_for_video =
        permission_status.content_setting == CONTENT_SETTING_ASK;

    bool has_pan_tilt_zoom_camera = controller->HasAvailableDevices(
        ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
        request.requested_video_device_id);
    base::UmaHistogramBoolean("WebRTC.MediaStreamDevices.HasPanTiltZoomCamera",
                              has_pan_tilt_zoom_camera);
    // Request CAMERA_PAN_TILT_ZOOM only if the the website requested
    // the pan-tilt-zoom permission and there are suitable PTZ capable devices
    // available.
    if (request.request_pan_tilt_zoom_permission && has_pan_tilt_zoom_camera) {
      permissions::PermissionResult permission_status =
          permission_manager->GetPermissionStatusForFrame(
              ContentSettingsType::CAMERA_PAN_TILT_ZOOM, rfh,
              request.security_origin);
      if (permission_status.content_setting == CONTENT_SETTING_BLOCK) {
        controller->denial_reason_ =
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
        controller->RunCallback(/*blocked_by_feature_policy=*/false);
        return;
      }

      content_settings_types.push_back(
          ContentSettingsType::CAMERA_PAN_TILT_ZOOM);
    }
  }

  permission_manager->RequestPermissions(
      content_settings_types, rfh, request.security_origin,
      request.user_gesture,
      base::BindOnce(
          &MediaStreamDevicesController::RequestAndroidPermissionsIfNeeded,
          web_contents, base::Passed(&controller), will_prompt_for_audio,
          will_prompt_for_video));
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        MediaStreamDevices(),
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
  DCHECK(blink::network_utils::IsOriginSecure(request_.security_origin) ||
         request_.request_type == blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY);

  if (!enumerator_)
    enumerator_ = &owned_enumerator_;

  denial_reason_ = blink::mojom::MediaStreamRequestResult::OK;
  audio_setting_ = GetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                                     request, &denial_reason_);
  video_setting_ = GetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                                     request, &denial_reason_);
}

void MediaStreamDevicesController::RequestAndroidPermissionsIfNeeded(
    content::WebContents* web_contents,
    std::unique_ptr<MediaStreamDevicesController> controller,
    bool did_prompt_for_audio,
    bool did_prompt_for_video,
    const std::vector<ContentSetting>& responses) {
#if defined(OS_ANDROID)
  // If either audio or video was previously allowed and Chrome no longer has
  // the necessary permissions, show a infobar to attempt to address this
  // mismatch.
  std::vector<ContentSettingsType> content_settings_types;
  // The audio setting will always be the first one in the vector, if it was
  // requested.
  // If the user was already prompted for mic (|did_prompt_for_audio| flag), we
  // would have requested Android permission at that point.
  if (!did_prompt_for_audio && controller->ShouldRequestAudio() &&
      responses.front() == CONTENT_SETTING_ALLOW) {
    content_settings_types.push_back(ContentSettingsType::MEDIASTREAM_MIC);
  }

  // If the user was already prompted for camera (|did_prompt_for_video| flag),
  // we would have requested Android permission at that point.
  if (!did_prompt_for_video && controller->ShouldRequestVideo() &&
      responses.back() == CONTENT_SETTING_ALLOW) {
    content_settings_types.push_back(ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  // If the user was already prompted for camera (|did_prompt_for_video| flag),
  // we would have requested Android permission at that point.
  if (!did_prompt_for_video && controller->ShouldRequestVideo() &&
      responses.back() == CONTENT_SETTING_ALLOW) {
    content_settings_types.push_back(ContentSettingsType::MEDIASTREAM_CAMERA);
  }
  if (content_settings_types.empty()) {
    controller->PromptAnsweredGroupedRequest(responses);
    return;
  }

  permissions::PermissionRepromptState reprompt_state =
      permissions::ShouldRepromptUserForPermissions(web_contents,
                                                    content_settings_types);
  switch (reprompt_state) {
    case permissions::PermissionRepromptState::kNoNeed:
      controller->PromptAnsweredGroupedRequest(responses);
      return;

    case permissions::PermissionRepromptState::kShow:
      permissions::PermissionsClient::Get()->RepromptForAndroidPermissions(
          web_contents, content_settings_types,
          base::BindOnce(&MediaStreamDevicesController::AndroidOSPromptAnswered,
                         std::move(controller), responses));
      return;

    case permissions::PermissionRepromptState::kCannotShow: {
      std::vector<ContentSetting> blocked_responses(responses.size(),
                                                    CONTENT_SETTING_BLOCK);
      controller->PromptAnsweredGroupedRequest(blocked_responses);
      return;
    }
  }

  NOTREACHED() << "Unknown show permission infobar state.";
#else
  controller->PromptAnsweredGroupedRequest(responses);
#endif
}

#if defined(OS_ANDROID)
// static
void MediaStreamDevicesController::AndroidOSPromptAnswered(
    std::unique_ptr<MediaStreamDevicesController> controller,
    std::vector<ContentSetting> responses,
    bool android_prompt_granted) {
  if (!android_prompt_granted) {
    // Only permissions that were previously ALLOW for a site will have had
    // their android permissions requested. It's only in that case that we need
    // to change the setting to BLOCK to reflect that it wasn't allowed.
    for (size_t i = 0; i < responses.size(); ++i) {
      if (responses[i] == CONTENT_SETTING_ALLOW)
        responses[i] = CONTENT_SETTING_BLOCK;
    }
  }

  controller->PromptAnsweredGroupedRequest(responses);
}
#endif  // defined(OS_ANDROID)

bool MediaStreamDevicesController::ShouldRequestAudio() const {
  return audio_setting_ == CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::ShouldRequestVideo() const {
  return video_setting_ == CONTENT_SETTING_ASK;
}

MediaStreamDevices MediaStreamDevicesController::GetDevices(
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  bool audio_allowed = audio_setting == CONTENT_SETTING_ALLOW;
  bool video_allowed = video_setting == CONTENT_SETTING_ALLOW;

  if (!audio_allowed && !video_allowed)
    return MediaStreamDevices();

  MediaStreamDevices devices;
  switch (request_.request_type) {
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      const blink::MediaStreamDevice* device = nullptr;
      // For open device request, when requested device_id is empty, pick
      // the first available of the given type. If requested device_id is
      // not empty, return the desired device if it's available. Otherwise,
      // return no device.
      if (audio_allowed &&
          request_.audio_type ==
              blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
        DCHECK_EQ(blink::mojom::MediaStreamType::NO_SERVICE,
                  request_.video_type);
        if (!request_.requested_audio_device_id.empty()) {
          device = enumerator_->GetRequestedAudioDevice(
              request_.requested_audio_device_id);
        } else {
          const blink::MediaStreamDevices& audio_devices =
              enumerator_->GetAudioCaptureDevices();
          if (!audio_devices.empty())
            device = &audio_devices.front();
        }
      } else if (video_allowed &&
                 request_.video_type ==
                     blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        DCHECK_EQ(blink::mojom::MediaStreamType::NO_SERVICE,
                  request_.audio_type);
        // Pepper API opens only one device at a time.
        if (!request_.requested_video_device_id.empty()) {
          device = enumerator_->GetRequestedVideoDevice(
              request_.requested_video_device_id);
        } else {
          const blink::MediaStreamDevices& video_devices =
              enumerator_->GetVideoCaptureDevices();
          if (!video_devices.empty())
            device = &video_devices.front();
        }
      }
      if (device)
        devices.push_back(*device);
      break;
    }
    case blink::MEDIA_GENERATE_STREAM: {
      bool get_default_audio_device = audio_allowed;
      bool get_default_video_device = video_allowed;

      // Get the exact audio or video device if an id is specified.
      if (audio_allowed && !request_.requested_audio_device_id.empty()) {
        const blink::MediaStreamDevice* audio_device =
            enumerator_->GetRequestedAudioDevice(
                request_.requested_audio_device_id);
        if (audio_device) {
          devices.push_back(*audio_device);
          get_default_audio_device = false;
        }
      }
      if (video_allowed && !request_.requested_video_device_id.empty()) {
        const blink::MediaStreamDevice* video_device =
            enumerator_->GetRequestedVideoDevice(
                request_.requested_video_device_id);
        if (video_device) {
          devices.push_back(*video_device);
          get_default_video_device = false;
        }
      }

      // If either or both audio and video devices were requested but not
      // specified by id, get the default devices.
      if (get_default_audio_device || get_default_video_device) {
        enumerator_->GetDefaultDevicesForBrowserContext(
            web_contents_->GetBrowserContext(), get_default_audio_device,
            get_default_video_device, &devices);
      }
      break;
    }
    case blink::MEDIA_DEVICE_ACCESS: {
      // Get the default devices for the request.
      enumerator_->GetDefaultDevicesForBrowserContext(
          web_contents_->GetBrowserContext(), audio_allowed, video_allowed,
          &devices);
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE: {
      NOTREACHED();
      break;
    }
  }  // switch

  return devices;
}

void MediaStreamDevicesController::RunCallback(bool blocked_by_feature_policy) {
  CHECK(callback_);

  MediaStreamDevices devices;
  // If all requested permissions are allowed then the callback should report
  // success, otherwise we report |denial_reason_|.
  blink::mojom::MediaStreamRequestResult request_result =
      blink::mojom::MediaStreamRequestResult::OK;
  if ((audio_setting_ == CONTENT_SETTING_ALLOW ||
       audio_setting_ == CONTENT_SETTING_DEFAULT) &&
      (video_setting_ == CONTENT_SETTING_ALLOW ||
       video_setting_ == CONTENT_SETTING_DEFAULT)) {
    devices = GetDevices(audio_setting_, video_setting_);
    if (devices.empty()) {
      // Even if all requested permissions are allowed, if there are no devices
      // at this point we still report a failure.
      request_result = blink::mojom::MediaStreamRequestResult::NO_HARDWARE;
    }
  } else {
    DCHECK_NE(blink::mojom::MediaStreamRequestResult::OK, denial_reason_);
    request_result = denial_reason_;
  }

  std::move(callback_).Run(devices, request_result, blocked_by_feature_policy,
                           audio_setting_, video_setting_);
}

ContentSetting MediaStreamDevicesController::GetContentSetting(
    ContentSettingsType content_type,
    const content::MediaStreamRequest& request,
    blink::mojom::MediaStreamRequestResult* denial_reason) const {
  DCHECK(content_type == ContentSettingsType::MEDIASTREAM_MIC ||
         content_type == ContentSettingsType::MEDIASTREAM_CAMERA);
  DCHECK(!request_.security_origin.is_empty());
  DCHECK(blink::network_utils::IsOriginSecure(request_.security_origin) ||
         request_.request_type == blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY);
  if (!ContentTypeIsRequested(content_type, request)) {
    // No denial reason set as it will have been previously set.
    return CONTENT_SETTING_DEFAULT;
  }

  std::string device_id;
  if (content_type == ContentSettingsType::MEDIASTREAM_MIC)
    device_id = request.requested_audio_device_id;
  else
    device_id = request.requested_video_device_id;
  if (!HasAvailableDevices(content_type, device_id)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::NO_HARDWARE;
    return CONTENT_SETTING_BLOCK;
  }

  if (!IsUserAcceptAllowed(content_type)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  // Don't request if the kill switch is on.
  if (PermissionIsBlockedForReason(
          content_type, permissions::PermissionStatusSource::KILL_SWITCH)) {
    *denial_reason = blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON;
    return CONTENT_SETTING_BLOCK;
  }

  return CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::IsUserAcceptAllowed(
    ContentSettingsType content_type) const {
#if defined(OS_ANDROID)
  ui::WindowAndroid* window_android =
      web_contents_->GetNativeView()->GetWindowAndroid();
  if (!window_android)
    return false;

  std::vector<std::string> android_permissions;
  permissions::GetAndroidPermissionsForContentSetting(content_type,
                                                      &android_permissions);
  for (const auto& android_permission : android_permissions) {
    if (!window_android->HasPermission(android_permission) &&
        !window_android->CanRequestPermission(android_permission)) {
      return false;
    }
  }

  // Don't approve device requests if the tab was hidden.
  // TODO(qinmin): Add a test for this. http://crbug.com/396869.
  // TODO(raymes): Shouldn't this apply to all permissions not just audio/video?
  return web_contents_->GetRenderWidgetHostView()->IsShowing();
#endif
  return true;
}

bool MediaStreamDevicesController::PermissionIsBlockedForReason(
    ContentSettingsType content_type,
    permissions::PermissionStatusSource reason) const {
  // TODO(raymes): This function wouldn't be needed if
  // PermissionManager::RequestPermissions returned a denial reason.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_.render_process_id, request_.render_frame_id);
  permissions::PermissionResult result =
      permissions::PermissionsClient::Get()
          ->GetPermissionManager(web_contents_->GetBrowserContext())
          ->GetPermissionStatusForFrame(content_type, rfh,
                                        request_.security_origin);
  if (result.source == reason) {
    DCHECK_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
    return true;
  }
  return false;
}

void MediaStreamDevicesController::PromptAnsweredGroupedRequest(
    const std::vector<ContentSetting>& responses) {
  bool need_audio = ShouldRequestAudio();
  bool need_video = ShouldRequestVideo();
  bool blocked_by_feature_policy = need_audio || need_video;
  // The audio setting will always be the first one in the vector, if it was
  // requested.
  if (need_audio) {
    audio_setting_ = responses.front();
    blocked_by_feature_policy &=
        audio_setting_ == CONTENT_SETTING_BLOCK &&
        PermissionIsBlockedForReason(
            ContentSettingsType::MEDIASTREAM_MIC,
            permissions::PermissionStatusSource::FEATURE_POLICY);
  }

  if (need_video) {
    video_setting_ = responses.at(need_audio ? 1 : 0);
    blocked_by_feature_policy &=
        video_setting_ == CONTENT_SETTING_BLOCK &&
        PermissionIsBlockedForReason(
            ContentSettingsType::MEDIASTREAM_CAMERA,
            permissions::PermissionStatusSource::FEATURE_POLICY);
  }

  for (ContentSetting response : responses) {
    if (response == CONTENT_SETTING_BLOCK)
      denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    else if (response == CONTENT_SETTING_ASK)
      denial_reason_ =
          blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED;
  }

  RunCallback(blocked_by_feature_policy);
}

bool MediaStreamDevicesController::HasAvailableDevices(
    ContentSettingsType content_type,
    const std::string& device_id) const {
  const MediaStreamDevices* devices = nullptr;
  if (content_type == ContentSettingsType::MEDIASTREAM_MIC) {
    devices = &enumerator_->GetAudioCaptureDevices();
  } else if (content_type == ContentSettingsType::MEDIASTREAM_CAMERA ||
             content_type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    devices = &enumerator_->GetVideoCaptureDevices();
  } else {
    NOTREACHED();
  }

  // TODO(tommi): It's kind of strange to have this here since if we fail this
  // test, there'll be a UI shown that indicates to the user that access to
  // non-existing audio/video devices has been denied.  The user won't have
  // any way to change that but there will be a UI shown which indicates that
  // access is blocked.
  if (devices->empty())
    return false;

  // If there are no particular device requirements, all devices will do.
  if (device_id.empty() &&
      content_type != ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    return true;
  }

  // Try to find a device which fulfils all device requirements.
  for (const blink::MediaStreamDevice& device : *devices) {
    if (!device_id.empty() && device.id != device_id) {
      continue;
    }
    if (content_type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM &&
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
