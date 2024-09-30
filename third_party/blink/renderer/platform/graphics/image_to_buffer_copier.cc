// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/image_to_buffer_copier.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

ImageToBufferCopier::ImageToBufferCopier(
    gpu::gles2::GLES2Interface* gl,
    gpu::SharedImageInterface* sii)
    : gl_(gl), sii_(sii) {}

ImageToBufferCopier::~ImageToBufferCopier() {
  CleanupDestImage();
}

bool ImageToBufferCopier::EnsureDestImage(const gfx::Size& size) {
  // Create a new SharedImage if the size has changed, or we don't have one.
  if (dest_image_size_ != size || !dest_shared_image_) {
    // Cleanup old copy image before allocating a new one.
    CleanupDestImage();

    dest_image_size_ = size;

    // We copy the contents of the source image into the destination SharedImage
    // via GL, followed by giving out the destination SharedImage's native
    // buffer handle to eventually be read by the display compositor.
    dest_shared_image_ = sii_->CreateSharedImage(
        {viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_GLES2_WRITE, "ImageToBufferCopier"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT);
    CHECK(dest_shared_image_);
  }
  return true;
}

std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken>
ImageToBufferCopier::CopyImage(Image* image) {
  if (!image)
    return {};

  TRACE_EVENT0("gpu", "ImageToBufferCopier::CopyImage");

  gfx::Size size = image->Size();
  if (!EnsureDestImage(size))
    return {};

  // Bind the write framebuffer to copy image.
  auto dest_si_texture = dest_shared_image_->CreateGLTexture(gl_);
  auto dest_scoped_si_access =
      dest_si_texture->BeginAccess(gpu::SyncToken(), /*readonly=*/false);

  GLenum target = GL_TEXTURE_2D;
  {
    gl_->BindTexture(target, dest_scoped_si_access->texture_id());
    gl_->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Bind the read framebuffer to our image.
  StaticBitmapImage* static_image = static_cast<StaticBitmapImage*>(image);
  auto source_mailbox_holder = static_image->GetMailboxHolder();

  // Not strictly necessary since we are on the same context, but keeping
  // for cleanliness and in case we ever move off the same context.
  gl_->WaitSyncTokenCHROMIUM(source_mailbox_holder.sync_token.GetData());

  GLuint source_texture_id = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(
      source_mailbox_holder.mailbox.name);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      source_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  gl_->CopySubTextureCHROMIUM(
      source_texture_id, 0, GL_TEXTURE_2D, dest_scoped_si_access->texture_id(),
      0, 0, 0, 0, 0, size.width(), size.height(), false, false, false);

  // Cleanup the read framebuffer and texture.
  gl_->EndSharedImageAccessDirectCHROMIUM(source_texture_id);
  gl_->DeleteTextures(1, &source_texture_id);

  // Cleanup the draw framebuffer and texture.
  gpu::SyncToken sync_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(dest_scoped_si_access));
  dest_si_texture.reset();

  static_image->UpdateSyncToken(sync_token);

  return std::make_pair(dest_shared_image_
                            ? dest_shared_image_->CloneGpuMemoryBufferHandle()
                            : gfx::GpuMemoryBufferHandle(),
                        sync_token);
}

void ImageToBufferCopier::CleanupDestImage() {
  if (!dest_shared_image_) {
    return;
  }

  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  sii_->DestroySharedImage(sync_token, std::move(dest_shared_image_));
}

}  // namespace blink
