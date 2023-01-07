// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_CHROMEOS_VIDEO_CAPTURE_DEPENDENCIES_H_
#define CONTENT_BROWSER_GPU_CHROMEOS_VIDEO_CAPTURE_DEPENDENCIES_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

// Browser-process-provided GPU dependencies for video capture.
class VideoCaptureDependencies {
 public:
  static void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          accelerator);
  static void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          accelerator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_CHROMEOS_VIDEO_CAPTURE_DEPENDENCIES_H_
