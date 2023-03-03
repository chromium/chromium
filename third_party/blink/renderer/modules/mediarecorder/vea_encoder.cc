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
// Force a keyframe in regular intervals.
const uint32_t kMaxKeyframeInterval = 100;

}  // anonymous namespace

bool VEAEncoder::OutputBuffer::IsValid() {
  return region.IsValid() && mapping.IsValid();
}

VEAEncoder::VEAEncoder(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const VideoTrackRecorder::OnErrorCB& on_error_cb,
    media::Bitrate::Mode bitrate_mode,
    uint32_t bits_per_second,
    media::VideoCodecProfile codec,
    absl::optional<uint8_t> level,
    const gfx::Size& size,
    bool use_native_input)
    : Encoder(on_encoded_video_cb,
              bits_per_second > 0
                  ? bits_per_second
                  : size.GetArea() * kVEADefaultBitratePerPixel),
      gpu_factories_(Platform::Current()->GetGpuFactories()),
      codec_(codec),
      level_(level),
      bitrate_mode_(bitrate_mode),
      size_(size),
      use_native_input_(use_native_input),
      error_notified_(false),
      num_frames_after_keyframe_(0),
      force_next_frame_to_be_keyframe_(false),
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

  num_frames_after_keyframe_ =
      metadata.key_frame ? 0 : num_frames_after_keyframe_ + 1;
  if (num_frames_after_keyframe_ > kMaxKeyframeInterval) {
    force_next_frame_to_be_keyframe_ = true;
    num_frames_after_keyframe_ = 0;
  }

  OutputBuffer* output_buffer = output_buffers_[bitstream_buffer_id].get();
  base::span<char> data_span =
      output_buffer->mapping.GetMemoryAsSpan<char>(metadata.payload_size_bytes);
  std::string data(data_span.begin(), data_span.end());

  auto front_frame = frames_in_encode_.front();
  frames_in_encode_.pop();

  if (metadata.encoded_size) {
    front_frame.first.visible_rect_size = *metadata.encoded_size;
  }

  on_encoded_video_cb_.Run(front_frame.first, std::move(data), std::string(),
                           front_frame.second, metadata.key_frame);

  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

void VEAEncoder::NotifyError(media::VideoEncodeAccelerator::Error error) {
  DVLOG(3) << __func__;
  UMA_HISTOGRAM_ENUMERATION("Media.MediaRecorder.VEAError", error,
                            media::VideoEncodeAccelerator::kErrorMax + 1);
  on_error_cb_.Run();
  error_notified_ = true;
}

void VEAEncoder::UseOutputBitstreamBufferId(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__;

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
                             base::TimeTicks capture_timestamp) {
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
    last_frame_ = std::make_unique<
        std::pair<scoped_refptr<media::VideoFrame>, base::TimeTicks>>(
        frame, capture_timestamp);
    return;
  }

  // If first frame hasn't been encoded, do it first.
  if (last_frame_) {
    std::unique_ptr<VideoFrameAndTimestamp> last_frame(last_frame_.release());
    EncodeFrame(last_frame->first, last_frame->second);
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
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    libyuv::I420Copy(
        frame->visible_data(media::VideoFrame::kYPlane),
        frame->stride(media::VideoFrame::kYPlane),
        frame->visible_data(media::VideoFrame::kUPlane),
        frame->stride(media::VideoFrame::kUPlane),
        frame->visible_data(media::VideoFrame::kVPlane),
        frame->stride(media::VideoFrame::kVPlane),
        video_frame->GetWritableVisibleData(media::VideoFrame::kYPlane),
        video_frame->stride(media::VideoFrame::kYPlane),
        video_frame->GetWritableVisibleData(media::VideoFrame::kUPlane),
        video_frame->stride(media::VideoFrame::kUPlane),
        video_frame->GetWritableVisibleData(media::VideoFrame::kVPlane),
        video_frame->stride(media::VideoFrame::kVPlane),
        input_visible_size_.width(), input_visible_size_.height());
    video_frame->BackWithSharedMemory(&input_buffer->region);
    video_frame->AddDestructionObserver(base::BindPostTaskToCurrentDefault(
        WTF::BindOnce(&VEAEncoder::FrameFinished, weak_factory_.GetWeakPtr(),
                      std::move(input_buffer))));
  }
  frames_in_encode_.emplace(media::Muxer::VideoParameters(*frame),
                            capture_timestamp);

  video_encoder_->Encode(video_frame, force_next_frame_to_be_keyframe_);
  force_next_frame_to_be_keyframe_ = false;
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

  // TODO(b/181797390): Use VBR bitrate mode.
  // TODO(crbug.com/1289907): remove the cast to uint32_t once
  // |bits_per_second_| is stored as uint32_t.
  const media::VideoEncodeAccelerator::Config config(
      pixel_format, input_visible_size_, codec_, bitrate, absl::nullopt,
      absl::nullopt, level_, false, storage_type,
      media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  if (!video_encoder_ ||
      !video_encoder_->Initialize(config, this,
                                  std::make_unique<media::NullMediaLog>())) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
  }
}

}  // namespace blink
