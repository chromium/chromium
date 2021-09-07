// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateful.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateless.h"

namespace media {

namespace {

// See http://crbug.com/255116.
constexpr int k1080pArea = 1920 * 1088;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
constexpr size_t kNumInputBuffers = 8;

// Input format V4L2 fourccs this class supports.
constexpr uint32_t kSupportedInputFourccs[] = {
    V4L2_PIX_FMT_H264_SLICE, V4L2_PIX_FMT_VP8_FRAME, V4L2_PIX_FMT_VP9_FRAME,
    V4L2_PIX_FMT_H264,       V4L2_PIX_FMT_VP8,       V4L2_PIX_FMT_VP9,
};

// Number of output buffers to use for each VD stage above what's required by
// the decoder (e.g. DPB size, in H264).  We need limits::kMaxVideoFrames to
// fill up the GpuVideoDecode pipeline, and +1 for a frame in transit.
constexpr size_t kDpbOutputBufferExtraCount = limits::kMaxVideoFrames + 1;

}  // namespace

// static
base::AtomicRefCount V4L2VideoDecoder::num_instances_(0);

// static
std::unique_ptr<VideoDecoderMixin> V4L2VideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  DCHECK(decoder_task_runner->RunsTasksInCurrentSequence());
  DCHECK(client);

  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device) {
    VLOGF(1) << "Failed to create V4L2 device.";
    return nullptr;
  }

  return base::WrapUnique<VideoDecoderMixin>(
      new V4L2VideoDecoder(std::move(media_log), std::move(decoder_task_runner),
                           std::move(client), std::move(device)));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2VideoDecoder::GetSupportedConfigs() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return absl::nullopt;

  auto configs = device->GetSupportedDecodeProfiles(
      base::size(kSupportedInputFourccs), kSupportedInputFourccs);

  if (configs.empty())
    return absl::nullopt;

  return ConvertFromSupportedProfiles(configs, false);
}

V4L2VideoDecoder::V4L2VideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    scoped_refptr<V4L2Device> device)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      device_(std::move(device)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2VideoDecoder::~V4L2VideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  // Call all pending decode callback.
  if (backend_) {
    backend_->ClearPendingRequests(DecodeStatus::ABORTED);
    backend_ = nullptr;
  }

  // Stop and Destroy device.
  StopStreamV4L2Queue(true);
  if (input_queue_) {
    input_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
  }
  if (output_queue_) {
    output_queue_->DeallocateBuffers();
    output_queue_ = nullptr;
  }

  weak_this_factory_.InvalidateWeakPtrs();

  if (can_use_decoder_)
    num_instances_.Decrement();
}

