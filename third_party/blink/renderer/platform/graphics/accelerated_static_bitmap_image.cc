// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
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
    IntSize mailbox_size,
    bool is_origin_top_left) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size, is_origin_top_left));
}

scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    PlatformThreadId context_thread_id,
    bool is_origin_top_left,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, shared_image_texture_id, sk_image_info,
      texture_target, std::move(context_provider_wrapper), context_thread_id,
      is_origin_top_left, std::move(release_callback)));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper)
    : paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  CHECK(image && image->isTextureBacked());
  skia_texture_holder_ = std::make_unique<SkiaTextureHolder>(
      std::move(image), std::move(context_provider_wrapper));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    IntSize mailbox_size,
    bool is_origin_top_left)
    : paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  mailbox_texture_holder_ = std::make_unique<MailboxTextureHolder>(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size, is_origin_top_left);
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    PlatformThreadId context_thread_id,
    bool is_origin_top_left,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : mailbox_ref_(base::MakeRefCounted<TextureHolder::MailboxRef>(
          std::move(release_callback))),
      paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  mailbox_texture_holder_ = std::make_unique<MailboxTextureHolder>(
      mailbox, sync_token, std::move(context_provider_wrapper), mailbox_ref_,
      context_thread_id, sk_image_info, texture_target, is_origin_top_left);
  if (shared_image_texture_id) {
    skia_texture_holder_ = std::make_unique<SkiaTextureHolder>(
        mailbox_texture_holder_.get(), shared_image_texture_id);
  }
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
  image.reset();
}

}  // namespace

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the original SkImage was retained, it must be destroyed on the thread
  // where it came from. In the same thread case, there is nothing to do because
  // the regular destruction flow is fine.
  if (original_skia_image_) {
    std::unique_ptr<gpu::SyncToken> sync_token =
        base::WrapUnique(new gpu::SyncToken(GetSyncToken()));
    if (!original_skia_image_task_runner_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *original_skia_image_task_runner_, FROM_HERE,
          CrossThreadBindOnce(
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(skia_texture_holder_);
  original_skia_image_ = skia_texture_holder_->GetSkImage();
  original_skia_image_context_provider_wrapper_ = ContextProviderWrapper();
  DCHECK(original_skia_image_);

  original_skia_image_task_runner_ = Thread::Current()->GetTaskRunner();
}

IntSize AcceleratedStaticBitmapImage::Size() const {
  return texture_holder()->Size();
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::MakeUnaccelerated() {
  CreateImageFromMailboxIfNeeded();
  return StaticBitmapImage::Create(
      skia_texture_holder_->GetSkImage()->makeNonTextureImage());
}

bool AcceleratedStaticBitmapImage::CopyToTexture(
    gpu::gles2::GLES2Interface* dest_gl,
    GLenum dest_target,
    GLuint dest_texture_id,
    GLint dest_level,
    bool unpack_premultiply_alpha,
    bool unpack_flip_y,
    const IntPoint& dest_point,
    const IntRect& source_sub_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return false;

  // TODO(junov) : could reduce overhead by using kOrderingBarrier when we know
  // that the source and destination context or on the same stream.
  EnsureMailbox(kUnverifiedSyncToken, GL_NEAREST);

  // This method should only be used for cross-context copying, otherwise it's
  // wasting overhead.
  DCHECK(mailbox_texture_holder_->IsCrossThread() ||
         dest_gl != ContextProviderWrapper()->ContextProvider()->ContextGL());

  bool is_shared_image = mailbox_texture_holder_->GetMailbox().IsSharedImage();

  // Get a texture id that |destProvider| knows about and copy from it.
  dest_gl->WaitSyncTokenCHROMIUM(
      mailbox_texture_holder_->GetSyncToken().GetConstData());
  GLuint source_texture_id;
  if (is_shared_image) {
    source_texture_id = dest_gl->CreateAndTexStorage2DSharedImageCHROMIUM(
        mailbox_texture_holder_->GetMailbox().name);
    dest_gl->BeginSharedImageAccessDirectCHROMIUM(
        source_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  } else {
    source_texture_id = dest_gl->CreateAndConsumeTextureCHROMIUM(
        mailbox_texture_holder_->GetMailbox().name);
  }
  dest_gl->CopySubTextureCHROMIUM(
      source_texture_id, 0, dest_target, dest_texture_id, dest_level,
      dest_point.X(), dest_point.Y(), source_sub_rectangle.X(),
      source_sub_rectangle.Y(), source_sub_rectangle.Width(),
      source_sub_rectangle.Height(), unpack_flip_y ? GL_FALSE : GL_TRUE,
      GL_FALSE, unpack_premultiply_alpha ? GL_FALSE : GL_TRUE);
  if (is_shared_image) {
    dest_gl->EndSharedImageAccessDirectCHROMIUM(source_texture_id);
  }
  // This drops the |destGL| context's reference on our |m_mailbox|, but it's
  // still held alive by our SkImage.
  dest_gl->DeleteTextures(1, &source_texture_id);

  // We need to update the texture holder's sync token to ensure that when this
  // image is deleted, the texture resource will not be recycled by skia before
  // the above texture copy has completed.
  gpu::SyncToken sync_token;
  dest_gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  mailbox_texture_holder_->UpdateSyncToken(sync_token);

  return true;
}

PaintImage AcceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return PaintImage();

  sk_sp<SkImage> image;
  if (original_skia_image_ &&
      original_skia_image_task_runner_->BelongsToCurrentThread()) {
    // We need to avoid consuming the mailbox in the context where it
    // originated.  This avoids swapping back and forth between TextureHolder
    // types.
    image = original_skia_image_;
  } else {
    CreateImageFromMailboxIfNeeded();
    image = skia_texture_holder_->GetSkImage();
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  return texture_holder()->IsValid();
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  return texture_holder()->ContextProvider();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
AcceleratedStaticBitmapImage::ContextProviderWrapper() const {
  if (!IsValid())
    return nullptr;

  return texture_holder()->ContextProviderWrapper();
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (skia_texture_holder_)
    return;

  DCHECK(mailbox_texture_holder_);
  skia_texture_holder_ =
      std::make_unique<SkiaTextureHolder>(mailbox_texture_holder_.get(), 0u);
}

void AcceleratedStaticBitmapImage::EnsureMailbox(MailboxSyncMode mode,
                                                 GLenum filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!mailbox_texture_holder_) {
    TRACE_EVENT0("blink", "AcceleratedStaticBitmapImage::EnsureMailbox");

    if (!original_skia_image_) {
      // To ensure that the texture resource stays alive we only really need
      // to retain the source SkImage until the mailbox is consumed, but this
      // works too.
      RetainOriginalSkImage();
    }

    mailbox_texture_holder_ = std::make_unique<MailboxTextureHolder>(
        skia_texture_holder_.get(), filter);
  }
  mailbox_texture_holder_->Sync(mode);
}

void AcceleratedStaticBitmapImage::Transfer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureMailbox(kVerifiedSyncToken, GL_NEAREST);

  // Release the SkiaTextureHolder, this SkImage is no longer valid to use
  // cross-thread.
  skia_texture_holder_.reset();

  DETACH_FROM_THREAD(thread_checker_);
}

bool AcceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
  return texture_holder()->CurrentFrameKnownToBeOpaque();
}

}  // namespace blink
