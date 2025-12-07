// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/shape_detection_service.h"

#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "content/public/browser/service_process_host_passkeys.h"
#include "services/shape_detection/shape_detection_library_holder.h"
#endif

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
                          (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)))
#include "content/public/browser/service_process_host.h"
#else
#include "content/browser/gpu/gpu_process_host.h"
#endif

namespace content {

shape_detection::mojom::ShapeDetectionService* GetShapeDetectionService() {
  static base::NoDestructor<
      mojo::Remote<shape_detection::mojom::ShapeDetectionService>>
      remote;
  if (!*remote) {
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
                          (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)))
    ServiceProcessHost::Launch<shape_detection::mojom::ShapeDetectionService>(
        remote->BindNewPipeAndPassReceiver(),
        ServiceProcessHost::Options()
            .WithDisplayName("Shape Detection Service")
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
            .WithPreloadedLibraries(
                {shape_detection::GetChromeShapeDetectionPath()},
                ServiceProcessHostPreloadLibraries::GetPassKey())
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
            .Pass());
#else
    auto* gpu = GpuProcessHost::Get();
    if (gpu) {
      gpu->RunService(remote->BindNewPipeAndPassReceiver());
    }
#endif
    remote->reset_on_disconnect();
  }

  return remote->get();
}

}  // namespace content
