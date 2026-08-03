// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_PROCESSOR_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_PROCESSOR_WRAPPER_H_

#include <wrl.h>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d12_helpers.h"
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
  virtual ~D3D12VideoProcessorWrapper();

  // Initializes command queue, allocator and list. Returns whether the
  // initialization was successful.
  virtual bool Init();

  // Wait until the GPU has finished processing the previous frames. The fence
  // must not be null. Returns whether the wait was successful.
  virtual bool Wait(D3D12FenceAndValue fence_and_value);

  // CPU-wait until any work this wrapper has previously submitted to its
  // video process queue has completed on the GPU. Returns kOk when there is
  // nothing to wait on (e.g. after a failed Init() or before any
  // ProcessFrames() call). Used both internally before reusing the command
  // allocator and externally when downstream code errors out after
  // ProcessFrames() but before its own fence wait, so resources referenced by
  // the in-flight command list are not released while the GPU is still using
  // them.
  virtual D3D11Status WaitForInFlightWork();

  // Returns whether the D3D12 video processor can convert an input of
  // |input_format| in |input_color_space| to an output of |output_format| in
  // |output_color_space| for the given input size.
  virtual bool CheckVideoProcessorSupport(
      UINT input_width,
      UINT input_height,
      DXGI_FORMAT input_format,
      const gfx::ColorSpace& input_color_space,
      DXGI_FORMAT output_format,
      const gfx::ColorSpace& output_color_space);

  // Processes the |input_texture| and writes the result to |output_texture|.
  // Returns {nullptr, 0} on failure, otherwise returns a valid fence and value.
  virtual D3D12FenceAndValue ProcessFrames(
      ID3D12Resource* input_texture,
      UINT input_subresource,
      const gfx::ColorSpace& input_color_space,
      const gfx::Rect& input_rectangle,
      ID3D12Resource* output_texture,
      UINT output_subresource,
      const gfx::ColorSpace& output_color_space,
      const gfx::Rect& output_rectangle);

 private:
  D3D11Status WaitForInFlightWorkImpl();

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
