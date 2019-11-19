// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This exposes services in the browser to the PPAPI process.

#include "content/browser/ppapi_plugin_process_host.h"

#include "build/build_config.h"

#if defined(OS_LINUX)
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#endif

namespace content {

void PpapiPluginProcessHost::BindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
#if defined(OS_LINUX)
  if (auto font_receiver = receiver.As<font_service::mojom::FontService>())
    ConnectToFontService(std::move(font_receiver));
#endif
}

}  // namespace content
