// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/vea_encoder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

namespace blink {
namespace {

// HW encoders expect a nonzero bitrate, so |kVEADefaultBitratePerPixel| is used
// to estimate bits per second for ~30 fps with ~1/16 compression rate.
const int kVEADefaultBitratePerPixel = 2;
// Number of output buffers used to copy the encoded data coming from HW
// encoders.
const int kVEAEncoderOutputBufferCount = 4;

}  // anonymous namespace

bool VEAEncoder::OutputBuffer::IsValid() {
  return region.IsValid() && mapping.IsValid();
}

VEAEncoder::VEAEncoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const VideoTrackRecorder::OnErrorCB& on_error_cb,
    media::Bitrate::Mode bitrate_mode,
    uint32_t bits_per_second,
    media::VideoCodecProfile codec,
    std::optional<uint8_t> level,
    const gfx::Size& size,
    bool use_native_input,
    bool is_screencast)
    : Encoder(std::move(encoding_task_runner),
              on_encoded_video_cb,
              bits_per_second > 0
                  ? bits_per_second
                  : size.GetArea() * kVEADefaultBitratePerPixel),
      gpu_factories_(Platform::Current()->GetGpuFactories()),
      codec_(codec),
      level_(level),
      bitrate_mode_(bitrate_mode),
      size_(size),
      use_native_input_(use_native_input),
      is_screencast_(is_screencast),
      error_notified_(false),
      on_error_cb_(on_error_cb) {
  DCHECK(gpu_factories_);
}

VEAEncoder::~VEAEncoder() {
  video_encoder_.reset();
}

void VEAEncoder::RequireBitstreamBuffers(unsigned int /*input_count*/,
                                         const gfx::Size& input_coded_size,
                                         size_t output_buffer_size) {
  DVLOG(3) << __func__;

  vea_requested_input_coded_size_ = input_coded_size;
  output_buffers_.clear();
  input_buffers_.clear();

  for (int i = 0; i < kVEAEncoderOutputBufferCount; ++i) {
    auto output_buffer = std::make_unique<OutputBuffer>();
    output_buffer->region =
        gpu_factories_->CreateSharedMemoryRegion(output_buffer_size);
    output_buffer->mapping = output_buffer->region.Map();
    if (output_buffer->IsValid())
      output_buffers_.push_back(std::move(output_buffer));
  }

  for (size_t i = 0; i < output_buffers_.size(); ++i)
    UseOutputBitstreamBufferId(static_cast<int32_t>(i));
}

void VEAEncoder::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(3) << __func__;

  OutputBuffer* output_buffer = output_buffers_[bitstream_buffer_id].get();
  auto data_span = output_buffer->mapping.GetMemoryAsSpan<const uint8_t>(
      metadata.payload_size_bytes);

  auto front_frame = frames_in_encode_.front();
  frames_in_encode_.pop();

  if (metadata.encoded_size) {
    front_frame.first.visible_rect_size = *metadata.encoded_size;
  }

  auto buffer = media::DecoderBuffer::CopyFrom(data_span);
  buffer->set_is_key_frame(metadata.key_frame);

  on_encoded_video_cb_.Run(front_frame.first, std::move(buffer), std::nullopt,
                           front_frame.second);

  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

void VEAEncoder::NotifyErrorStatus(const media::EncoderStatus& status) {
  DVLOG(3) << __func__;
  CHECK(!status.is_ok());
  DLOG(ERROR) << "NotifyErrorStatus() is called with code="
              << static_cast<int>(status.code())
              << ", message=" << status.message();
  metrics_provider_->SetError(status);
  on_error_cb_.Run();
  error_notified_ = true;
}

void VEAEncoder::UseOutputBitstreamBufferId(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__;
  metrics_provider_->IncrementEncodedFrameCount();

  video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id,
      output_buffers_[bitstream_buffer_id]->region.Duplicate(),
      output_buffers_[bitstream_buffer_id]->region.GetSize()));
}

