// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"

namespace media {

// Uses D3D11VideoProcessor to convert between an input texture2D and an output
// texture2D.  Each instance owns its own destination texture.
class MEDIA_GPU_EXPORT CopyingTexture2DWrapper : public Texture2DWrapper {
 public:
  // |output_wrapper| must wrap a Texture2D which is a single-entry Texture,
  // while |input_texture| may have multiple entries.  |output_color_space| is
  // the color space that we'll copy to, if specified.  If not, then we'll use
  // the input color space for a passthrough copy (e.g., NV12 => NV12 that will
  // be given to the swap chain directly, or video processed later).
  CopyingTexture2DWrapper(const gfx::Size& size,
                          std::unique_ptr<Texture2DWrapper> output_wrapper,
                          std::unique_ptr<VideoProcessorProxy> processor,
                          ComD3D11Texture2D output_texture,
                          base::Optional<gfx::ColorSpace> output_color_space);
  ~CopyingTexture2DWrapper() override;

  Status ProcessTexture(const gfx::ColorSpace& input_color_space,
                        MailboxHolderArray* mailbox_dest,
                        gfx::ColorSpace* output_color_space) override;

  Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
              GetCommandBufferHelperCB get_helper_cb,
              ComD3D11Texture2D texture,
              size_t array_slice) override;

  void SetStreamHDRMetadata(const gl::HDRMetadata& stream_metadata) override;
  void SetDisplayHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) override;

 private:
  gfx::Size size_;
  std::unique_ptr<VideoProcessorProxy> video_processor_;
  std::unique_ptr<Texture2DWrapper> output_texture_wrapper_;
  ComD3D11Texture2D output_texture_;
  // If set, then this is the desired output color space for the copy.
  base::Optional<gfx::ColorSpace> output_color_space_;

  // If set, this is the color space that we last saw in ProcessTexture.
  base::Optional<gfx::ColorSpace> previous_input_color_space_;

  ComD3D11Texture2D texture_;
  size_t array_slice_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
