// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/vea_encoder.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;
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

scoped_refptr<VEAEncoder> VEAEncoder::Create(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    const VideoTrackRecorder::OnErrorCB& on_error_callback,
    int32_t bits_per_second,
    media::VideoCodecProfile codec,
    const gfx::Size& size,
    bool use_native_input,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto encoder = base::AdoptRef(
      new VEAEncoder(on_encoded_video_callback, on_error_callback,
                     bits_per_second, codec, size, std::move(task_runner)));
  PostCrossThreadTask(
      *encoder->encoding_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&VEAEncoder::ConfigureEncoderOnEncodingTaskRunner,
                          encoder, size, use_native_input));
  return encoder;
}

bool VEAEncoder::OutputBuffer::IsValid() {
  return region.IsValid() && mapping.IsValid();
}

VEAEncoder::VEAEncoder(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    const VideoTrackRecorder::OnErrorCB& on_error_callback,
    int32_t bits_per_second,
    media::VideoCodecProfile codec,
    const gfx::Size& size,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : Encoder(on_encoded_video_callback,
              bits_per_second > 0 ? bits_per_second
                                  : size.GetArea() * kVEADefaultBitratePerPixel,
              std::move(task_runner),
              Platform::Current()->GetGpuFactories()->GetTaskRunner()),
      gpu_factories_(Platform::Current()->GetGpuFactories()),
      codec_(codec),
      error_notified_(false),
      num_frames_after_keyframe_(0),
      force_next_frame_to_be_keyframe_(false),
      on_error_callback_(on_error_callback) {
  DCHECK(gpu_factories_);
  DCHECK_GE(size.width(), kVEAEncoderMinResolutionWidth);
  DCHECK_GE(size.height(), kVEAEncoderMinResolutionHeight);
}

VEAEncoder::~VEAEncoder() {
  if (encoding_task_runner_->BelongsToCurrentThread()) {
    DestroyOnEncodingTaskRunner();
    return;
  }

  base::WaitableEvent release_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // CrossThreadUnretained is safe because the class will be alive until
  // |release_waiter| is signaled.
  // TODO(emircan): Consider refactoring media::VideoEncodeAccelerator to avoid
  // using naked pointers and using DeleteSoon() here, see
  // http://crbug.com/701627.
  // It is currently unsafe because |video_encoder_| might be in use on another
  // function on |encoding_task_runner_|, see http://crbug.com/701030.
  PostCrossThreadTask(
      *encoding_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&VEAEncoder::DestroyOnEncodingTaskRunner,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&release_waiter)));
  release_waiter.Wait();
}

void VEAEncoder::RequireBitstreamBuffers(unsigned int /*input_count*/,
                                         const gfx::Size& input_coded_size,
                                         size_t output_buffer_size) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  vea_requested_input_coded_size_ = input_coded_size;
  output_buffers_.clear();
  base::queue<std::unique_ptr<InputBuffer>>().swap(input_buffers_);

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
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

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

  const auto front_frame = frames_in_encode_.front();
  frames_in_encode_.pop();

  PostCrossThreadTask(
      *origin_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          OnFrameEncodeCompleted,
          WTF::Passed(CrossThreadBindRepeating(on_encoded_video_callback_)),
          front_frame.first, std::move(data), std::string(), front_frame.second,
          metadata.key_frame));

  UseOutputBitstreamBufferId(bitstream_buffer_id);
}

void VEAEncoder::NotifyError(media::VideoEncodeAccelerator::Error error) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Media.MediaRecorder.VEAError", error,
                            media::VideoEncodeAccelerator::kErrorMax + 1);
  on_error_callback_.Run();
  error_notified_ = true;
}

void VEAEncoder::UseOutputBitstreamBufferId(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  video_encoder_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id,
      output_buffers_[bitstream_buffer_id]->region.Duplicate(),
      output_buffers_[bitstream_buffer_id]->region.GetSize()));
}

