// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/dcomp_texture_wrapper_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/base/bind_to_current_loop.h"
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
  DCOMPTextureMailboxResources(gpu::Mailbox mailbox,
                               scoped_refptr<DCOMPTextureFactory> factory)
      : mailbox_(mailbox), factory_(factory) {
    DVLOG_FUNC(1);
  }

  void SetSyncToken(const gpu::SyncToken& sync_token) {
    last_sync_token_ = sync_token;
  }

  DCOMPTextureFactory* Factory() { return factory_.get(); }

 private:
  friend class base::RefCounted<DCOMPTextureMailboxResources>;

  ~DCOMPTextureMailboxResources() {
    DVLOG_FUNC(1);
    if (!last_sync_token_)
      return;

    gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
    sii->DestroySharedImage(last_sync_token_.value(), mailbox_);
  }

  gpu::Mailbox mailbox_;
  scoped_refptr<DCOMPTextureFactory> factory_;

  // TODO(xhwang): Follow the example of UpdateReleaseSyncToken to wait for
  // all SyncTokens and create a new one for DestroySharedImage().
  absl::optional<gpu::SyncToken> last_sync_token_;
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
  if (!mailbox_added_) {
    DVLOG_FUNC(1) << "AddMailbox";
    mailbox_added_ = true;
    gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
    sii->NotifyMailboxAdded(mailbox_, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                          gpu::SHARED_IMAGE_USAGE_GLES2 |
                                          gpu::SHARED_IMAGE_USAGE_RASTER);
  }

  gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
      gpu::MailboxHolder(mailbox_, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES)};

  if (!dcomp_texture_resources_) {
    dcomp_texture_resources_ =
        base::MakeRefCounted<DCOMPTextureMailboxResources>(mailbox_, factory_);
  }

  auto frame = media::VideoFrame::WrapNativeTextures(
      media::PIXEL_FORMAT_ARGB, holders,
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

  uint32_t usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                   gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
                   gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      gpu::GpuMemoryBufferImplDXGI::CreateFromHandle(
          std::move(dx_handle), natural_size, gfx::BufferFormat::RGBA_8888,
          gfx::BufferUsage::GPU_READ, base::NullCallback(), nullptr, nullptr);

  // The VideoFrame object requires a 4 array mailbox holder because some
  // formats can have 4 separate planes that can have 4 different GPU
  // memories and even though in our case we are using only the first plane we
  // still need to provide the video frame creation with a 4 array mailbox
  // holder.
  gpu::MailboxHolder holder[media::VideoFrame::kMaxPlanes];
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      gmb.get(), nullptr, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, usage);
  gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();
  holder[0] = gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D);

  scoped_refptr<media::VideoFrame> video_frame_texture =
      media::VideoFrame::WrapExternalGpuMemoryBuffer(
          gfx::Rect(natural_size), natural_size, std::move(gmb), holder,
          base::NullCallback(), base::TimeDelta::Min());
  video_frame_texture->metadata().wants_promotion_hint = true;
  video_frame_texture->metadata().allow_overlay = true;

  video_frame_texture->AddDestructionObserver(base::BindPostTask(
      media_task_runner_,
      base::BindOnce(&DCOMPTextureWrapperImpl::OnDXVideoFrameDestruction,
                     weak_factory_.GetWeakPtr(), sync_token, mailbox),
      FROM_HERE));

  std::move(create_video_frame_cb).Run(video_frame_texture);
}

void DCOMPTextureWrapperImpl::OnDXVideoFrameDestruction(
    const gpu::SyncToken& sync_token,
    const gpu::Mailbox& image_mailbox) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  gpu::SharedImageInterface* sii = factory_->SharedImageInterface();
  sii->DestroySharedImage(sync_token, image_mailbox);
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
