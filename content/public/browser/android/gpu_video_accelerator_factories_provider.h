// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_GPU_VIDEO_ACCELERATOR_FACTORIES_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_GPU_VIDEO_ACCELERATOR_FACTORIES_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

using GpuVideoAcceleratorFactoriesCallback = base::OnceCallback<void(
    std::unique_ptr<media::GpuVideoAcceleratorFactories>)>;

// Provides hardware video decoding contexts in the browser process.
CONTENT_EXPORT
void CreateGpuVideoAcceleratorFactories(
    GpuVideoAcceleratorFactoriesCallback callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_GPU_VIDEO_ACCELERATOR_FACTORIES_PROVIDER_H_