void V4L2VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool /*low_delay*/,
                                  CdmContext* cdm_context,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& /*waiting_cb*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(state_ == State::kUninitialized || state_ == State::kInitialized ||
         state_ == State::kDecoding);
  DVLOGF(3);

  if (cdm_context || config.is_encrypted()) {
    VLOGF(1) << "V4L2 decoder does not support encrypted stream";
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }

  // Stop and reset the queues if we're currently decoding but want to
  // re-initialize the decoder. This is not needed if the decoder is in the
  // |kInitialized| state because the queues should still be stopped in that
  // case.
  if (state_ == State::kDecoding) {
    if (!StopStreamV4L2Queue(true)) {
      std::move(init_cb).Run(StatusCode::kV4l2FailedToStopStreamQueue);
      return;
    }

    input_queue_->DeallocateBuffers();
    output_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
    output_queue_ = nullptr;

    if (can_use_decoder_) {
      num_instances_.Decrement();
      can_use_decoder_ = false;
    }

    continue_change_resolution_cb_.Reset();

    device_ = V4L2Device::Create();
    if (!device_) {
      VLOGF(1) << "Failed to create V4L2 device.";
      SetState(State::kError);
      std::move(init_cb).Run(StatusCode::kV4l2NoDevice);
      return;
    }

    if (backend_)
      backend_ = nullptr;
  }

  DCHECK(!input_queue_);
  DCHECK(!output_queue_);

  profile_ = config.profile();
  aspect_ratio_ = config.aspect_ratio();

  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN) {
    VLOGF(1) << "Unknown profile.";
    SetState(State::kError);
    std::move(init_cb).Run(StatusCode::kV4l2NoDecoder);
    return;
  }

  // Call init_cb
  output_cb_ = std::move(output_cb);
  SetState(State::kInitialized);
  std::move(init_cb).Run(::media::OkStatus());
}

bool V4L2VideoDecoder::NeedsBitstreamConversion() const {
  DCHECK(output_cb_) << "V4L2VideoDecoder hasn't been initialized";
  NOTREACHED();
  return (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) ||
         (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX);
}

bool V4L2VideoDecoder::CanReadWithoutStalling() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return true;
}

int V4L2VideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED();
  return 4;
}

VideoDecoderType V4L2VideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kV4L2;
}

bool V4L2VideoDecoder::IsPlatformDecoder() const {
  return true;
}

StatusCode V4L2VideoDecoder::InitializeBackend() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(state_ == State::kInitialized);

  can_use_decoder_ = num_instances_.Increment() < kMaxNumOfInstances;
  if (!can_use_decoder_) {
    VLOGF(1) << "Reached maximum number of decoder instances ("
             << kMaxNumOfInstances << ")";
    return StatusCode::kDecoderCreationFailed;
  }

  constexpr bool kStateful = false;
  constexpr bool kStateless = true;
  absl::optional<std::pair<bool, uint32_t>> api_and_format;
  // Try both kStateful and kStateless APIs via |fourcc| and select the first
  // combination where Open()ing the |device_| works.
  for (const auto api : {kStateful, kStateless}) {
    const auto fourcc =
        V4L2Device::VideoCodecProfileToV4L2PixFmt(profile_, api);
    constexpr uint32_t kInvalidV4L2PixFmt = 0;
    if (fourcc == kInvalidV4L2PixFmt ||
        !device_->Open(V4L2Device::Type::kDecoder, fourcc)) {
      continue;
    }
    api_and_format = std::make_pair(api, fourcc);
    break;
  }

  if (!api_and_format.has_value()) {
    num_instances_.Decrement();
    can_use_decoder_ = false;
    VLOGF(1) << "No V4L2 API found for profile: " << GetProfileName(profile_);
    return StatusCode::kV4l2NoDecoder;
  }

  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) ||
      (caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP, "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    return StatusCode::kV4l2FailedFileCapabilitiesCheck;
  }

  // Create Input/Output V4L2Queue
  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to create V4L2 queue.";
    return StatusCode::kV4l2FailedResourceAllocation;
  }

  const auto preferred_api_and_format = api_and_format.value();
  const uint32_t input_format_fourcc = preferred_api_and_format.second;
  if (preferred_api_and_format.first == kStateful) {
    VLOGF(1) << "Using a stateful API for profile: " << GetProfileName(profile_)
             << " and fourcc: " << FourccToString(input_format_fourcc);
    backend_ = std::make_unique<V4L2StatefulVideoDecoderBackend>(
        this, device_, profile_, decoder_task_runner_);
  } else {
    DCHECK_EQ(preferred_api_and_format.first, kStateless);
    VLOGF(1) << "Using a stateless API for profile: "
             << GetProfileName(profile_)
             << " and fourcc: " << FourccToString(input_format_fourcc);
    backend_ = std::make_unique<V4L2StatelessVideoDecoderBackend>(
        this, device_, profile_, decoder_task_runner_);
  }

  if (!backend_->Initialize()) {
    VLOGF(1) << "Failed to initialize backend.";
    return StatusCode::kV4l2FailedResourceAllocation;
  }

  if (!SetupInputFormat(input_format_fourcc)) {
    VLOGF(1) << "Failed to setup input format.";
    return StatusCode::kV4l2BadFormat;
  }

  if (input_queue_->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP) == 0) {
    VLOGF(1) << "Failed to allocate input buffer.";
    return StatusCode::kV4l2FailedResourceAllocation;
  }

  // Start streaming input queue and polling. This is required for the stateful
  // decoder, and doesn't hurt for the stateless one.
  if (!StartStreamV4L2Queue(false)) {
    VLOGF(1) << "Failed to start streaming.";
    return StatusCode::kV4L2FailedToStartStreamQueue;
  }

  SetState(State::kDecoding);
  return StatusCode::kOk;
}

bool V4L2VideoDecoder::SetupInputFormat(uint32_t input_format_fourcc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kInitialized);

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

bool V4L2VideoDecoder::SetupOutputFormat(const gfx::Size& size,
                                         const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "size: " << size.ToString()
            << ", visible_rect: " << visible_rect.ToString();

  // Get the supported output formats and their corresponding negotiated sizes.
  std::vector<std::pair<Fourcc, gfx::Size>> candidates;
  for (const uint32_t& pixfmt : device_->EnumerateSupportedPixelformats(
           V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
    const auto candidate = Fourcc::FromV4L2PixFmt(pixfmt);
    if (!candidate) {
      DVLOGF(1) << "Pixel format " << FourccToString(pixfmt)
                << " is not supported, skipping...";
      continue;
    }

    absl::optional<struct v4l2_format> format =
        output_queue_->TryFormat(pixfmt, size, 0);
    if (!format)
      continue;

    gfx::Size adjusted_size(format->fmt.pix_mp.width,
                            format->fmt.pix_mp.height);
    candidates.emplace_back(*candidate, adjusted_size);
  }

  // Ask the pipeline to pick the output format.
  StatusOr<std::pair<Fourcc, gfx::Size>> status_or_output_format =
      client_->PickDecoderOutputFormat(
          candidates, visible_rect, aspect_ratio_.GetNaturalSize(visible_rect),
          /*output_size=*/absl::nullopt, num_output_frames_,
          /*use+protected=*/false, /*need_aux_frame_pool=*/false);
  if (status_or_output_format.has_error()) {
    VLOGF(1) << "Failed to pick an output format.";
    return false;
  }
  const auto output_format = std::move(status_or_output_format).value();
  Fourcc fourcc = std::move(output_format.first);
  gfx::Size picked_size = std::move(output_format.second);

  // We successfully picked the output format. Now setup output format again.
  absl::optional<struct v4l2_format> format =
      output_queue_->SetFormat(fourcc.ToV4L2PixFmt(), picked_size, 0);
  DCHECK(format);
  gfx::Size adjusted_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);
  DCHECK_EQ(adjusted_size.width() % 16, 0);
  DCHECK_EQ(adjusted_size.height() % 16, 0);
  if (!gfx::Rect(adjusted_size).Contains(gfx::Rect(picked_size))) {
    VLOGF(1) << "The adjusted coded size (" << adjusted_size.ToString()
             << ") should contains the original coded size("
             << picked_size.ToString() << ").";
    return false;
  }

  // Got the adjusted size from the V4L2 driver. Now setup the frame pool.
  // TODO(akahuang): It is possible there is an allocatable formats among
  // candidates, but PickDecoderOutputFormat() selects other non-allocatable
  // format. The correct flow is to attach an info to candidates if it is
  // created by VideoFramePool.
  DmabufVideoFramePool* pool = client_->GetVideoFramePool();
  if (pool) {
    // TODO(andrescj): the call to PickDecoderOutputFormat() should have already
    // initialized the frame pool, so this call to Initialize() is redundant.
    // However, we still have to get the GpuBufferLayout to find out the
    // modifier that we need to give to the driver. We should add a
    // GetGpuBufferLayout() method to DmabufVideoFramePool to query that without
    // having to re-initialize the pool.
    StatusOr<GpuBufferLayout> status_or_layout = pool->Initialize(
        fourcc, adjusted_size, visible_rect,
        aspect_ratio_.GetNaturalSize(visible_rect), num_output_frames_,
        /*use_protected=*/false);
    if (status_or_layout.has_error()) {
      VLOGF(1) << "Failed to setup format to VFPool";
      return false;
    }
    const GpuBufferLayout layout = std::move(status_or_layout).value();
    if (layout.size() != adjusted_size) {
      VLOGF(1) << "The size adjusted by VFPool is different from one "
               << "adjusted by a video driver. fourcc: " << fourcc.ToString()
               << ", (video driver v.s. VFPool) " << adjusted_size.ToString()
               << " != " << layout.size().ToString();
      return false;
    }

    VLOGF(1) << "buffer modifier: " << std::hex << layout.modifier();
    if (layout.modifier() &&
        layout.modifier() != gfx::NativePixmapHandle::kNoModifier) {
      absl::optional<struct v4l2_format> modifier_format =
          output_queue_->SetModifierFormat(layout.modifier(), picked_size);
      if (!modifier_format)
        return false;

      gfx::Size size_for_modifier_format(format->fmt.pix_mp.width,
                                         format->fmt.pix_mp.height);
      if (size_for_modifier_format != adjusted_size) {
        VLOGF(1)
            << "Buffers were allocated for " << adjusted_size.ToString()
            << " but modifier format is expecting buffers to be allocated for "
            << size_for_modifier_format.ToString();
        return false;
      }
    }
  }

  return true;
}

