// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateless.h"

namespace media {

namespace {

// See http://crbug.com/255116.
constexpr int k1080pArea = 1920 * 1088;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
constexpr size_t kNumInputBuffers = 16;

// Input format V4L2 fourccs this class supports.
constexpr uint32_t kSupportedInputFourccs[] = {
    V4L2_PIX_FMT_H264_SLICE,
    V4L2_PIX_FMT_VP8_FRAME,
    V4L2_PIX_FMT_VP9_FRAME,
};

}  // namespace

// static
std::unique_ptr<VideoDecoderPipeline::DecoderInterface>
V4L2SliceVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    GetFramePoolCB get_pool_cb) {
  DCHECK(decoder_task_runner->RunsTasksInCurrentSequence());
  DCHECK(get_pool_cb);

  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device) {
    VLOGF(1) << "Failed to create V4L2 device.";
    return nullptr;
  }

  return base::WrapUnique<VideoDecoderPipeline::DecoderInterface>(
      new V4L2SliceVideoDecoder(std::move(decoder_task_runner),
                                std::move(device), std::move(get_pool_cb)));
}

// static
SupportedVideoDecoderConfigs V4L2SliceVideoDecoder::GetSupportedConfigs() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedVideoDecoderConfigs();

  return ConvertFromSupportedProfiles(
      device->GetSupportedDecodeProfiles(base::size(kSupportedInputFourccs),
                                         kSupportedInputFourccs),
      false);
}

V4L2SliceVideoDecoder::V4L2SliceVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    scoped_refptr<V4L2Device> device,
    GetFramePoolCB get_pool_cb)
    : device_(std::move(device)),
      get_pool_cb_(std::move(get_pool_cb)),
      decoder_task_runner_(std::move(decoder_task_runner)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2SliceVideoDecoder::~V4L2SliceVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  // Call all pending decode callback.
  if (backend_) {
    backend_->ClearPendingRequests(DecodeStatus::ABORTED);
    backend_ = nullptr;
  }

  // Stop and Destroy device.
  StopStreamV4L2Queue();
  if (input_queue_) {
    input_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
  }
  if (output_queue_) {
    output_queue_->DeallocateBuffers();
    output_queue_ = nullptr;
  }

  weak_this_factory_.InvalidateWeakPtrs();
}

void V4L2SliceVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                       InitCB init_cb,
                                       const OutputCB& output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(state_ == State::kUninitialized || state_ == State::kDecoding);
  DVLOGF(3);

  // Reset V4L2 device and queue if reinitializing decoder.
  if (state_ != State::kUninitialized) {
    if (!StopStreamV4L2Queue()) {
      std::move(init_cb).Run(false);
      return;
    }

    input_queue_->DeallocateBuffers();
    output_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
    output_queue_ = nullptr;

    device_ = V4L2Device::Create();
    if (!device_) {
      VLOGF(1) << "Failed to create V4L2 device.";
      std::move(init_cb).Run(false);
      return;
    }

    if (backend_)
      backend_ = nullptr;

    SetState(State::kUninitialized);
  }

  // Setup frame pool.
  frame_pool_ = get_pool_cb_.Run();

  // Open V4L2 device.
  VideoCodecProfile profile = config.profile();
  uint32_t input_format_fourcc =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(profile, true);
  if (!input_format_fourcc ||
      !device_->Open(V4L2Device::Type::kDecoder, input_format_fourcc)) {
    VLOGF(1) << "Failed to open device for profile: " << profile
             << " fourcc: " << FourccToString(input_format_fourcc);
    std::move(init_cb).Run(false);
    return;
  }

  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) ||
      (caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP, "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    std::move(init_cb).Run(false);
    return;
  }

  pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  // Create Input/Output V4L2Queue
  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to create V4L2 queue.";
    std::move(init_cb).Run(false);
    return;
  }

  // Create the backend (only stateless API supported as of now).
  backend_ = std::make_unique<V4L2StatelessVideoDecoderBackend>(
      this, device_, frame_pool_, profile, decoder_task_runner_);
  if (!backend_->Initialize()) {
    std::move(init_cb).Run(false);
    return;
  }

  // Setup input format.
  if (!SetupInputFormat(input_format_fourcc)) {
    VLOGF(1) << "Failed to setup input format.";
    std::move(init_cb).Run(false);
    return;
  }

  if (!SetCodedSizeOnInputQueue(config.coded_size())) {
    VLOGF(1) << "Failed to set coded size on input queue";
    std::move(init_cb).Run(false);
    return;
  }

  // Setup output format.
  if (!SetupOutputFormat(config.coded_size(), config.visible_rect())) {
    VLOGF(1) << "Failed to setup output format.";
    std::move(init_cb).Run(false);
    return;
  }

  if (input_queue_->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP) == 0) {
    VLOGF(1) << "Failed to allocate input buffer.";
    std::move(init_cb).Run(false);
    return;
  }

  // Call init_cb
  output_cb_ = output_cb;
  SetState(State::kDecoding);
  std::move(init_cb).Run(true);
}

