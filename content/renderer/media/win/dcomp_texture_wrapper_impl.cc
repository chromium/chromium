// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/dcomp_texture_wrapper_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/base/win/mf_helpers.h"

namespace content {

// A RefCounted wrapper to destroy SharedImage associated with the `mailbox_`.
// We cannot destroy SharedImage in DCOMPTextureWrapperImpl destructor
// as it is possible for the VideoFrame to outlive the DCOMPTextureWrapperImpl
// instance.
// Both DCOMPTextureWrapperImpl and each bound callback to OnReleaseVideoFrame()
// hold outstanding reference of this class. When ref count is zero, the
// wrapped ShareImaged is destroyed within ~DCOMPTextureMailboxResources().
class DCOMPTextureMailboxResources
    : public base::RefCounted<DCOMPTextureMailboxResources> {
 public:
  DCOMPTextureMailboxResources(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      scoped_refptr<DCOMPTextureFactory> factory)
      : shared_image_(std::move(shared_image)), factory_(factory) {
    DVLOG_FUNC(1);
  }

  void SetSyncToken(const gpu::SyncToken& sync_token) {
    last_sync_token_ = sync_token;
  }

  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() {
    return shared_image_;
  }

  DCOMPTextureFactory* Factory() { return factory_.get(); }

 private:
  friend class base::RefCounted<DCOMPTextureMailboxResources>;

  ~DCOMPTextureMailboxResources() {
    DVLOG_FUNC(1);
    if (!last_sync_token_)
      return;

    gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
    sii->DestroySharedImage(last_sync_token_.value(), std::move(shared_image_));
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  scoped_refptr<DCOMPTextureFactory> factory_;

  // TODO(xhwang): Follow the example of UpdateReleaseSyncToken to wait for
  // all SyncTokens and create a new one for DestroySharedImage().
  std::optional<gpu::SyncToken> last_sync_token_;
};

namespace {

// Non-member function to allow it to run even after this class is deleted.
// Always called on the media task runner.
void OnReleaseVideoFrame(
    scoped_refptr<DCOMPTextureMailboxResources> dcomp_texture_resources,
    const gpu::SyncToken& sync_token) {
  DVLOG(1) << __func__;

  dcomp_texture_resources->SetSyncToken(sync_token);
  gpu::SharedImageInterface* sii =
      dcomp_texture_resources->Factory()->SharedImageInterface();
  sii->Flush();
}

}  // namespace

// static
std::unique_ptr<media::DCOMPTextureWrapper> DCOMPTextureWrapperImpl::Create(
    scoped_refptr<DCOMPTextureFactory> factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner);

  // This can happen if EstablishGpuChannelSync() failed previously.
  // See https://crbug.com/1378123.
  if (!factory)
    return nullptr;

  return base::WrapUnique(
      new DCOMPTextureWrapperImpl(factory, media_task_runner));
}

DCOMPTextureWrapperImpl::DCOMPTextureWrapperImpl(
    scoped_refptr<DCOMPTextureFactory> factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : factory_(factory), media_task_runner_(media_task_runner) {
  DVLOG_FUNC(1);
  DCHECK(factory_);
}

DCOMPTextureWrapperImpl::~DCOMPTextureWrapperImpl() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // We do not need any additional cleanup logic here as the
  // OnReleaseVideoFrame() callback handles cleaning up the shared image.
}

bool DCOMPTextureWrapperImpl::Initialize(
    const gfx::Size& output_size,
    OutputRectChangeCB output_rect_change_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  output_size_ = output_size;

  dcomp_texture_host_ = factory_->CreateDCOMPTextureHost(this);
  if (!dcomp_texture_host_)
    return false;

  output_rect_change_cb_ = std::move(output_rect_change_cb);
  return true;
}

void DCOMPTextureWrapperImpl::UpdateTextureSize(const gfx::Size& new_size) {
  DVLOG_FUNC(2);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // We would like to invoke SetTextureSize() which will let DCOMPTexture to
  // bind a mailbox to its SharedImage as early as possible. Let new_size of
  // gfx::Size(1, 1) to pass thru. as it is the initial |output_size_|.
  if (output_size_ == new_size && new_size != gfx::Size(1, 1))
    return;

  output_size_ = new_size;
  dcomp_texture_host_->SetTextureSize(new_size);
}

void DCOMPTextureWrapperImpl::SetDCOMPSurfaceHandle(
    const base::UnguessableToken& token,
    SetDCOMPSurfaceHandleCB set_dcomp_surface_handle_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  dcomp_texture_host_->SetDCOMPSurfaceHandle(
      token, std::move(set_dcomp_surface_handle_cb));
}

void DCOMPTextureWrapperImpl::CreateVideoFrame(
    const gfx::Size& natural_size,
    CreateVideoFrameCB create_video_frame_cb) {
  DVLOG_FUNC(2);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  natural_size_ = natural_size;
  if (mailbox_.IsZero()) {
    DVLOG_FUNC(1) << "mailbox_ not bound yet";
    create_video_frame_cb_ = std::move(create_video_frame_cb);
    return;
  }

  // No need to wait on any sync token as the SharedImage |mailbox_| should be
  // ready for use.
  if (!dcomp_texture_resources_) {
    DVLOG_FUNC(1) << "AddMailbox";
    gpu::SharedImageInterface* sii = factory_->SharedImageInterface();

    // The SI backing this VideoFrame will be read by the display compositor and
    // raster. The latter will be over GL if not using OOP-R. NOTE: GL usage can
    // be eliminated once OOP-R ships definitively.
    // TODO(crbug.com/40286368): Check the potential inconsistency between the
    // |usage| passed to NotifyMailboxAdded() here and the |usage| that
    // DCOMPTextureBacking's constructor uses to initialize
    // ClearTrackingSharedImageBacking.
    scoped_refptr<gpu::ClientSharedImage> shared_image;

    // Ensure that the ClientSI holds the correct texture target (which is *not*
    // the texture target that ClientSharedImage would compute internally for
    // these parameters).
    shared_image =
        sii->NotifyMailboxAdded(mailbox_, viz::SinglePlaneFormat::kBGRA_8888,
                                natural_size_, gfx::ColorSpace::CreateSRGB(),
                                kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                    gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                    gpu::SHARED_IMAGE_USAGE_RASTER_READ,
                                GL_TEXTURE_EXTERNAL_OES);

    CHECK(shared_image);
    dcomp_texture_resources_ =
        base::MakeRefCounted<DCOMPTextureMailboxResources>(
            std::move(shared_image), factory_);
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      dcomp_texture_resources_->GetSharedImage();

  auto frame = media::VideoFrame::WrapSharedImage(
      media::PIXEL_FORMAT_BGRA, shared_image, gpu::SyncToken(),
      base::BindPostTask(
          media_task_runner_,
          base::BindOnce(&OnReleaseVideoFrame, dcomp_texture_resources_)),
      natural_size_, gfx::Rect(natural_size_), natural_size_,
      base::TimeDelta());

  // Sets `dcomp_surface` to use StreamTexture. See `VideoResourceUpdater`.
  frame->metadata().dcomp_surface = true;

  std::move(create_video_frame_cb).Run(frame, mailbox_);
}

void DCOMPTextureWrapperImpl::CreateVideoFrame(
    const gfx::Size& natural_size,
    gfx::GpuMemoryBufferHandle dx_handle,
    CreateDXVideoFrameCB create_video_frame_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  gpu::SharedImageInterface* sii = factory_->SharedImageInterface();

  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                   gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
                                   gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
                                   gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto shared_image = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kBGRA_8888, natural_size, gfx::ColorSpace(),
       usage, "DCOMPTextureWrapperImpl"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
      std::move(dx_handle));
  CHECK(shared_image);

  gpu::Mailbox mailbox = shared_image->mailbox();
  gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();

  auto video_frame_texture = media::VideoFrame::WrapMappableSharedImage(
      shared_image, sync_token, base::NullCallback(), gfx::Rect(natural_size),
      natural_size, base::TimeDelta::Min());
  video_frame_texture->metadata().wants_promotion_hint = true;
  video_frame_texture->metadata().allow_overlay = true;

  video_frame_texture->AddDestructionObserver(base::BindPostTask(
      media_task_runner_,
      base::BindOnce(&DCOMPTextureWrapperImpl::OnDXVideoFrameDestruction,
                     weak_factory_.GetWeakPtr(), sync_token,
                     std::move(shared_image)),
      FROM_HERE));

  std::move(create_video_frame_cb).Run(video_frame_texture, mailbox);
}

void DCOMPTextureWrapperImpl::OnDXVideoFrameDestruction(
    const gpu::SyncToken& sync_token,
    scoped_refptr<gpu::ClientSharedImage> shared_image) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
  sii->DestroySharedImage(sync_token, std::move(shared_image));
}

void DCOMPTextureWrapperImpl::OnSharedImageMailboxBound(gpu::Mailbox mailbox) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  mailbox_ = std::move(mailbox);

  if (!create_video_frame_cb_.is_null()) {
    DVLOG_FUNC(3) << "Mailbox bound: CreateVideoFrame";
    CreateVideoFrame(natural_size_, std::move(create_video_frame_cb_));
  }
}

void DCOMPTextureWrapperImpl::OnOutputRectChange(gfx::Rect output_rect) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  output_rect_change_cb_.Run(output_rect);
}

}  // namespace content
