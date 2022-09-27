// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_video_frame_submitter.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

namespace cc {
class LayerTreeSettings;
class VideoFrameProvider;
}  // namespace cc

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
}

namespace blink {

std::unique_ptr<WebVideoFrameSubmitter> WebVideoFrameSubmitter::Create(
    WebContextProviderCallback context_provider_callback,
    cc::VideoPlaybackRoughnessReporter::ReportingCallback
        roughness_reporting_callback,
    const cc::LayerTreeSettings& settings,
    bool use_sync_primitives) {
  return std::make_unique<VideoFrameSubmitter>(
      std::move(context_provider_callback),
      std::move(roughness_reporting_callback),
      std::make_unique<VideoFrameResourceProvider>(settings,
                                                   use_sync_primitives));
}

}  // namespace blink
