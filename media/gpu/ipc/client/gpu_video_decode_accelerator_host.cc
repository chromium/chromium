// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/client/gpu_video_decode_accelerator_host.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace media {

GpuVideoDecodeAcceleratorHost::GpuVideoDecodeAcceleratorHost(
    gpu::CommandBufferProxyImpl* impl)
    : client_(nullptr),
      impl_(impl),
      media_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(impl_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
  impl_->AddDeletionObserver(this);
}

GpuVideoDecodeAcceleratorHost::~GpuVideoDecodeAcceleratorHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(impl_lock_);
  if (impl_)
    impl_->RemoveDeletionObserver(this);
}

void GpuVideoDecodeAcceleratorHost::OnDisconnectedFromGpuProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decoder_.reset();
  client_receiver_.reset();
  DLOG(ERROR) << "OnDisconnectedFromGpuProcess()";
  PostNotifyError(PLATFORM_FAILURE);
}

bool GpuVideoDecodeAcceleratorHost::Initialize(const Config& config,
                                               Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ = client;

  base::AutoLock lock(impl_lock_);
  if (!impl_)
    return false;

  // Mojo will ignore our request to bind to a different thread than the main or
  // IO thread unless we construct this object. It does this to avoid breaking
  // use cases that depend on the behavior of ignoring other bindings, as
  // detailed in the documentation for
  // IPC::ScopedAllowOffSequenceChannelAssociatedBindings.
  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      impl_->channel()->io_task_runner();
  bool succeeded = false;
  mojo::SharedAssociatedRemote<mojom::GpuAcceleratedVideoDecoderProvider>
      provider;
  impl_->BindMediaReceiver(
      provider.BindNewEndpointAndPassReceiver(io_task_runner));
  provider->CreateAcceleratedVideoDecoder(
      config, decoder_.BindNewEndpointAndPassReceiver(io_task_runner),
      client_receiver_.BindNewEndpointAndPassRemote(), &succeeded);
  if (!succeeded) {
    DLOG(ERROR) << "CreateAcceleratedVideoDecoder() failed";
    PostNotifyError(PLATFORM_FAILURE);
    decoder_.reset();
    client_receiver_.reset();
    return false;
  }

  decoder_.set_disconnect_handler(
      base::BindOnce(
          &GpuVideoDecodeAcceleratorHost::OnDisconnectedFromGpuProcess,
          weak_this_),
      base::SequencedTaskRunner::GetCurrentDefault());
  return true;
}

void GpuVideoDecodeAcceleratorHost::Decode(BitstreamBuffer bitstream_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  decoder_->Decode(std::move(bitstream_buffer));
}

void GpuVideoDecodeAcceleratorHost::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  std::vector<mojom::PictureBufferAssignmentPtr> assignments;
  for (const auto& buffer : buffers) {
    if (buffer.size() != picture_buffer_dimensions_) {
      DLOG(ERROR) << "buffer.size() invalid: expected "
                  << picture_buffer_dimensions_.ToString() << ", got "
                  << buffer.size().ToString();
      PostNotifyError(INVALID_ARGUMENT);
      return;
    }
    assignments.push_back(mojom::PictureBufferAssignment::New(
        buffer.id(), buffer.client_texture_ids()));
  }
  decoder_->AssignPictureBuffers(std::move(assignments));
}

void GpuVideoDecodeAcceleratorHost::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  decoder_->Flush(base::BindOnce(&GpuVideoDecodeAcceleratorHost::OnFlushDone,
                                 base::Unretained(this)));
}

void GpuVideoDecodeAcceleratorHost::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  decoder_->Flush(base::BindOnce(&GpuVideoDecodeAcceleratorHost::OnResetDone,
                                 base::Unretained(this)));
}

void GpuVideoDecodeAcceleratorHost::SetOverlayInfo(
    const OverlayInfo& overlay_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decoder_)
    return;
  decoder_->SetOverlayInfo(overlay_info);
}

void GpuVideoDecodeAcceleratorHost::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_ = nullptr;
  delete this;
}

void GpuVideoDecodeAcceleratorHost::OnWillDeleteImpl() {
  base::AutoLock lock(impl_lock_);
  impl_ = nullptr;

  // The gpu::CommandBufferProxyImpl is going away; error out this VDA.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuVideoDecodeAcceleratorHost::OnDisconnectedFromGpuProcess,
          weak_this_));
}

void GpuVideoDecodeAcceleratorHost::PostNotifyError(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "PostNotifyError(): error=" << error;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuVideoDecodeAcceleratorHost::OnError,
                                weak_this_, error));
}

// TODO(tmathmeyer) This needs to accept a Status at some point
void GpuVideoDecodeAcceleratorHost::OnInitializationComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_) {
    client_->NotifyInitializationComplete(
        success ? DecoderStatus::Codes::kOk : DecoderStatus::Codes::kFailed);
  }
}

void GpuVideoDecodeAcceleratorHost::OnBitstreamBufferProcessed(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnProvidePictureBuffers(
    uint32_t num_requested_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  picture_buffer_dimensions_ = dimensions;

  const int kMaxVideoPlanes = 4;
  if (textures_per_buffer > kMaxVideoPlanes) {
    PostNotifyError(PLATFORM_FAILURE);
    return;
  }

  if (client_) {
    client_->ProvidePictureBuffers(num_requested_buffers, format,
                                   textures_per_buffer, dimensions,
                                   texture_target);
  }
}

void GpuVideoDecodeAcceleratorHost::OnDismissPictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_)
    client_->DismissPictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAcceleratorHost::OnPictureReady(
    mojom::PictureReadyParamsPtr params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client_)
    return;
  Picture picture(params->picture_buffer_id, params->bitstream_buffer_id,
                  params->visible_rect, params->color_space,
                  params->allow_overlay);
  picture.set_read_lock_fences_enabled(params->read_lock_fences_enabled);
  picture.set_size_changed(params->size_changed);
  picture.set_texture_owner(params->surface_texture);
  picture.set_wants_promotion_hint(params->wants_promotion_hint);
  client_->PictureReady(picture);
}

void GpuVideoDecodeAcceleratorHost::OnFlushDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_)
    client_->NotifyFlushDone();
}

void GpuVideoDecodeAcceleratorHost::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_)
    client_->NotifyResetDone();
}

void GpuVideoDecodeAcceleratorHost::OnError(uint32_t error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client_)
    return;
  weak_this_factory_.InvalidateWeakPtrs();

  // Client::NotifyError() may Destroy() |this|, so calling it needs to be the
  // last thing done on this stack!
  VideoDecodeAccelerator::Client* client = client_;
  client_ = nullptr;
  client->NotifyError(static_cast<VideoDecodeAccelerator::Error>(error));
}

}  // namespace media
