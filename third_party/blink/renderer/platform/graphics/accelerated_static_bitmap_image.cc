// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrTexture.h"

#include <memory>
#include <utility>

namespace blink {

scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromSkImage(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  CHECK(image && image->isTextureBacked());
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(image), std::move(context_provider_wrapper)));
}

scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromWebGLContextImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    IntSize mailbox_size) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper)
    : paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  CHECK(image && image->isTextureBacked());
  texture_holder_ = std::make_unique<SkiaTextureHolder>(
      std::move(image), std::move(context_provider_wrapper));
  thread_checker_.DetachFromThread();
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    IntSize mailbox_size)
    : paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  texture_holder_ = std::make_unique<MailboxTextureHolder>(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size);
  thread_checker_.DetachFromThread();
}

namespace {

void DestroySkImageOnOriginalThread(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    std::unique_ptr<gpu::SyncToken> sync_token) {
  if (context_provider_wrapper &&
      image->isValid(
          context_provider_wrapper->ContextProvider()->GetGrContext())) {
    if (sync_token->HasData()) {
      // To make sure skia does not recycle the texture while it is still in use
      // by another context.
      context_provider_wrapper->ContextProvider()
          ->ContextGL()
          ->WaitSyncTokenCHROMIUM(sync_token->GetData());
    }
    // In case texture was used by compositor, which may have changed params.
    image->getTexture()->textureParamsModified();
  }
  // destroy by letting |image| go out of scope
}

}  // unnamed namespace

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  // If the original SkImage was retained, it must be destroyed on the thread
  // where it came from. In the same thread case, there is nothing to do because
  // the regular destruction flow is fine.
  if (original_skia_image_) {
    std::unique_ptr<gpu::SyncToken> sync_token =
        base::WrapUnique(new gpu::SyncToken(texture_holder_->GetSyncToken()));
    if (original_skia_image_thread_id_ !=
        Platform::Current()->CurrentThread()->ThreadId()) {
      PostCrossThreadTask(
          *original_skia_image_task_runner_, FROM_HERE,
          CrossThreadBind(
              &DestroySkImageOnOriginalThread, std::move(original_skia_image_),
              std::move(original_skia_image_context_provider_wrapper_),
              WTF::Passed(std::move(sync_token))));
    } else {
      DestroySkImageOnOriginalThread(
          std::move(original_skia_image_),
          std::move(original_skia_image_context_provider_wrapper_),
          std::move(sync_token));
    }
  }
}

void AcceleratedStaticBitmapImage::RetainOriginalSkImage() {
  DCHECK(texture_holder_->IsSkiaTextureHolder());
  original_skia_image_ = texture_holder_->GetSkImage();
  original_skia_image_context_provider_wrapper_ = ContextProviderWrapper();
  DCHECK(original_skia_image_);
  Thread* thread = Platform::Current()->CurrentThread();
  original_skia_image_thread_id_ = thread->ThreadId();
  original_skia_image_task_runner_ = thread->GetTaskRunner();
}

