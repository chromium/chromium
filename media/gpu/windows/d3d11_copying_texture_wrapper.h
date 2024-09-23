// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace media {

// Uses D3D11VideoProcessor to convert between an input texture2D and an output
// texture2D.  Each instance owns its own destination texture.
class MEDIA_GPU_EXPORT CopyingTexture2DWrapper : public Texture2DWrapper {
 public:
  // |output_wrapper| must wrap a Texture2D which is a single-entry Texture,
  // while |input_texture| may have multiple entries.
  CopyingTexture2DWrapper(const gfx::Size& size,
                          std::unique_ptr<Texture2DWrapper> output_wrapper,
                          scoped_refptr<VideoProcessorProxy> processor,
                          ComD3D11Texture2D output_texture);
  ~CopyingTexture2DWrapper() override;

  D3D11Status BeginSharedImageAccess() override;

  D3D11Status ProcessTexture(
      const gfx::ColorSpace& input_color_space,
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) override;

  D3D11Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                   GetCommandBufferHelperCB get_helper_cb,
                   ComD3D11Texture2D texture,
                   size_t array_slice,
                   scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
                   PictureBufferGPUResourceInitDoneCB
                       picture_buffer_gpu_resource_init_done_cb) override;

 private:
  gfx::Size size_;
  scoped_refptr<VideoProcessorProxy> video_processor_;
  std::unique_ptr<Texture2DWrapper> output_texture_wrapper_;
  ComD3D11Texture2D output_texture_;

  // If set, this is the color space that we last saw in ProcessTexture.
  std::optional<gfx::ColorSpace> previous_input_color_space_;

  ComD3D11Texture2D texture_;
  size_t array_slice_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