void VEAEncoder::FrameFinished(
    std::unique_ptr<base::MappedReadOnlyRegion> shm) {
  DVLOG(3) << __func__;
  input_buffers_.push_back(std::move(shm));
}

void VEAEncoder::EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                             base::TimeTicks capture_timestamp,
                             bool request_keyframe) {
  TRACE_EVENT0("media", "VEAEncoder::EncodeFrame");
  DVLOG(3) << __func__;

  if (input_visible_size_ != frame->visible_rect().size() && video_encoder_) {
    // TODO(crbug.com/719023): This is incorrect. Flush() should instead be
    // called to ensure submitted outputs are retrieved first.
    video_encoder_.reset();
  }

  if (!video_encoder_) {
    bool use_native_input =
        frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    ConfigureEncoder(frame->visible_rect().size(), use_native_input);
  }

  if (error_notified_) {
    DLOG(ERROR) << "An error occurred in VEA encoder";
    return;
  }

  // Drop frames if RequireBitstreamBuffers() hasn't been called.
  if (output_buffers_.empty() || vea_requested_input_coded_size_.IsEmpty()) {
    // TODO(emircan): Investigate if resetting encoder would help.
    DVLOG(3) << "Might drop frame.";
    last_frame_ = std::make_unique<VideoFrameAndMetadata>(
        std::move(frame), capture_timestamp, request_keyframe);
    return;
  }

  // If first frame hasn't been encoded, do it first.
  if (last_frame_) {
    std::unique_ptr<VideoFrameAndMetadata> last_frame = std::move(last_frame_);
    last_frame_ = nullptr;
    EncodeFrame(last_frame->frame, last_frame->timestamp,
                last_frame->request_keyframe);
  }

  // Lower resolutions may fall back to SW encoder in some platforms, i.e. Mac.
  // In that case, the encoder expects more frames before returning result.
  // Therefore, a copy is necessary to release the current frame.
  // Only STORAGE_SHMEM backed frames can be shared with GPU process, therefore
  // a copy is required for other storage types.
  // With STORAGE_GPU_MEMORY_BUFFER we delay the scaling of the frame to the end
  // of the encoding pipeline.
  scoped_refptr<media::VideoFrame> video_frame = frame;
  bool can_share_frame =
      (video_frame->storage_type() == media::VideoFrame::STORAGE_SHMEM);
  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      (!can_share_frame ||
       vea_requested_input_coded_size_ != frame->coded_size() ||
       input_visible_size_.width() < kVEAEncoderMinResolutionWidth ||
       input_visible_size_.height() < kVEAEncoderMinResolutionHeight)) {
    TRACE_EVENT0("media", "VEAEncoder::EncodeFrame::Copy");
    // Create SharedMemory backed input buffers as necessary. These SharedMemory
    // instances will be shared with GPU process.
    const size_t desired_mapped_size = media::VideoFrame::AllocationSize(
        media::PIXEL_FORMAT_I420, vea_requested_input_coded_size_);
    std::unique_ptr<base::MappedReadOnlyRegion> input_buffer;
    if (input_buffers_.empty()) {
      input_buffer = std::make_unique<base::MappedReadOnlyRegion>(
          base::ReadOnlySharedMemoryRegion::Create(desired_mapped_size));
      if (!input_buffer->IsValid())
        return;
    } else {
      do {
        input_buffer = std::move(input_buffers_.back());
        input_buffers_.pop_back();
      } while (!input_buffers_.empty() &&
               input_buffer->mapping.size() < desired_mapped_size);
      if (!input_buffer || input_buffer->mapping.size() < desired_mapped_size)
        return;
    }

    video_frame = media::VideoFrame::WrapExternalData(
        media::PIXEL_FORMAT_I420, vea_requested_input_coded_size_,
        gfx::Rect(input_visible_size_), input_visible_size_,
        input_buffer->mapping.GetMemoryAsSpan<uint8_t>().data(),
        input_buffer->mapping.size(), frame->timestamp());
    if (!video_frame) {
      NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderFailedEncode,
                         "Failed to create VideoFrame"});
      return;
    }
    libyuv::I420Copy(
        frame->visible_data(media::VideoFrame::Plane::kY),
        frame->stride(media::VideoFrame::Plane::kY),
        frame->visible_data(media::VideoFrame::Plane::kU),
        frame->stride(media::VideoFrame::Plane::kU),
        frame->visible_data(media::VideoFrame::Plane::kV),
        frame->stride(media::VideoFrame::Plane::kV),
        video_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
        video_frame->stride(media::VideoFrame::Plane::kY),
        video_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
        video_frame->stride(media::VideoFrame::Plane::kU),
        video_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
        video_frame->stride(media::VideoFrame::Plane::kV),
        input_visible_size_.width(), input_visible_size_.height());
    video_frame->BackWithSharedMemory(&input_buffer->region);
    video_frame->AddDestructionObserver(base::BindPostTask(
        encoding_task_runner_,
        WTF::BindOnce(&VEAEncoder::FrameFinished, weak_factory_.GetWeakPtr(),
                      std::move(input_buffer))));
  }
  frames_in_encode_.emplace(media::Muxer::VideoParameters(*frame),
                            capture_timestamp);

  video_encoder_->Encode(video_frame, request_keyframe);
}