void VEAEncoder::FrameFinished(std::unique_ptr<InputBuffer> shm) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  input_buffers_.push(std::move(shm));
}

void VEAEncoder::EncodeOnEncodingTaskRunner(scoped_refptr<VideoFrame> frame,
                                            base::TimeTicks capture_timestamp) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  if (input_visible_size_ != frame->visible_rect().size() && video_encoder_)
    video_encoder_.reset();

  if (!video_encoder_) {
    bool use_native_input =
        frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    ConfigureEncoderOnEncodingTaskRunner(frame->visible_rect().size(),
                                         use_native_input);
  }

  if (error_notified_) {
    DVLOG(3) << "An error occurred in VEA encoder";
    return;
  }

  // Drop frames if RequireBitstreamBuffers() hasn't been called.
  if (output_buffers_.IsEmpty() || vea_requested_input_coded_size_.IsEmpty()) {
    // TODO(emircan): Investigate if resetting encoder would help.
    DVLOG(3) << "Might drop frame.";
    last_frame_.reset(new std::pair<scoped_refptr<VideoFrame>, base::TimeTicks>(
        frame, capture_timestamp));
    return;
  }

  // If first frame hasn't been encoded, do it first.
  if (last_frame_) {
    std::unique_ptr<VideoFrameAndTimestamp> last_frame(last_frame_.release());
    EncodeOnEncodingTaskRunner(last_frame->first, last_frame->second);
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
    // Create SharedMemory backed input buffers as necessary. These SharedMemory
    // instances will be shared with GPU process.
    const size_t desired_mapped_size = media::VideoFrame::AllocationSize(
        media::PIXEL_FORMAT_I420, vea_requested_input_coded_size_);
    auto input_buffer = std::make_unique<InputBuffer>();
    if (input_buffers_.empty()) {
      input_buffer->region =
          gpu_factories_->CreateSharedMemoryRegion(desired_mapped_size);
      input_buffer->mapping = input_buffer->region.Map();
    } else {
      do {
        input_buffer = std::move(input_buffers_.front());
        input_buffers_.pop();
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
    video_frame->BackWithSharedMemory(&input_buffer->region);
    video_frame->AddDestructionObserver(media::BindToCurrentLoop(
        WTF::Bind(&VEAEncoder::FrameFinished, WrapRefCounted(this),
                  std::move(input_buffer))));
    libyuv::I420Copy(frame->visible_data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->visible_data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->visible_data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     video_frame->visible_data(media::VideoFrame::kYPlane),
                     video_frame->stride(media::VideoFrame::kYPlane),
                     video_frame->visible_data(media::VideoFrame::kUPlane),
                     video_frame->stride(media::VideoFrame::kUPlane),
                     video_frame->visible_data(media::VideoFrame::kVPlane),
                     video_frame->stride(media::VideoFrame::kVPlane),
                     input_visible_size_.width(), input_visible_size_.height());
  }
  frames_in_encode_.push(std::make_pair(
      media::WebmMuxer::VideoParameters(frame), capture_timestamp));

  video_encoder_->Encode(video_frame, force_next_frame_to_be_keyframe_);
  force_next_frame_to_be_keyframe_ = false;
}

void VEAEncoder::ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size,
                                                      bool use_native_input) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_GT(bits_per_second_, 0);

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
    storage_type = media::VideoEncodeAccelerator::Config::StorageType::kDmabuf;
  }
  const media::VideoEncodeAccelerator::Config config(
      pixel_format, input_visible_size_, codec_, bits_per_second_,
      base::nullopt, base::nullopt, base::nullopt, storage_type,
      media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  if (!video_encoder_ || !video_encoder_->Initialize(config, this))
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
}

void VEAEncoder::DestroyOnEncodingTaskRunner(
    base::WaitableEvent* async_waiter) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  video_encoder_.reset();
  if (async_waiter)
    async_waiter->Signal();
}

}  // namespace blink
