// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_WEBGPU_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

namespace mediapipe {
class CalculatorGraph;
class Packet;
}  // namespace mediapipe

namespace video_effects {

class VideoEffectsGraphWebGpu {
 public:
  static std::unique_ptr<VideoEffectsGraphWebGpu> Create();

  VideoEffectsGraphWebGpu(const VideoEffectsGraphWebGpu& other) = delete;
  VideoEffectsGraphWebGpu& operator=(const VideoEffectsGraphWebGpu& other) =
      delete;

  VideoEffectsGraphWebGpu(VideoEffectsGraphWebGpu&& other) = delete;
  VideoEffectsGraphWebGpu& operator=(VideoEffectsGraphWebGpu&& other) = delete;

  ~VideoEffectsGraphWebGpu();

  bool Start(StaticConfig static_config,
             base::RepeatingCallback<void(wgpu::Texture)> on_frame_cb);
  bool ProcessFrame(base::TimeDelta timedelta,
                    wgpu::Texture input_texture,
                    wgpu::Texture output_texture,
                    const RuntimeConfig& runtime_config);
  bool WaitUntilIdle();

 private:
  explicit VideoEffectsGraphWebGpu(
      std::unique_ptr<mediapipe::CalculatorGraph> graph);

  void OnFrameProcessed(const mediapipe::Packet& packet);

  std::unique_ptr<mediapipe::CalculatorGraph> graph_;

  base::RepeatingCallback<void(wgpu::Texture)> on_frame_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsGraphWebGpu> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_VIDEO_EFFECTS_GRAPH_WEBGPU_H_
