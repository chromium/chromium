// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services in the browser to the utility process.

#include "build/build_config.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
#include "components/viz/host/gpu_client.h"
#include "content/public/browser/gpu_client.h"
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

namespace content {

void UtilityProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (auto font_receiver = receiver.As<font_service::mojom::FontService>()) {
    ConnectToFontService(std::move(font_receiver));
    return;
  }
#endif
#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  if (allowed_gpu_) {
    // TODO(crbug.com/328099369) Remove once all clients get this directly.
    if (auto gpu_receiver = receiver.As<viz::mojom::Gpu>()) {
      gpu_client_ = content::CreateGpuClient(std::move(gpu_receiver));
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  GetContentClient()->browser()->BindUtilityHostReceiver(std::move(receiver));
}

}  // namespace content
