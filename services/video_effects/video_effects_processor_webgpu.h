// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_
#define SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace viz {
class ContextProviderCommandBuffer;
}

namespace video_effects {

// Companion to `VideoEffectsProcessorImpl`. Initializes the WebGPU API that'll
// subsequently be used for post-processing the incoming video frames.
class VideoEffectsProcessorWebGpu {
 public:
  explicit VideoEffectsProcessorWebGpu(
      wgpu::Device device,
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      scoped_refptr<viz::RasterContextProvider>
          raster_interface_context_provider,
      scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface);
  ~VideoEffectsProcessorWebGpu();

  VideoEffectsProcessorWebGpu(const VideoEffectsProcessorWebGpu& other) =
      delete;
  VideoEffectsProcessorWebGpu& operator=(
      const VideoEffectsProcessorWebGpu& other) = delete;

  // Returns `true` if the synchronous part of initialization has completed.
  // This does not mean that the processor is fully initialized - WebGPU
  // initialization may still proceed asynchronously.
  bool Initialize();

  void PostProcess(
      media::mojom::VideoBufferHandlePtr input_frame_data,
      media::mojom::VideoFrameInfoPtr input_frame_info,
      media::mojom::VideoBufferHandlePtr result_frame_data,
      media::VideoPixelFormat result_pixel_format,
      mojom::VideoEffectsProcessor::PostProcessCallback post_process_cb);

 private:
  // Ensures that awaiting WebGPUInterface commands are flushed.
  void EnsureFlush();

  void QueryDone(
      GLuint query_id,
      uint64_t trace_id,
      media::mojom::VideoBufferHandlePtr input_frame_data,
      media::mojom::VideoFrameInfoPtr input_frame_info,
      media::mojom::VideoBufferHandlePtr result_frame_data,
      media::VideoPixelFormat result_pixel_format,
      mojom::VideoEffectsProcessor::PostProcessCallback post_process_cb);

  wgpu::ComputePipeline CreateComputePipeline();

  wgpu::Device device_;
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  scoped_refptr<viz::RasterContextProvider> raster_interface_context_provider_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  // Compute pipeline executing basic compute shader on a video frame.
  wgpu::ComputePipeline compute_pipeline_;

  // WebGPU buffer that we use to send the parameters to our compute shader.
  wgpu::Buffer uniforms_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoEffectsProcessorWebGpu> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIDEO_EFFECTS_PROCESSOR_WEBGPU_H_
