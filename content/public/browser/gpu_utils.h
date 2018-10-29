// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace content {

CONTENT_EXPORT const gpu::GpuPreferences GetGpuPreferencesFromCommandLine();

CONTENT_EXPORT void StopGpuProcess(const base::Closure& callback);

CONTENT_EXPORT gpu::GpuChannelEstablishFactory* GetGpuChannelEstablishFactory();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