bool V4L2SliceVideoDecoder::SetupInputFormat(uint32_t input_format_fourcc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kUninitialized);

  // Check if the format is supported.
  std::vector<uint32_t> formats = device_->EnumerateSupportedPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (std::find(formats.begin(), formats.end(), input_format_fourcc) ==
      formats.end()) {
    DVLOGF(3) << "Input fourcc " << input_format_fourcc
              << " not supported by device.";
    return false;
  }

  // Determine the input buffer size.
  gfx::Size max_size, min_size;
  device_->GetSupportedResolution(input_format_fourcc, &min_size, &max_size);
  size_t input_size = max_size.GetArea() > k1080pArea
                          ? kInputBufferMaxSizeFor4k
                          : kInputBufferMaxSizeFor1080p;

  // Setup the input format.
  auto format =
      input_queue_->SetFormat(input_format_fourcc, gfx::Size(), input_size);
  if (!format) {
    VPLOGF(1) << "Failed to call IOCTL to set input format.";
    return false;
  }
  DCHECK_EQ(format->fmt.pix_mp.pixelformat, input_format_fourcc);

  return true;
}

bool V4L2SliceVideoDecoder::SetCodedSizeOnInputQueue(
    const gfx::Size& coded_size) {
  struct v4l2_format format = {};

  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  if (device_->Ioctl(VIDIOC_G_FMT, &format) != 0) {
    VPLOGF(1) << "Failed getting OUTPUT format";
    return false;
  }

  format.fmt.pix_mp.width = coded_size.width();
  format.fmt.pix_mp.height = coded_size.height();

  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    VPLOGF(1) << "Failed setting OUTPUT format";
    return false;
  }

  return true;
}

