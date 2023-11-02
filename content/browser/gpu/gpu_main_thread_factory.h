// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_MAIN_THREAD_FACTORY_H_
#define CONTENT_BROWSER_GPU_GPU_MAIN_THREAD_FACTORY_H_

#include "content/common/content_export.h"

namespace base {
class Thread;
}

namespace gpu {
struct GpuPreferences;
}

namespace content {
class InProcessChildThreadParams;

typedef base::Thread* (*GpuMainThreadFactoryFunction)(
    const InProcessChildThreadParams&,
    const gpu::GpuPreferences&);

CONTENT_EXPORT void RegisterGpuMainThreadFactory(
    GpuMainThreadFactoryFunction create);

GpuMainThreadFactoryFunction GetGpuMainThreadFactory();

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_MAIN_THREAD_FACTORY_H_
