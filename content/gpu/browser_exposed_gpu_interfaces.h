// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_
#define CONTENT_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}

namespace mojo {
class BinderMap;
}

namespace content {

// Populates a BinderMap with interfaces exposed by all Content embedders from
// the GPU process to the browser. The browser can bind these interfaces through
// |GpuProcessHost::BindReceiver()|.
//
// Embedder-specific GPU interfaces can be exposed to the browser via
// |ContentGpuClient::ExposeInterfacesToBrowser()| or embedder-specific helper
// functions.
void ExposeGpuInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders);

}  // namespace content

#endif  // CONTENT_GPU_BROWSER_EXPOSED_GPU_INTERFACES_H_
