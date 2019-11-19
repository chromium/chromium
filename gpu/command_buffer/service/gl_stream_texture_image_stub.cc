// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_stream_texture_image_stub.h"

#include "ui/gfx/gpu_fence.h"

namespace gpu {
namespace gles2 {

// Overridden from GLImage:
gfx::Size GLStreamTextureImageStub::GetSize() {
  return gfx::Size();
}
unsigned GLStreamTextureImageStub::GetInternalFormat() {
  return 0;
}
unsigned GLStreamTextureImageStub::GetDataType() {
  return 0;
}
GLStreamTextureImageStub::BindOrCopy
GLStreamTextureImageStub::ShouldBindOrCopy() {
  return BIND;
}
bool GLStreamTextureImageStub::BindTexImage(unsigned target) {
  return false;
}
bool GLStreamTextureImageStub::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}
bool GLStreamTextureImageStub::CopyTexSubImage(unsigned target,
                                               const gfx::Point& offset,
                                               const gfx::Rect& rect) {
  return false;
}

bool GLStreamTextureImageStub::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

bool GLStreamTextureImageStub::EmulatingRGB() const {
  return false;
}

}  // namespace gles2
}  // namespace gpu