void V4L2VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // In order to preserve the order of the callbacks between Decode() and
  // Reset(), we also trampoline the callback of Reset().
  auto trampoline_reset_cb = base::BindOnce(
      &base::SequencedTaskRunner::PostTask,
      base::SequencedTaskRunnerHandle::Get(), FROM_HERE, std::move(closure));

  // Reset callback for resolution change, because the pipeline won't notify
  // flushed after reset.
  continue_change_resolution_cb_.Reset();

  if (state_ == State::kInitialized) {
    std::move(trampoline_reset_cb).Run();
    return;
  }

  // Call all pending decode callback.
  backend_->ClearPendingRequests(DecodeStatus::ABORTED);

  // Streamoff V4L2 queues to drop input and output buffers.
  // If the queues are streaming before reset, then we need to start streaming
  // them after stopping.
  const bool is_input_streaming = input_queue_->IsStreaming();
  const bool is_output_streaming = output_queue_->IsStreaming();
  if (!StopStreamV4L2Queue(true))
    return;

  if (is_input_streaming) {
    if (!StartStreamV4L2Queue(is_output_streaming))
      return;
  }

  // If during flushing, Reset() will abort the following flush tasks.
  // Now we are ready to decode new buffer. Go back to decoding state.
  SetState(State::kDecoding);

  std::move(trampoline_reset_cb).Run();
}

