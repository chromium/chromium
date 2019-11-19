// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_

#include <memory>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"

namespace media {

// Uses D3D11VideoProcessor to convert between an input texture2D and an output
// texture2D.
class MEDIA_GPU_EXPORT CopyingTexture2DWrapper : public Texture2DWrapper {
 public:
  // |output_wrapper| must wrap a Texture2D which is a single-entry Texture,
  // while |input_texture| may have multiple entries.
  CopyingTexture2DWrapper(std::unique_ptr<Texture2DWrapper> output_wrapper,
                          std::unique_ptr<VideoProcessorProxy> processor,
                          ComD3D11Texture2D input_texture);
  ~CopyingTexture2DWrapper() override;

  bool ProcessTexture(const D3D11PictureBuffer* owner_pb,
                      MailboxHolderArray* mailbox_dest) override;

  bool Init(GetCommandBufferHelperCB get_helper_cb,
            size_t array_slice,
            gfx::Size size) override;

 private:
  std::unique_ptr<VideoProcessorProxy> video_processor_;
  std::unique_ptr<Texture2DWrapper> output_texture_wrapper_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COPYING_TEXTURE_WRAPPER_H_
