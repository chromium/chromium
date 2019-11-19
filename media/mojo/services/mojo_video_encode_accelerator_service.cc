// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_service.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

// static
void MojoVideoEncodeAcceleratorService::Create(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver,
    const CreateAndInitializeVideoEncodeAcceleratorCallback&
        create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoVideoEncodeAcceleratorService>(create_vea_callback,
                                                          gpu_preferences),
      std::move(receiver));
}

MojoVideoEncodeAcceleratorService::MojoVideoEncodeAcceleratorService(
    const CreateAndInitializeVideoEncodeAcceleratorCallback&
        create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences)
    : create_vea_callback_(create_vea_callback),
      gpu_preferences_(gpu_preferences),
      output_buffer_size_(0) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MojoVideoEncodeAcceleratorService::~MojoVideoEncodeAcceleratorService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoVideoEncodeAcceleratorService::Initialize(
    const media::VideoEncodeAccelerator::Config& config,
    mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> client,
    InitializeCallback success_callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!encoder_);
  DCHECK(config.input_format == PIXEL_FORMAT_I420 ||
         config.input_format == PIXEL_FORMAT_NV12)
      << "Only I420 or NV12 format supported";

  if (!client) {
    DLOG(ERROR) << __func__ << "null |client|";
    std::move(success_callback).Run(false);
    return;
  }
  vea_client_.Bind(std::move(client));

  if (config.input_visible_size.width() > limits::kMaxDimension ||
      config.input_visible_size.height() > limits::kMaxDimension ||
      config.input_visible_size.GetArea() > limits::kMaxCanvas) {
    DLOG(ERROR) << __func__ << "too large input_visible_size "
                << config.input_visible_size.ToString();
    std::move(success_callback).Run(false);
    return;
  }

  encoder_ = create_vea_callback_.Run(config, this, gpu_preferences_);
  if (!encoder_) {
    DLOG(ERROR) << __func__ << " Error creating or initializing VEA";
    std::move(success_callback).Run(false);
    return;
  }

  std::move(success_callback).Run(true);
  return;
}

void MojoVideoEncodeAcceleratorService::Encode(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe,
    EncodeCallback callback) {
  DVLOG(2) << __func__ << " tstamp=" << frame->timestamp();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!encoder_)
    return;

  if (frame->coded_size() != input_coded_size_ &&
      frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DLOG(ERROR) << __func__ << " wrong input coded size, expected "
                << input_coded_size_.ToString() << ", got "
                << frame->coded_size().ToString();
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    std::move(callback).Run();
    return;
  }

  frame->AddDestructionObserver(media::BindToCurrentLoop(std::move(callback)));
  encoder_->Encode(frame, force_keyframe);
}

void MojoVideoEncodeAcceleratorService::UseOutputBitstreamBuffer(
    int32_t bitstream_buffer_id,
    mojo::ScopedSharedBufferHandle buffer) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_)
    return;
  if (!buffer.is_valid()) {
    DLOG(ERROR) << __func__ << " invalid |buffer|.";
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }
  if (bitstream_buffer_id < 0) {
    DLOG(ERROR) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
                << " must be >= 0";
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

  base::subtle::PlatformSharedMemoryRegion region =
      mojo::UnwrapPlatformSharedMemoryRegion(std::move(buffer));

  auto memory_size = region.GetSize();
  if (memory_size < output_buffer_size_) {
    DLOG(ERROR) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
                << " has a size of " << memory_size
                << "B, different from expected " << output_buffer_size_ << "B";
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

  encoder_->UseOutputBitstreamBuffer(
      BitstreamBuffer(bitstream_buffer_id, std::move(region), memory_size));
}

void MojoVideoEncodeAcceleratorService::RequestEncodingParametersChange(
    const media::VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_)
    return;

  DVLOG(2) << __func__ << " bitrate=" << bitrate_allocation.GetSumBps()
           << " framerate=" << framerate;

  encoder_->RequestEncodingParametersChange(bitrate_allocation, framerate);
}

void MojoVideoEncodeAcceleratorService::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DVLOG(2) << __func__ << " input_count=" << input_count
           << " input_coded_size=" << input_coded_size.ToString()
           << " output_buffer_size=" << output_buffer_size;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vea_client_)
    return;

  output_buffer_size_ = output_buffer_size;
  input_coded_size_ = input_coded_size;

  vea_client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                       output_buffer_size);
}

void MojoVideoEncodeAcceleratorService::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << "B,  key_frame=" << metadata.key_frame;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vea_client_)
    return;

  vea_client_->BitstreamBufferReady(bitstream_buffer_id, metadata);
}

void MojoVideoEncodeAcceleratorService::NotifyError(
    ::media::VideoEncodeAccelerator::Error error) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vea_client_)
    return;

  vea_client_->NotifyError(error);
}

}  // namespace media
