// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_permission_checker.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

MediaDevicesManager::BoolDeviceTypes DoCheckPermissionsOnUIThread(
    MediaDevicesManager::BoolDeviceTypes requested_device_types,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);

  // If there is no |frame_host|, return false for all permissions.
  if (!frame_host)
    return MediaDevicesManager::BoolDeviceTypes();

  RenderFrameHostDelegate* delegate = frame_host->delegate();
  url::Origin origin = frame_host->GetLastCommittedOrigin();
  bool audio_permission = delegate->CheckMediaAccessPermission(
      frame_host, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool mic_feature_policy = true;
  bool camera_feature_policy = true;
  mic_feature_policy = frame_host->IsFeatureEnabled(
      blink::mojom::FeaturePolicyFeature::kMicrophone);
  camera_feature_policy =
      frame_host->IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kCamera);

  MediaDevicesManager::BoolDeviceTypes result;
  // Speakers.
  // TODO(guidou): use specific permission for audio output when it becomes
  // available. See http://crbug.com/556542.
  result[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] =
      requested_device_types[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] &&
      audio_permission;

  // Mic.
  result[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] =
      requested_device_types[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] &&
      audio_permission && mic_feature_policy;

  // Camera.
  result[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      requested_device_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] &&
      delegate->CheckMediaAccessPermission(
          frame_host, origin,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) &&
      camera_feature_policy;

  return result;
}

bool CheckSinglePermissionOnUIThread(blink::MediaDeviceType device_type,
                                     int render_process_id,
                                     int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MediaDevicesManager::BoolDeviceTypes requested;
  requested[device_type] = true;
  MediaDevicesManager::BoolDeviceTypes result = DoCheckPermissionsOnUIThread(
      requested, render_process_id, render_frame_id);
  return result[device_type];
}

}  // namespace

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker()
    : use_override_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)),
      override_value_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kUseFakeUIForMediaStream) != "deny") {}

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker(
    bool override_value)
    : use_override_(true), override_value_(override_value) {}

bool MediaDevicesPermissionChecker::CheckPermissionOnUIThread(
    blink::MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id) const {
  if (use_override_)
    return override_value_;

  return CheckSinglePermissionOnUIThread(device_type, render_process_id,
                                         render_frame_id);
}

void MediaDevicesPermissionChecker::CheckPermission(
    blink::MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> callback) const {
  if (use_override_) {
    std::move(callback).Run(override_value_);
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckSinglePermissionOnUIThread, device_type,
                     render_process_id, render_frame_id),
      std::move(callback));
}

void MediaDevicesPermissionChecker::CheckPermissions(
    MediaDevicesManager::BoolDeviceTypes requested,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(const MediaDevicesManager::BoolDeviceTypes&)>
        callback) const {
  if (use_override_) {
    MediaDevicesManager::BoolDeviceTypes result;
    result.fill(override_value_);
    std::move(callback).Run(result);
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DoCheckPermissionsOnUIThread, requested,
                     render_process_id, render_frame_id),
      std::move(callback));
}

}  // namespace content