IntSize AcceleratedStaticBitmapImage::Size() const {
  return texture_holder_->Size();
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::MakeUnaccelerated() {
  CreateImageFromMailboxIfNeeded();
  return StaticBitmapImage::Create(
      texture_holder_->GetSkImage()->makeNonTextureImage());
}

void AcceleratedStaticBitmapImage::UpdateSyncToken(gpu::SyncToken sync_token) {
  texture_holder_->UpdateSyncToken(sync_token);
}

bool AcceleratedStaticBitmapImage::CopyToTexture(
    gpu::gles2::GLES2Interface* dest_gl,
    GLenum dest_target,
    GLuint dest_texture_id,
    bool unpack_premultiply_alpha,
    bool unpack_flip_y,
    const IntPoint& dest_point,
    const IntRect& source_sub_rectangle) {
  CheckThread();
  if (!IsValid())
    return false;
  // This method should only be used for cross-context copying, otherwise it's
  // wasting overhead.
  DCHECK(texture_holder_->IsCrossThread() ||
         dest_gl != ContextProviderWrapper()->ContextProvider()->ContextGL());

  // TODO(junov) : could reduce overhead by using kOrderingBarrier when we know
  // that the source and destination context or on the same stream.
  EnsureMailbox(kUnverifiedSyncToken, GL_NEAREST);

  // Get a texture id that |destProvider| knows about and copy from it.
  dest_gl->WaitSyncTokenCHROMIUM(
      texture_holder_->GetSyncToken().GetConstData());
  GLuint source_texture_id = dest_gl->CreateAndConsumeTextureCHROMIUM(
      texture_holder_->GetMailbox().name);
  dest_gl->CopySubTextureCHROMIUM(
      source_texture_id, 0, dest_target, dest_texture_id, 0, dest_point.X(),
      dest_point.Y(), source_sub_rectangle.X(), source_sub_rectangle.Y(),
      source_sub_rectangle.Width(), source_sub_rectangle.Height(),
      unpack_flip_y ? GL_FALSE : GL_TRUE, GL_FALSE,
      unpack_premultiply_alpha ? GL_FALSE : GL_TRUE);
  // This drops the |destGL| context's reference on our |m_mailbox|, but it's
  // still held alive by our SkImage.
  dest_gl->DeleteTextures(1, &source_texture_id);

  // We need to update the texture holder's sync token to ensure that when this
  // image is deleted, the texture resource will not be recycled by skia before
  // the above texture copy has completed.
  gpu::SyncToken sync_token;
  dest_gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  texture_holder_->UpdateSyncToken(sync_token);

  return true;
}

PaintImage AcceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  CheckThread();
  if (!IsValid())
    return PaintImage();

  sk_sp<SkImage> image;
  if (original_skia_image_ &&
      original_skia_image_thread_id_ ==
          Platform::Current()->CurrentThread()->ThreadId()) {
    // We need to avoid consuming the mailbox in the context where it
    // originated.  This avoids swapping back and forth between TextureHolder
    // types.
    image = original_skia_image_;
  } else {
    CreateImageFromMailboxIfNeeded();
    image = texture_holder_->GetSkImage();
  }

  return CreatePaintImageBuilder()
      .set_image(image, paint_image_content_id_)
      .set_completion_state(PaintImage::CompletionState::DONE)
      .TakePaintImage();
}

void AcceleratedStaticBitmapImage::Draw(cc::PaintCanvas* canvas,
                                        const cc::PaintFlags& flags,
                                        const FloatRect& dst_rect,
                                        const FloatRect& src_rect,
                                        RespectImageOrientationEnum,
                                        ImageClampingMode image_clamping_mode,
                                        ImageDecodingMode decode_mode) {
  auto paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return;
  auto paint_image_decoding_mode = ToPaintImageDecodingMode(decode_mode);
  if (paint_image.decoding_mode() != paint_image_decoding_mode) {
    paint_image = PaintImageBuilder::WithCopy(std::move(paint_image))
                      .set_decoding_mode(paint_image_decoding_mode)
                      .TakePaintImage();
  }
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect,
                                image_clamping_mode, paint_image);
}

bool AcceleratedStaticBitmapImage::IsValid() const {
  return texture_holder_ && texture_holder_->IsValid();
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  if (!IsValid())
    return nullptr;
  return texture_holder_->ContextProvider();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
AcceleratedStaticBitmapImage::ContextProviderWrapper() const {
  if (!IsValid())
    return nullptr;
  return texture_holder_->ContextProviderWrapper();
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  if (texture_holder_->IsSkiaTextureHolder())
    return;
  texture_holder_ =
      std::make_unique<SkiaTextureHolder>(std::move(texture_holder_));
}

void AcceleratedStaticBitmapImage::EnsureMailbox(MailboxSyncMode mode,
                                                 GLenum filter) {
  if (!texture_holder_->IsMailboxTextureHolder()) {
    TRACE_EVENT0("blink", "AcceleratedStaticBitmapImage::EnsureMailbox");

    if (!original_skia_image_) {
      // To ensure that the texture resource stays alive we only really need
      // to retain the source SkImage until the mailbox is consumed, but this
      // works too.
      RetainOriginalSkImage();
    }

    texture_holder_ = std::make_unique<MailboxTextureHolder>(
        std::move(texture_holder_), filter);
  }
  texture_holder_->Sync(mode);
}

void AcceleratedStaticBitmapImage::Transfer() {
  CheckThread();
  EnsureMailbox(kVerifiedSyncToken, GL_NEAREST);
  detach_thread_at_next_check_ = true;
}

bool AcceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
  return texture_holder_->CurrentFrameKnownToBeOpaque();
}

void AcceleratedStaticBitmapImage::CheckThread() {
  if (detach_thread_at_next_check_) {
    thread_checker_.DetachFromThread();
    detach_thread_at_next_check_ = false;
  }
  CHECK(thread_checker_.CalledOnValidThread());
}

void AcceleratedStaticBitmapImage::Abandon() {
  texture_holder_->Abandon();
}

}  // namespace blink
