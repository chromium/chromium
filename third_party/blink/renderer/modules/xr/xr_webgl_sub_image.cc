// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_sub_image.h"

#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"

namespace blink {

XRWebGLSubImage::XRWebGLSubImage(const gfx::Rect& viewport,
                                 std::optional<uint16_t> image_index,
                                 XRWebGLSwapChain* color_swap_chain,
                                 XRWebGLSwapChain* depth_stencil_swap_chain,
                                 XRWebGLSwapChain* motion_vector_swap_chain)
    : XRSubImage(viewport), image_index_(image_index) {
  // Must have color swap chain, depth/stencil and motion vector are optional.
  CHECK(color_swap_chain);
  color_texture_ = color_swap_chain->GetCurrentTexture();
  color_texture_width_ = color_swap_chain->descriptor().width;
  color_texture_height_ = color_swap_chain->descriptor().height;

  if (depth_stencil_swap_chain) {
    depth_stencil_texture_ = depth_stencil_swap_chain->GetCurrentTexture();
    depth_stencil_texture_width_ = depth_stencil_swap_chain->descriptor().width;
    depth_stencil_texture_height_ =
        depth_stencil_swap_chain->descriptor().height;
  }

  if (motion_vector_swap_chain) {
    motion_vector_texture_ = motion_vector_swap_chain->GetCurrentTexture();
    motion_vector_texture_width_ = motion_vector_swap_chain->descriptor().width;
    motion_vector_texture_height_ =
        motion_vector_swap_chain->descriptor().height;
  }
}

void XRWebGLSubImage::Trace(Visitor* visitor) const {
  visitor->Trace(color_texture_);
  visitor->Trace(depth_stencil_texture_);
  visitor->Trace(motion_vector_texture_);
  XRSubImage::Trace(visitor);
}

}  // namespace blink
