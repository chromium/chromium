// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the GPU process to the browser.

#include "content/gpu/gpu_child_thread.h"

#include "base/no_destructor.h"
#include "media/mojo/buildflags.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"
#include "services/shape_detection/shape_detection_service.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "content/gpu/gpu_service_factory.h"
#include "media/mojo/mojom/media_service.mojom.h"
#endif

namespace content {

void GpuChildThread::BindServiceInterface(
    mojo::GenericPendingReceiver receiver) {
  if (!service_factory_) {
    pending_service_receivers_.push_back(std::move(receiver));
    return;
  }

  if (auto shape_detection_receiver =
          receiver.As<shape_detection::mojom::ShapeDetectionService>()) {
    static base::NoDestructor<shape_detection::ShapeDetectionService> service{
        std::move(shape_detection_receiver)};
    return;
  }

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  if (auto r = receiver.As<media::mojom::MediaService>()) {
    service_factory_->RunMediaService(std::move(r));
    return;
  }
#endif
}

}  // namespace content
