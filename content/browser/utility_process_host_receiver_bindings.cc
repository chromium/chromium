// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services in the browser to the utility process.

#include "content/browser/utility_process_host.h"

#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
#include "components/viz/host/gpu_client.h"
#include "content/public/browser/gpu_client.h"
#endif

namespace content {

void UtilityProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (auto font_receiver = receiver.As<font_service::mojom::FontService>()) {
    ConnectToFontService(std::move(font_receiver));
    return;
  }
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
  if (allowed_gpu_) {
    // TODO(crbug.com/328099369) Remove once all clients get this directly.
    if (auto gpu_receiver = receiver.As<viz::mojom::Gpu>()) {
      gpu_client_ = content::CreateGpuClient(std::move(gpu_receiver));
      return;
    }
  }
#endif
  GetContentClient()->browser()->BindUtilityHostReceiver(std::move(receiver));
}

}  // namespace content
