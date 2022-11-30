// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the GPU process to the browser.

#include "content/gpu/gpu_child_thread.h"

#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "media/mojo/buildflags.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || !BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"  // nogncheck
#include "services/shape_detection/shape_detection_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "content/gpu/gpu_service_factory.h"
#include "media/mojo/mojom/media_service.mojom.h"
#endif

namespace content {

void GpuChildThread::BindServiceInterface(
    mojo::GenericPendingReceiver receiver) {
  if (auto viz_receiver = receiver.As<viz::mojom::VizMain>()) {
    // Note that unlike other interfaces, we want to allow VizMain to bind
    // early. It's required to unblock the rest of GPU initialization.
    viz_main_.Bind(std::move(viz_receiver));
    return;
  }

  if (!service_factory_) {
    pending_service_receivers_.push_back(std::move(receiver));
    return;
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || !BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto shape_detection_receiver =
          receiver.As<shape_detection::mojom::ShapeDetectionService>()) {
    static base::NoDestructor<shape_detection::ShapeDetectionService> service{
        std::move(shape_detection_receiver)};
    return;
  }
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  if (auto r = receiver.As<media::mojom::MediaService>()) {
    service_factory_->RunMediaService(std::move(r));
    return;
  }
#endif
}

}  // namespace content
