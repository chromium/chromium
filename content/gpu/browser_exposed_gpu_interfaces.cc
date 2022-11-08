// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/browser_exposed_gpu_interfaces.h"

#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/gpu/content_gpu_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

void ExposeGpuInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
  if (GetContentClient()->gpu()) {  // May be null in tests.
    GetContentClient()->gpu()->ExposeInterfacesToBrowser(
        gpu_preferences, gpu_workarounds, binders);
  }

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::GetInstance()->AddInterfaces(binders);
#endif
}

}  // namespace content