void V4L2VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_NE(state_, State::kUninitialized);

  // VideoDecoder interface: |decode_cb| can't be called from within Decode().
  auto trampoline_decode_cb = base::BindOnce(
      [](const scoped_refptr<base::SequencedTaskRunner>& this_sequence_runner,
         DecodeCB decode_cb, Status status) {
        this_sequence_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(decode_cb), status));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(decode_cb));

  if (state_ == State::kError) {
    std::move(trampoline_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (state_ == State::kInitialized) {
    const StatusCode status = InitializeBackend();
    if (status != StatusCode::kOk) {
      SetState(State::kError);
      std::move(trampoline_decode_cb).Run(status);
      return;
    }
  }

  const int32_t bitstream_id = bitstream_id_generator_.GetNextBitstreamId();
  backend_->EnqueueDecodeTask(std::move(buffer),
                              std::move(trampoline_decode_cb), bitstream_id);
}

bool V4L2VideoDecoder::StartStreamV4L2Queue(bool start_output_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!input_queue_->Streamon() ||
      (start_output_queue && !output_queue_->Streamon())) {
    VLOGF(1) << "Failed to streamon V4L2 queue.";
    SetState(State::kError);
    return false;
  }

  if (!device_->StartPolling(
          base::BindRepeating(&V4L2VideoDecoder::ServiceDeviceTask, weak_this_),
          base::BindRepeating(&V4L2VideoDecoder::SetState, weak_this_,
                              State::kError))) {
    SetState(State::kError);
    return false;
  }

  return true;
}

bool V4L2VideoDecoder::StopStreamV4L2Queue(bool stop_input_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!device_->StopPolling()) {
    SetState(State::kError);
    return false;
  }

  // Streamoff input and output queue.
  if (input_queue_ && stop_input_queue)
    input_queue_->Streamoff();
  if (output_queue_)
    output_queue_->Streamoff();

  if (backend_)
    backend_->OnStreamStopped(stop_input_queue);

  return true;
}

void V4L2VideoDecoder::InitiateFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  SetState(State::kFlushing);
}

void V4L2VideoDecoder::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  SetState(State::kDecoding);
}

void V4L2VideoDecoder::ChangeResolution(gfx::Size pic_size,
                                        gfx::Rect visible_rect,
                                        size_t num_output_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK(!continue_change_resolution_cb_);

  // After the pipeline flushes all frames, we can start changing resolution.
  continue_change_resolution_cb_ =
      base::BindOnce(&V4L2VideoDecoder::ContinueChangeResolution, weak_this_,
                     pic_size, visible_rect, num_output_frames);

  DCHECK(client_);
  client_->PrepareChangeResolution();
}

void V4L2VideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK(continue_change_resolution_cb_);

  std::move(continue_change_resolution_cb_).Run();
}