base::Optional<GpuBufferLayout> V4L2SliceVideoDecoder::SetupOutputFormat(
    const gfx::Size& size,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  const std::vector<uint32_t> formats = device_->EnumerateSupportedPixelformats(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  DCHECK(!formats.empty());
  for (const auto format_fourcc : formats) {
    if (!device_->CanCreateEGLImageFrom(format_fourcc))
      continue;

    base::Optional<struct v4l2_format> format =
        output_queue_->SetFormat(format_fourcc, size, 0);
    if (!format)
      continue;

    // SetFormat is successful. Next make sure VFPool can allocate video frames
    // with width and height adjusted by a video driver.
    gfx::Size adjusted_size(format->fmt.pix_mp.width,
                            format->fmt.pix_mp.height);

    // Make sure VFPool can allocate video frames with width and height.
    auto layout =
        UpdateVideoFramePoolFormat(format_fourcc, adjusted_size, visible_rect);
    if (!layout)
      continue;

    if (layout->size() != adjusted_size) {
      VLOGF(1) << "The size adjusted by VFPool is different from one "
               << "adjusted by a video driver. fourcc: " << format_fourcc
               << ", (video driver v.s. VFPool) " << adjusted_size.ToString()
               << " != " << layout->size().ToString();
      continue;
    }
    return layout;
  }

  // TODO(akahuang): Use ImageProcessor in this case.
  VLOGF(2) << "WARNING: Cannot find format that can create EGL image. "
           << "We need ImageProcessor to convert pixel format.";
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<GpuBufferLayout>
V4L2SliceVideoDecoder::UpdateVideoFramePoolFormat(
    uint32_t output_format_fourcc,
    const gfx::Size& size,
    const gfx::Rect& visible_rect) {
  gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
  return frame_pool_->RequestFrames(
      Fourcc::FromV4L2PixFmt(output_format_fourcc), size, visible_rect,
      natural_size, num_output_frames_);
}

void V4L2SliceVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // Call all pending decode callback.
  backend_->ClearPendingRequests(DecodeStatus::ABORTED);

  // Streamoff V4L2 queues to drop input and output buffers.
  // If the queues are streaming before reset, then we need to start streaming
  // them after stopping.
  bool is_streaming = input_queue_->IsStreaming();
  if (!StopStreamV4L2Queue())
    return;

  if (is_streaming) {
    if (!StartStreamV4L2Queue())
      return;
  }

  std::move(closure).Run();
}

void V4L2SliceVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                   DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_NE(state_, State::kUninitialized);

  if (state_ == State::kError) {
    std::move(decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  const int32_t bitstream_id = bitstream_id_generator_.GetNextBitstreamId();
  backend_->EnqueueDecodeTask(std::move(buffer), std::move(decode_cb),
                              bitstream_id);
}

bool V4L2SliceVideoDecoder::StartStreamV4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!input_queue_->Streamon() || !output_queue_->Streamon()) {
    VLOGF(1) << "Failed to streamon V4L2 queue.";
    SetState(State::kError);
    return false;
  }

  if (!device_->StartPolling(
          base::BindRepeating(&V4L2SliceVideoDecoder::ServiceDeviceTask,
                              weak_this_),
          base::BindRepeating(&V4L2SliceVideoDecoder::SetState, weak_this_,
                              State::kError))) {
    SetState(State::kError);
    return false;
  }

  return true;
}

bool V4L2SliceVideoDecoder::StopStreamV4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!device_->StopPolling()) {
    SetState(State::kError);
    return false;
  }

  // Streamoff input and output queue.
  if (input_queue_)
    input_queue_->Streamoff();
  if (output_queue_)
    output_queue_->Streamoff();

  if (backend_)
    backend_->OnStreamStopped();

  return true;
}

void V4L2SliceVideoDecoder::InitiateFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  SetState(State::kFlushing);
}

void V4L2SliceVideoDecoder::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  SetState(State::kDecoding);
}

