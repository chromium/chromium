// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
#define CONTENT_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_

#include "base/memory/weak_ptr.h"

namespace mojo {
class BinderMap;
}

namespace content {

class RenderThreadImpl;

// Populates |*binders| with callbacks to expose interfaces from every renderer
// process to the browser process. These interfaces are scoped to the entire
// render process rather than to a specific (e.g. frame) context and can be
// acquired by the browser through |RenderProcessHost::BindReceiver()|.
void ExposeRendererInterfacesToBrowser(
    base::WeakPtr<RenderThreadImpl> render_thread,
    mojo::BinderMap* binders);

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