void V4L2VideoDecoder::ContinueChangeResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect,
    const size_t num_output_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // If we already reset, then skip it.
  if (state_ == State::kDecoding)
    return;
  DCHECK_EQ(state_, State::kFlushing);

  // Notify |backend_| that changing resolution fails.
  // Note: |backend_| is owned by this, using base::Unretained() is safe.
  base::ScopedClosureRunner done_caller(
      base::BindOnce(&V4L2VideoDecoderBackend::OnChangeResolutionDone,
                     base::Unretained(backend_.get()), false));

  DCHECK_GT(num_output_frames, 0u);
  num_output_frames_ = num_output_frames + kDpbOutputBufferExtraCount;

  // Stateful decoders require the input queue to keep running during resolution
  // changes, but stateless ones require it to be stopped.
  if (!StopStreamV4L2Queue(backend_->StopInputQueueOnResChange()))
    return;

  if (!output_queue_->DeallocateBuffers()) {
    SetState(State::kError);
    return;
  }

  if (!backend_->ApplyResolution(pic_size, visible_rect, num_output_frames_)) {
    SetState(State::kError);
    return;
  }

  if (!SetupOutputFormat(pic_size, visible_rect)) {
    VLOGF(1) << "Failed to setup output format.";
    SetState(State::kError);
    return;
  }

  const v4l2_memory type =
      client_->GetVideoFramePool() ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
  const size_t v4l2_num_buffers =
      (type == V4L2_MEMORY_DMABUF) ? VIDEO_MAX_FRAME : num_output_frames_;

  if (output_queue_->AllocateBuffers(v4l2_num_buffers, type) == 0) {
    VLOGF(1) << "Failed to request output buffers.";
    SetState(State::kError);
    return;
  }
  if (output_queue_->AllocatedBuffersCount() < num_output_frames_) {
    VLOGF(1) << "Could not allocate requested number of output buffers.";
    SetState(State::kError);
    return;
  }

  if (!StartStreamV4L2Queue(true)) {
    SetState(State::kError);
    return;
  }

  // Now notify |backend_| that changing resolution is done successfully.
  // Note: |backend_| is owned by this, using base::Unretained() is safe.
  done_caller.ReplaceClosure(
      base::BindOnce(&V4L2VideoDecoderBackend::OnChangeResolutionDone,
                     base::Unretained(backend_.get()), true));
}

void V4L2VideoDecoder::ServiceDeviceTask(bool event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (input_queue_ && output_queue_) {
    DVLOGF(3) << "Number of queued input buffers: "
              << input_queue_->QueuedBuffersCount()
              << ", Number of queued output buffers: "
              << output_queue_->QueuedBuffersCount();
    TRACE_COUNTER_ID2(
        "media,gpu", "V4L2 queue sizes", this, "input (OUTPUT_queue)",
        input_queue_->QueuedBuffersCount(), "output (CAPTURE_queue)",
        output_queue_->QueuedBuffersCount());
  }

  if (backend_)
    backend_->OnServiceDeviceTask(event);

  // Dequeue V4L2 output buffer first to reduce output latency.
  bool success;
  while (output_queue_ && output_queue_->QueuedBuffersCount() > 0) {
    V4L2ReadableBufferRef dequeued_buffer;

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
  while (input_queue_ && input_queue_->QueuedBuffersCount() > 0) {
    V4L2ReadableBufferRef dequeued_buffer;

    std::tie(success, dequeued_buffer) = input_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;
  }
}

void V4L2VideoDecoder::OutputFrame(scoped_refptr<VideoFrame> frame,
                                   const gfx::Rect& visible_rect,
                                   base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "timestamp: " << timestamp.InMilliseconds() << " msec";

  // Set the timestamp at which the decode operation started on the
  // |frame|. If the frame has been outputted before (e.g. because of VP9
  // show-existing-frame feature) we can't overwrite the timestamp directly, as
  // the original frame might still be in use. Instead we wrap the frame in
  // another frame with a different timestamp.
  if (frame->timestamp().is_zero())
    frame->set_timestamp(timestamp);

  if (frame->visible_rect() != visible_rect ||
      frame->timestamp() != timestamp) {
    gfx::Size natural_size = aspect_ratio_.GetNaturalSize(visible_rect);
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        frame, frame->format(), visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);

    frame = std::move(wrapped_frame);
  }

  output_cb_.Run(std::move(frame));
}

DmabufVideoFramePool* V4L2VideoDecoder::GetVideoFramePool() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  return client_->GetVideoFramePool();
}

void V4L2VideoDecoder::SetState(State new_state) {
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
      VLOGF(1) << "Should not set to kUninitialized.";
      new_state = State::kError;
      break;

    case State::kInitialized:
      if ((state_ != State::kUninitialized) && (state_ != State::kDecoding)) {
        VLOGF(1) << "Can only transition to kInitialized from kUninitialized "
                    "or kDecoding";
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
    VLOGF(1) << "Error occurred, stopping queues.";
    StopStreamV4L2Queue(true);
    if (backend_)
      backend_->ClearPendingRequests(DecodeStatus::DECODE_ERROR);
    return;
  }
  state_ = new_state;
  return;
}

void V4L2VideoDecoder::OnBackendError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  SetState(State::kError);
}

bool V4L2VideoDecoder::IsDecoding() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  return state_ == State::kDecoding;
}

}  // namespace media
