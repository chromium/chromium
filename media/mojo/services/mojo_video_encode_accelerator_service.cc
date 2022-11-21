// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_service.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/mojo/mojom/video_encoder_info.mojom.h"
#include "media/mojo/services/mojo_media_log.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

// static
void MojoVideoEncodeAcceleratorService::Create(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver,
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoVideoEncodeAcceleratorService>(
          std::move(create_vea_callback), gpu_preferences, gpu_workarounds,
          gpu_device),
      std::move(receiver));
}

MojoVideoEncodeAcceleratorService::MojoVideoEncodeAcceleratorService(
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device)
    : create_vea_callback_(std::move(create_vea_callback)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      gpu_device_(gpu_device),
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
    mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient> client,
    mojo::PendingRemote<mojom::MediaLog> media_log,
    InitializeCallback success_callback) {
  DVLOG(1) << __func__ << " " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.input_format == PIXEL_FORMAT_I420 ||
         config.input_format == PIXEL_FORMAT_NV12)
      << "Only I420 or NV12 format supported, got "
      << VideoPixelFormatToString(config.input_format);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  media_log_ =
      std::make_unique<MojoMediaLog>(std::move(media_log), task_runner);

  if (gpu_workarounds_.disable_accelerated_vp8_encode &&
      config.output_profile == VP8PROFILE_ANY) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << " VP8 encoding disabled by GPU policy";
    std::move(success_callback).Run(false);
    return;
  }

  if (gpu_workarounds_.disable_accelerated_vp9_encode &&
      config.output_profile >= VP9PROFILE_PROFILE0 &&
      config.output_profile <= VP9PROFILE_PROFILE3) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << " VP9 encoding disabled by GPU policy";
    std::move(success_callback).Run(false);
    return;
  }

  if (gpu_workarounds_.disable_accelerated_h264_encode &&
      config.output_profile >= H264PROFILE_MIN &&
      config.output_profile <= H264PROFILE_MAX) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << " H.264 encoding disabled by GPU policy";
    std::move(success_callback).Run(false);
    return;
  }

  if (encoder_) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << " VEA is already initialized";
    std::move(success_callback).Run(false);
    return;
  }

  if (!client) {
    MEDIA_LOG(ERROR, media_log_.get()) << __func__ << "null |client|";
    std::move(success_callback).Run(false);
    return;
  }
  vea_client_.Bind(std::move(client));

  if (config.input_visible_size.width() > limits::kMaxDimension ||
      config.input_visible_size.height() > limits::kMaxDimension ||
      config.input_visible_size.GetArea() > limits::kMaxCanvas) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << "too large input_visible_size "
        << config.input_visible_size.ToString();
    std::move(success_callback).Run(false);
    return;
  }

  encoder_ = std::move(create_vea_callback_)
                 .Run(config, this, *gpu_preferences_, gpu_workarounds_,
                      *gpu_device_, media_log_->Clone());
  if (!encoder_) {
    MEDIA_LOG(ERROR, media_log_.get())
        << __func__ << " Error creating or initializing VEA";
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
  if (!encoder_) {
    DLOG(ERROR) << __func__ << " Failed to encode, the encoder is invalid";
    std::move(callback).Run();
    return;
  }

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
    base::UnsafeSharedMemoryRegion region) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_)
    return;
  if (!region.IsValid()) {
    DLOG(ERROR) << __func__ << " invalid |region|.";
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }
  if (bitstream_buffer_id < 0) {
    DLOG(ERROR) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
                << " must be >= 0";
    NotifyError(::media::VideoEncodeAccelerator::kInvalidArgumentError);
    return;
  }

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

void MojoVideoEncodeAcceleratorService::
    RequestEncodingParametersChangeWithLayers(
        const media::VideoBitrateAllocation& bitrate_allocation,
        uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_)
    return;

  DVLOG(2) << __func__ << " bitrate=" << bitrate_allocation.GetSumBps()
           << " framerate=" << framerate;

  encoder_->RequestEncodingParametersChange(bitrate_allocation, framerate);
}

void MojoVideoEncodeAcceleratorService::
    RequestEncodingParametersChangeWithBitrate(const media::Bitrate& bitrate,
                                               uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_)
    return;

  DVLOG(2) << __func__ << " bitrate=" << bitrate.target_bps()
           << " framerate=" << framerate;

  encoder_->RequestEncodingParametersChange(bitrate, framerate);
}

void MojoVideoEncodeAcceleratorService::IsFlushSupported(
    IsFlushSupportedCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_) {
    DLOG(ERROR) << __func__
                << " Failed to detect flush support, the encoder is invalid";
    std::move(callback).Run(false);
    return;
  }

  bool flush_support = encoder_->IsFlushSupported();
  std::move(callback).Run(flush_support);
}

void MojoVideoEncodeAcceleratorService::Flush(FlushCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_) {
    DLOG(ERROR) << __func__ << " Failed to flush, the encoder is invalid";
    std::move(callback).Run(false);
    return;
  }

  encoder_->Flush(std::move(callback));
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

void MojoVideoEncodeAcceleratorService::NotifyEncoderInfoChange(
    const ::media::VideoEncoderInfo& info) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vea_client_)
    return;

  vea_client_->NotifyEncoderInfoChange(info);
}

}  // namespace media
