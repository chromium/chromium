// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_impl.h"

#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/media_log.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"

namespace media {

namespace {

static bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

D3D11VideoDecoderImpl::D3D11VideoDecoderImpl(
    std::unique_ptr<MediaLog> media_log,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb)
    : media_log_(std::move(media_log)),
      get_stub_cb_(get_stub_cb),
      weak_factory_(this) {
  // May be called from any thread.
}

D3D11VideoDecoderImpl::~D3D11VideoDecoderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stub_)
    DestroyStub();
}

void D3D11VideoDecoderImpl::Initialize(
    InitCB init_cb,
    ReturnPictureBufferCB return_picture_buffer_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return_picture_buffer_cb_ = std::move(return_picture_buffer_cb);

  // If already have a stub, then we're as initialized as we need to be.
  if (stub_) {
    std::move(init_cb).Run(true);
    return;
  }

  // First init.  Get the stub, register, and generally do stuff.
  stub_ = get_stub_cb_.Run();
  if (!MakeContextCurrent(stub_)) {
    const char* reason = "Failed to get decoder stub";
    DLOG(ERROR) << reason;
    if (media_log_) {
      media_log_->AddEvent(media_log_->CreateStringEvent(
          MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error", reason));
    }
    stub_ = nullptr;
    std::move(init_cb).Run(false);
    return;
  }

  stub_->AddDestructionObserver(this);
  wait_sequence_id_ = stub_->channel()->scheduler()->CreateSequence(
      gpu::SchedulingPriority::kNormal);

  std::move(init_cb).Run(true);
}

void D3D11VideoDecoderImpl::OnMailboxReleased(
    scoped_refptr<D3D11PictureBuffer> buffer,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!stub_)
    return;

  stub_->channel()->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      wait_sequence_id_,
      base::BindOnce(&D3D11VideoDecoderImpl::OnSyncTokenReleased, GetWeakPtr(),
                     std::move(buffer)),
      std::vector<gpu::SyncToken>({sync_token})));
}

void D3D11VideoDecoderImpl::OnSyncTokenReleased(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return_picture_buffer_cb_.Run(std::move(buffer));
}

void D3D11VideoDecoderImpl::OnWillDestroyStub(bool have_context) {
  DestroyStub();
}

void D3D11VideoDecoderImpl::DestroyStub() {
  DCHECK(stub_);
  gpu::CommandBufferStub* stub = stub_;
  stub_ = nullptr;

  stub->RemoveDestructionObserver(this);

  if (!wait_sequence_id_.is_null())
    stub->channel()->scheduler()->DestroySequence(wait_sequence_id_);
}

base::WeakPtr<D3D11VideoDecoderImpl> D3D11VideoDecoderImpl::GetWeakPtr() {
  // May be called from any thread.
  return weak_factory_.GetWeakPtr();
}

}  // namespace media
