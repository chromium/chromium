// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/dcomp_texture_wrapper_impl.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
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
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  DVLOG(1) << __func__;
  // auto* impl = new DCOMPTextureWrapperImpl(factory, media_task_runner);
  return base::WrapUnique(
      new DCOMPTextureWrapperImpl(factory, media_task_runner));
}

DCOMPTextureWrapperImpl::DCOMPTextureWrapperImpl(
    scoped_refptr<DCOMPTextureFactory> factory,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : factory_(factory), media_task_runner_(media_task_runner) {
  DVLOG_FUNC(1);
}

DCOMPTextureWrapperImpl::~DCOMPTextureWrapperImpl() {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // We do not need any additional cleanup logic here as the
  // OnReleaseVideoFrame() callback handles cleaning up the shared image.
}

// TODO(xhwang): Remove `init_cb` and return the result synchronously.
void DCOMPTextureWrapperImpl::Initialize(
    const gfx::Size& natural_size,
    DCOMPSurfaceHandleBoundCB dcomp_surface_handle_bound_cb,
    CompositionParamsReceivedCB comp_params_received_cb,
    InitCB init_cb) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  natural_size_ = natural_size;

  dcomp_texture_host_ = factory_->CreateDCOMPTextureHost(this);
  if (!dcomp_texture_host_) {
    std::move(init_cb).Run(false);
    return;
  }

  dcomp_surface_handle_bound_cb_ = std::move(dcomp_surface_handle_bound_cb);
  comp_params_received_cb_ = std::move(comp_params_received_cb);

  std::move(init_cb).Run(true);
}

void DCOMPTextureWrapperImpl::UpdateTextureSize(const gfx::Size& new_size) {
  DVLOG_FUNC(2);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // We would like to invoke SetTextureSize() which will let DCOMPTexture to
  // bind a mailbox to its SharedImage as early as possible. Let new_size of
  // gfx::Size(1, 1) to pass thru. as it is the initial |natural_size_|.
  if (natural_size_ == new_size && new_size != gfx::Size(1, 1))
    return;

  natural_size_ = new_size;
  dcomp_texture_host_->SetTextureSize(new_size);
}

void DCOMPTextureWrapperImpl::SetDCOMPSurface(
    const base::UnguessableToken& surface_token) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  dcomp_texture_host_->SetDCOMPSurface(surface_token);
}

void DCOMPTextureWrapperImpl::CreateVideoFrame(
    CreateVideoFrameCB create_video_frame_cb) {
  DVLOG_FUNC(2);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

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
    sii->NotifyMailboxAdded(mailbox_, gpu::SHARED_IMAGE_USAGE_DISPLAY |
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

  std::move(create_video_frame_cb).Run(frame);
}

void DCOMPTextureWrapperImpl::OnSharedImageMailboxBound(gpu::Mailbox mailbox) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  mailbox_ = std::move(mailbox);

  if (!create_video_frame_cb_.is_null()) {
    DVLOG_FUNC(3) << "Mailbox bound: CreateVideoFrame";
    CreateVideoFrame(std::move(create_video_frame_cb_));
  }
}

void DCOMPTextureWrapperImpl::OnDCOMPSurfaceHandleBound(bool success) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(dcomp_surface_handle_bound_cb_);

  std::move(dcomp_surface_handle_bound_cb_).Run(success);
}

void DCOMPTextureWrapperImpl::OnCompositionParamsReceived(
    gfx::Rect output_rect) {
  DVLOG_FUNC(1);
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  comp_params_received_cb_.Run(output_rect);
}

}  // namespace content
