// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_PROCESSOR_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_PROCESSOR_WRAPPER_H_

#include <wrl.h>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Wraps a D3D12 video processor and its related video processor input and
// output view resources.
class MEDIA_GPU_EXPORT D3D12VideoProcessorWrapper {
 public:
  explicit D3D12VideoProcessorWrapper(ComD3D12VideoDevice video_device);
  D3D12VideoProcessorWrapper(const D3D12VideoProcessorWrapper&) = delete;
  ~D3D12VideoProcessorWrapper();

  // Initializes command queue, allocator and list. Returns whether the
  // initialization was successful.
  bool Init();

  // Processes the |input_texture| and writes the result to |output_texture|.
  // Returns whether the processing was successful.
  bool ProcessFrames(ID3D12Resource* input_texture,
                     UINT input_subresource,
                     const gfx::ColorSpace& input_color_space,
                     const gfx::Rect& input_rectangle,
                     ID3D12Resource* output_texture,
                     UINT output_subresource,
                     const gfx::ColorSpace& output_color_space,
                     const gfx::Rect& output_rectangle);

 private:
  ComD3D12Device device_;
  ComD3D12VideoDevice video_device_;
  D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC input_stream_desc_{};
  D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC output_stream_desc_{};
  ComD3D12VideoProcessor video_processor_;
  ComD3D12CommandQueue command_queue_;
  ComD3D12CommandAllocator command_allocator_;
  ComD3D12VideoProcessCommandList command_list_;
  scoped_refptr<D3D12Fence> fence_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_PROCESSOR_WRAPPER_H_
