// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SCOPED_ACCESS_PERMISSION_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_SCOPED_ACCESS_PERMISSION_MEDIA_TO_MOJO_ADAPTER_H_

#include "media/capture/video/video_capture_device_client.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"

namespace video_capture {

class ScopedAccessPermissionMediaToMojoAdapter
    : public video_capture::mojom::ScopedAccessPermission {
 public:
  explicit ScopedAccessPermissionMediaToMojoAdapter(
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          access_permission);
  ~ScopedAccessPermissionMediaToMojoAdapter() override;

 private:
  std::unique_ptr<
      media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
      access_permission_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SCOPED_ACCESS_PERMISSION_MEDIA_TO_MOJO_ADAPTER_H_
