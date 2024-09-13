// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/media_capture_util.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/media_capture_devices.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using blink::MediaStreamDevice;
using blink::MediaStreamDevices;
using content::MediaCaptureDevices;
using content::MediaStreamUI;
using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

// Return the first device from `requested_device_ids` that's found in the id
// list. If none of the ids are found then return the first device.
const MediaStreamDevice* GetRequestedDeviceOrDefault(
    const MediaStreamDevices& devices,
    const std::vector<std::string>& requested_device_ids) {
  for (const auto& requested_device_id : requested_device_ids) {
    if (requested_device_id.empty()) {
      continue;
    }

    auto it = base::ranges::find(devices, requested_device_id,
                                 &MediaStreamDevice::id);
    if (it != devices.end()) {
      return &(*it);
    }
  }

  if (!devices.empty()) {
    return &devices[0];
  }

  return nullptr;
}

}  // namespace

namespace media_capture_util {

// See also Chrome's MediaCaptureDevicesDispatcher.
void GrantMediaStreamRequest(content::WebContents* web_contents,
                             const content::MediaStreamRequest& request,
                             content::MediaResponseCallback callback,
                             const Extension* extension) {
  // app_shell only supports audio and video capture, not tab or screen capture.
  DCHECK(request.audio_type ==
             blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         request.video_type ==
             blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  // TOOD(crbug.com/1300883): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];

  if (request.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    VerifyMediaAccessPermission(request.audio_type, extension);
    const MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices(),
        request.requested_audio_device_ids);
    if (device) {
      devices.audio_device = *device;
    }
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    VerifyMediaAccessPermission(request.video_type, extension);
    const MediaStreamDevice* device = GetRequestedDeviceOrDefault(
        MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices(),
        request.requested_video_device_ids);
    if (device) {
      devices.video_device = *device;
    }
  }

  // TODO(jamescook): Should we show a recording icon somewhere? If so, where?
  std::unique_ptr<MediaStreamUI> ui;
  std::move(callback).Run(
      stream_devices_set,
      (devices.audio_device.has_value() || devices.video_device.has_value())
          ? blink::mojom::MediaStreamRequestResult::OK
          : blink::mojom::MediaStreamRequestResult::INVALID_STATE,
      std::move(ui));
}

void VerifyMediaAccessPermission(blink::mojom::MediaStreamType type,
                                 const Extension* extension) {
  const PermissionsData* permissions_data = extension->permissions_data();
  if (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    // app_shell has no UI surface to show an error, and on an embedded device
    // it's better to crash than to have a feature not work.
    CHECK(permissions_data->HasAPIPermission(APIPermissionID::kAudioCapture))
        << "Audio capture request but no audioCapture permission in manifest.";
  } else {
    DCHECK(type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
    CHECK(permissions_data->HasAPIPermission(APIPermissionID::kVideoCapture))
        << "Video capture request but no videoCapture permission in manifest.";
  }
}

}  // namespace media_capture_util
}  // namespace extensions
