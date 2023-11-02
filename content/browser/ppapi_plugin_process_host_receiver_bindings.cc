// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This exposes services in the browser to the PPAPI process.

#include "content/browser/ppapi_plugin_process_host.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#endif

namespace content {

void PpapiPluginProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (auto font_receiver = receiver.As<font_service::mojom::FontService>())
    ConnectToFontService(std::move(font_receiver));
#endif
}

}  // namespace content
