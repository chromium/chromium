// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHROMEOS_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_CHROMEOS_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace content {

// Implementation of video_capture::mojom::AcceleratorFactor that satisfies
// requests for a JpegDecodeAccelerator by delegating to the global instance of
// viz::mojom::GpuService that is accessible from the Browser process.
class CONTENT_EXPORT DelegateToBrowserGpuServiceAcceleratorFactory
    : public video_capture::mojom::AcceleratorFactory {
 public:
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHROMEOS_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_