bool V4L2SliceVideoDecoder::ChangeResolution(gfx::Size pic_size,
                                             gfx::Rect visible_rect,
                                             size_t num_output_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK_EQ(state_, State::kFlushing);
  DCHECK_EQ(input_queue_->QueuedBuffersCount(), 0u);
  DCHECK_EQ(output_queue_->QueuedBuffersCount(), 0u);

  num_output_frames_ = num_output_frames;

  if (!StopStreamV4L2Queue())
    return false;

  if (!output_queue_->DeallocateBuffers()) {
    SetState(State::kError);
    return false;
  }
  DCHECK_GT(num_output_frames, 0u);

  if (!SetCodedSizeOnInputQueue(pic_size)) {
    VLOGF(1) << "Failed to set coded size on input queue";
    return false;
  }

  auto layout = SetupOutputFormat(pic_size, visible_rect);
  if (!layout) {
    VLOGF(1) << "No format is available with thew new resolution";
    SetState(State::kError);
    return false;
  }

  auto coded_size = layout->size();
  DCHECK_EQ(coded_size.width() % 16, 0);
  DCHECK_EQ(coded_size.height() % 16, 0);
  if (!gfx::Rect(coded_size).Contains(gfx::Rect(pic_size))) {
    VLOGF(1) << "Got invalid adjusted coded size: " << coded_size.ToString();
    SetState(State::kError);
    return false;
  }

  if (output_queue_->AllocateBuffers(num_output_frames, V4L2_MEMORY_DMABUF) ==
      0) {
    VLOGF(1) << "Failed to request output buffers.";
    SetState(State::kError);
    return false;
  }
  if (output_queue_->AllocatedBuffersCount() != num_output_frames_) {
    VLOGF(1) << "Could not allocate requested number of output buffers.";
    SetState(State::kError);
    return false;
  }

  if (!StartStreamV4L2Queue()) {
    SetState(State::kError);
    return false;
  }

  return true;
}

void V4L2SliceVideoDecoder::ServiceDeviceTask(bool /* event */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Number of queued input buffers: "
            << input_queue_->QueuedBuffersCount()
            << ", Number of queued output buffers: "
            << output_queue_->QueuedBuffersCount();

  // Dequeue V4L2 output buffer first to reduce output latency.
  bool success;
  V4L2ReadableBufferRef dequeued_buffer;
  while (output_queue_->QueuedBuffersCount() > 0) {
    std::tie(success, dequeued_buffer) = output_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;

    backend_->OnOutputBufferDequeued(std::move(dequeued_buffer));
  }

  // Dequeue V4L2 input buffer.
  while (input_queue_->QueuedBuffersCount() > 0) {
    std::tie(success, dequeued_buffer) = input_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;
  }
}

void V4L2SliceVideoDecoder::OutputFrame(scoped_refptr<VideoFrame> frame,
                                        const gfx::Rect& visible_rect,
                                        base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "timestamp: " << timestamp;

  // Set the timestamp at which the decode operation started on the
  // |frame|. If the frame has been outputted before (e.g. because of VP9
  // show-existing-frame feature) we can't overwrite the timestamp directly, as
  // the original frame might still be in use. Instead we wrap the frame in
  // another frame with a different timestamp.
  if (frame->timestamp().is_zero())
    frame->set_timestamp(timestamp);

  if (frame->visible_rect() != visible_rect ||
      frame->timestamp() != timestamp) {
    gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        frame, frame->format(), visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);

    frame = std::move(wrapped_frame);
  }
  output_cb_.Run(std::move(frame));
}

void V4L2SliceVideoDecoder::SetState(State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Change state from " << static_cast<int>(state_) << " to "
            << static_cast<int>(new_state);

  if (state_ == new_state)
    return;
  if (state_ == State::kError) {
    DVLOGF(3) << "Already in kError state.";
    return;
  }

  // Check if the state transition is valid.
  switch (new_state) {
    case State::kUninitialized:
      if (state_ != State::kDecoding) {
        VLOGF(1) << "Should not set to kUninitialized.";
        new_state = State::kError;
      }
      break;

    case State::kDecoding:
      break;

    case State::kFlushing:
      if (state_ != State::kDecoding) {
        VLOGF(1) << "kFlushing should only be set when kDecoding.";
        new_state = State::kError;
      }
      break;

    case State::kError:
      break;
  }

  if (new_state == State::kError) {
    VLOGF(1) << "Error occurred.";
    if (backend_)
      backend_->ClearPendingRequests(DecodeStatus::DECODE_ERROR);
    return;
  }
  state_ = new_state;
  return;
}

void V4L2SliceVideoDecoder::OnBackendError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  SetState(State::kError);
}

bool V4L2SliceVideoDecoder::IsDecoding() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  return state_ == State::kDecoding;
}

}  // namespace media
