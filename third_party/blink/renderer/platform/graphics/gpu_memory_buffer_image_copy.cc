// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu_memory_buffer_image_copy.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

GpuMemoryBufferImageCopy::GpuMemoryBufferImageCopy(
    gpu::gles2::GLES2Interface* gl,
    gpu::SharedImageInterface* sii)
    : gl_(gl), sii_(sii) {}

GpuMemoryBufferImageCopy::~GpuMemoryBufferImageCopy() {
  CleanupDestImage();
}

bool GpuMemoryBufferImageCopy::EnsureDestImage(const gfx::Size& size) {
  // Create a new memorybuffer if the size has changed, or we don't have one.
  if (dest_image_size_ != size || !gpu_memory_buffer_) {
    // Cleanup old copy image before allocating a new one.
    CleanupDestImage();

    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
        Platform::Current()->GetGpuMemoryBufferManager();
    if (!gpu_memory_buffer_manager)
      return false;

    gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        size, gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::SCANOUT,
        gpu::kNullSurfaceHandle, nullptr);
    if (!gpu_memory_buffer_)
      return false;

    dest_image_size_ = size;

    dest_mailbox_ = sii_->CreateSharedImage(
        viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
        gpu::SHARED_IMAGE_USAGE_GLES2, "GpuMemoryBufferImageCopy",
        gpu_memory_buffer_->CloneHandle());
    gl_->WaitSyncTokenCHROMIUM(sii_->GenUnverifiedSyncToken().GetConstData());
  }
  return true;
}

std::pair<gfx::GpuMemoryBuffer*, gpu::SyncToken>
GpuMemoryBufferImageCopy::CopyImage(Image* image) {
  if (!image)
    return {};

  TRACE_EVENT0("gpu", "GpuMemoryBufferImageCopy::CopyImage");

  gfx::Size size = image->Size();
  if (!EnsureDestImage(size))
    return {};

  // Bind the write framebuffer to copy image.
  GLuint dest_texture_id =
      gl_->CreateAndTexStorage2DSharedImageCHROMIUM(dest_mailbox_.name);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      dest_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  GLenum target = GL_TEXTURE_2D;
  {
    gl_->BindTexture(target, dest_texture_id);
    gl_->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind the read framebuffer to our image.
  StaticBitmapImage* static_image = static_cast<StaticBitmapImage*>(image);
  auto source_mailbox_holder = static_image->GetMailboxHolder();
  DCHECK(source_mailbox_holder.mailbox.IsSharedImage());

  // Not strictly necessary since we are on the same context, but keeping
  // for cleanliness and in case we ever move off the same context.
  gl_->WaitSyncTokenCHROMIUM(source_mailbox_holder.sync_token.GetData());

  GLuint source_texture_id = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(
      source_mailbox_holder.mailbox.name);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      source_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  gl_->CopySubTextureCHROMIUM(source_texture_id, 0, GL_TEXTURE_2D,
                              dest_texture_id, 0, 0, 0, 0, 0, size.width(),
                              size.height(), false, false, false);

  // Cleanup the read framebuffer and texture.
  gl_->EndSharedImageAccessDirectCHROMIUM(source_texture_id);
  gl_->DeleteTextures(1, &source_texture_id);

  // Cleanup the draw framebuffer and texture.
  gl_->EndSharedImageAccessDirectCHROMIUM(dest_texture_id);
  gl_->DeleteTextures(1, &dest_texture_id);

  gpu::SyncToken sync_token;
  gl_->GenSyncTokenCHROMIUM(sync_token.GetData());

  static_image->UpdateSyncToken(sync_token);

  return std::make_pair(gpu_memory_buffer_.get(), sync_token);
}

void GpuMemoryBufferImageCopy::CleanupDestImage() {
  gpu_memory_buffer_.reset();

  if (dest_mailbox_.IsZero())
    return;

  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  sii_->DestroySharedImage(sync_token, dest_mailbox_);
  dest_mailbox_.SetZero();
}

}  // namespace blink
