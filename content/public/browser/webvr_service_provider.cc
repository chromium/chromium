// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webvr_service_provider.h"

#include "base/lazy_instance.h"

namespace content {

base::LazyInstance<WebvrServiceProvider::BindWebvrServiceCallback>::Leaky
    g_callback = LAZY_INSTANCE_INITIALIZER;

void WebvrServiceProvider::BindWebvrService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<device::mojom::VRService> receiver) {
  // Ignore the pending receiver if the callback is unset.
  if (g_callback.Get().is_null())
    return;
  g_callback.Get().Run(render_frame_host, std::move(receiver));
}

void WebvrServiceProvider::SetWebvrServiceCallback(
    BindWebvrServiceCallback callback) {
  g_callback.Get() = std::move(callback);
}

}  // namespace content