void VEAEncoder::Initialize() {
  ConfigureEncoder(size_, use_native_input_);
}

void VEAEncoder::ConfigureEncoder(const gfx::Size& size,
                                  bool use_native_input) {
  DVLOG(3) << __func__;
  DCHECK_NE(bits_per_second_, 0u);

  input_visible_size_ = size;
  vea_requested_input_coded_size_ = gfx::Size();
  video_encoder_ = gpu_factories_->CreateVideoEncodeAccelerator();

  auto pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_I420;
  auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  if (use_native_input) {
    // Currently the VAAPI and V4L2 VEA support only native input mode with NV12
    // DMA-buf buffers.
    pixel_format = media::PIXEL_FORMAT_NV12;
    storage_type =
        media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  }

  auto bitrate = media::Bitrate::ConstantBitrate(bits_per_second_);
  if (bitrate_mode_ == media::Bitrate::Mode::kVariable) {
    constexpr uint32_t kNumPixelsIn4KResolution = 3840 * 2160;
    constexpr uint32_t kMaxAllowedBitrate =
        kNumPixelsIn4KResolution * kVEADefaultBitratePerPixel;
    const uint32_t max_peak_bps =
        std::max(bits_per_second_, kMaxAllowedBitrate);
    // This magnification is determined in crbug.com/1342850.
    constexpr uint32_t kPeakBpsMagnification = 2;
    base::CheckedNumeric<uint32_t> peak_bps = bits_per_second_;
    peak_bps *= kPeakBpsMagnification;
    bitrate = media::Bitrate::VariableBitrate(
        bits_per_second_,
        base::strict_cast<uint32_t>(peak_bps.ValueOrDefault(max_peak_bps)));
  }

  metrics_provider_->Initialize(codec_, input_visible_size_,
                                /*is_hardware_encoder=*/true);
  // TODO(b/181797390): Use VBR bitrate mode.
  // TODO(crbug.com/1289907): remove the cast to uint32_t once
  // |bits_per_second_| is stored as uint32_t.
  media::VideoEncodeAccelerator::Config config(
      pixel_format, input_visible_size_, codec_, bitrate,
      media::VideoEncodeAccelerator::kDefaultFramerate, storage_type,
      is_screencast_
          ? media::VideoEncodeAccelerator::Config::ContentType::kDisplay
          : media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  config.h264_output_level = level_;
  if (!video_encoder_ ||
      !video_encoder_->Initialize(config, this,
                                  std::make_unique<media::NullMediaLog>())) {
    NotifyErrorStatus({media::EncoderStatus::Codes::kEncoderInitializationError,
                       "Failed to initialize"});
  }
}

}  // namespace blink
