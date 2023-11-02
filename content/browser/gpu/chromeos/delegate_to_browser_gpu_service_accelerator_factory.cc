// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/chromeos/delegate_to_browser_gpu_service_accelerator_factory.h"

#include "content/browser/gpu/chromeos/video_capture_dependencies.h"

namespace content {

void DelegateToBrowserGpuServiceAcceleratorFactory::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  VideoCaptureDependencies::CreateJpegDecodeAccelerator(
      std::move(jda_receiver));
}

}  // namespace content
