// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <limits>
#include <vector>

#include "base/bind_post_task.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/video/gpu_video_accelerator_factories.h"

namespace media {

namespace {

// HW encoders expect a nonzero bitrate, so |kVEADefaultBitratePerPixel| is used
// to estimate bits per second for ~30 fps with ~1/16 compression rate.
constexpr int kVEADefaultBitratePerPixel = 2;

VideoEncodeAccelerator::Config SetUpVeaConfig(
    VideoCodecProfile profile,
    const VideoEncoder::Options& opts,
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type) {
  base::Optional<uint32_t> initial_framerate;
  if (opts.framerate.has_value())
    initial_framerate = static_cast<uint32_t>(opts.framerate.value());

  auto config = VideoEncodeAccelerator::Config(
      format, opts.frame_size, profile,
      opts.bitrate.value_or(opts.frame_size.width() * opts.frame_size.height() *
                            kVEADefaultBitratePerPixel),
      initial_framerate);

  const bool is_rgb =
      format == PIXEL_FORMAT_XBGR || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_ARGB;

  // Override the provided format if incoming frames are RGB -- they'll be
  // converted to I420 or NV12 depending on the VEA configuration.
  if (is_rgb)
    config.input_format = PIXEL_FORMAT_I420;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (storage_type == VideoFrame::STORAGE_DMABUFS ||
      storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    if (is_rgb)
      config.input_format = PIXEL_FORMAT_NV12;
    config.storage_type =
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  }
#endif

  return config;
}

}  // namespace

VideoEncodeAcceleratorAdapter::PendingOp::PendingOp() = default;
VideoEncodeAcceleratorAdapter::PendingOp::~PendingOp() = default;
VideoEncodeAcceleratorAdapter::PendingEncode::PendingEncode() = default;
VideoEncodeAcceleratorAdapter::PendingEncode::~PendingEncode() = default;

VideoEncodeAcceleratorAdapter::VideoEncodeAcceleratorAdapter(
    GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : output_pool_(base::MakeRefCounted<SharedMemoryPool>()),
      input_pool_(base::MakeRefCounted<SharedMemoryPool>()),
      gpu_factories_(gpu_factories),
      accelerator_task_runner_(gpu_factories_->GetTaskRunner()),
      callback_task_runner_(std::move(callback_task_runner)) {
  DETACH_FROM_SEQUENCE(accelerator_sequence_checker_);
}

VideoEncodeAcceleratorAdapter::~VideoEncodeAcceleratorAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  input_pool_->Shutdown();
  output_pool_->Shutdown();
}

void VideoEncodeAcceleratorAdapter::DestroyAsync(
    std::unique_ptr<VideoEncodeAcceleratorAdapter> self) {
  DCHECK(self);
  auto runner = self->accelerator_task_runner_;
  DCHECK(runner);
  if (!runner->RunsTasksInCurrentSequence())
    runner->DeleteSoon(FROM_HERE, std::move(self));
}

void VideoEncodeAcceleratorAdapter::SetInputBufferPreferenceForTesting(
    InputBufferKind pref) {
  input_buffer_preference_ = pref;
}

void VideoEncodeAcceleratorAdapter::Initialize(VideoCodecProfile profile,
                                               const Options& options,
                                               OutputCB output_cb,
                                               StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoEncodeAcceleratorAdapter::InitializeOnAcceleratorThread,
          base::Unretained(this), profile, options,
          WrapCallback(std::move(output_cb)),
          WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::InitializeOnAcceleratorThread(
    VideoCodecProfile profile,
    const Options& options,
    OutputCB output_cb,
    StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (state_ != State::kNotInitialized) {
    auto status = Status(StatusCode::kEncoderInitializeTwice,
                         "Encoder has already been initialized.");
    std::move(done_cb).Run(status);
    return;
  }

  accelerator_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!accelerator_) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Failed to create video encode accelerator.");
    std::move(done_cb).Run(status);
    return;
  }

  if (options.frame_size.width() <= 0 || options.frame_size.height() <= 0) {
    auto status = Status(StatusCode::kEncoderUnsupportedConfig,
                         "Negative width or height values.");
    std::move(done_cb).Run(status);
    return;
  }

  if (!options.frame_size.GetCheckedArea().IsValid()) {
    auto status =
        Status(StatusCode::kEncoderUnsupportedConfig, "Frame is too large.");
    std::move(done_cb).Run(status);
    return;
  }

  profile_ = profile;
  options_ = options;
  output_cb_ = std::move(output_cb);
  state_ = State::kWaitingForFirstFrame;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX &&
      !options_.avc.produce_annexb) {
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  std::move(done_cb).Run(Status());

  // The accelerator will be initialized for real once we have the first frame.
}

void VideoEncodeAcceleratorAdapter::InitializeInternalOnAcceleratorThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForFirstFrame);
  DCHECK(!pending_encodes_.empty());

  // We use the first frame to setup the VEA config so that we can ensure that
  // zero copy hardware encoding from the camera can be used.
  const auto& first_frame = pending_encodes_.front()->frame;
  const auto format = first_frame->format();
  const bool is_rgb =
      format == PIXEL_FORMAT_XBGR || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_ARGB;
  const bool supported_format =
      format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_I420 || is_rgb;
  if (!supported_format) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode, "Unexpected frame format.")
            .WithData("frame", first_frame->AsHumanReadableString());
    InitCompleted(std::move(status));
    return;
  }

  auto vea_config =
      SetUpVeaConfig(profile_, options_, format, first_frame->storage_type());

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Linux/ChromeOS require a special configuration to use dmabuf storage.
  // We need to keep sending frames the same way the first frame was sent.
  // Other platforms will happily mix GpuMemoryBuffer storage with regular
  // storage, so we don't care about mismatches on other platforms.
  if (input_buffer_preference_ == InputBufferKind::Any) {
    if (vea_config.storage_type ==
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer) {
      input_buffer_preference_ = InputBufferKind::GpuMemBuf;
    } else {
      input_buffer_preference_ = InputBufferKind::CpuMemBuf;
    }
  }
#endif

  if (!accelerator_->Initialize(vea_config, this)) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Failed to initialize video encode accelerator.");
    InitCompleted(status);
    return;
  }

  state_ = State::kInitializing;
  format_ = vea_config.input_format;
}

void VideoEncodeAcceleratorAdapter::Encode(scoped_refptr<VideoFrame> frame,
                                           bool key_frame,
                                           StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAcceleratorAdapter::EncodeOnAcceleratorThread,
                     base::Unretained(this), std::move(frame), key_frame,
                     WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::EncodeOnAcceleratorThread(
    scoped_refptr<VideoFrame> frame,
    bool key_frame,
    StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);

  if (state_ == State::kWaitingForFirstFrame ||
      state_ == State::kInitializing) {
    auto pending_encode = std::make_unique<PendingEncode>();
    pending_encode->done_callback = std::move(done_cb);
    pending_encode->frame = std::move(frame);
    pending_encode->key_frame = key_frame;
    pending_encodes_.push_back(std::move(pending_encode));
    if (state_ == State::kWaitingForFirstFrame)
      InitializeInternalOnAcceleratorThread();
    return;
  }

  if (state_ != State::kReadyToEncode) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode, "Encoder can't encode now.");
    std::move(done_cb).Run(status);
    return;
  }

  const bool frame_needs_resizing =
      frame->visible_rect().size() != options_.frame_size;

  // Try using a frame with GPU buffer both are true:
  // 1. the frame already has GPU buffer
  // 2. frame doesn't need resizing or can be resized by GPU encoder.
  bool use_gpu_buffer = frame->HasGpuMemoryBuffer() &&
                        (!frame_needs_resizing || gpu_resize_supported_);

  // Currently configured encoder's preference takes precedence overe heuristic
  // above.
  if (input_buffer_preference_ == InputBufferKind::GpuMemBuf)
    use_gpu_buffer = true;
  if (input_buffer_preference_ == InputBufferKind::CpuMemBuf)
    use_gpu_buffer = false;

  StatusOr<scoped_refptr<VideoFrame>> result(nullptr);
  if (use_gpu_buffer)
    result = PrepareGpuFrame(options_.frame_size, frame);
  else
    result = PrepareCpuFrame(options_.frame_size, frame);

  if (result.has_error()) {
    auto status = std::move(result).error();
    status.WithData("frame", frame->AsHumanReadableString());
    std::move(done_cb).Run(std::move(status).AddHere());
    return;
  }

  frame = std::move(result).value();

  auto active_encode = std::make_unique<PendingOp>();
  active_encode->done_callback = std::move(done_cb);
  active_encode->timestamp = frame->timestamp();
  active_encodes_.push_back(std::move(active_encode));
  accelerator_->Encode(frame, key_frame);
}

void VideoEncodeAcceleratorAdapter::ChangeOptions(const Options& options,
                                                  OutputCB output_cb,
                                                  StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoEncodeAcceleratorAdapter::ChangeOptionsOnAcceleratorThread,
          base::Unretained(this), options, WrapCallback(std::move(output_cb)),
          WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::ChangeOptionsOnAcceleratorThread(
    const Options options,
    OutputCB output_cb,
    StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK(active_encodes_.empty());
  DCHECK(pending_encodes_.empty());
  DCHECK_EQ(state_, State::kReadyToEncode);

  if (options.frame_size != options_.frame_size) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Resolution change is not supported.");
    std::move(done_cb).Run(status);
    return;
  }

  uint32_t bitrate =
      std::min(options.bitrate.value_or(options.frame_size.width() *
                                        options.frame_size.height() *
                                        kVEADefaultBitratePerPixel),
               uint64_t{std::numeric_limits<uint32_t>::max()});

  uint32_t framerate = uint32_t{std::round(
      options.framerate.value_or(VideoEncodeAccelerator::kDefaultFramerate))};

  accelerator_->RequestEncodingParametersChange(bitrate, framerate);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    if (options.avc.produce_annexb) {
      h264_converter_.reset();
    } else if (!h264_converter_) {
      h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
    }
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  options_ = options;
  if (!output_cb.is_null())
    output_cb_ = std::move(output_cb);
  std::move(done_cb).Run(Status());
}

void VideoEncodeAcceleratorAdapter::Flush(StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread,
                     base::Unretained(this), WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread(StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (state_ == State::kWaitingForFirstFrame) {
    // Nothing to do since we haven't actually initialized yet.
    std::move(done_cb).Run(Status());
    return;
  }

  if (state_ != State::kReadyToEncode && state_ != State::kInitializing) {
    auto status =
        Status(StatusCode::kEncoderFailedFlush, "Encoder can't flush now");
    std::move(done_cb).Run(status);
    return;
  }

  if (active_encodes_.empty() && pending_encodes_.empty()) {
    // No active or pending encodes, nothing to flush.
    std::move(done_cb).Run(Status());
    return;
  }

  // When initializing the flush will be handled after pending encodes are sent.
  if (state_ != State::kInitializing) {
    DCHECK_EQ(state_, State::kReadyToEncode);
    state_ = State::kFlushing;
  }

  pending_flush_ = std::make_unique<PendingOp>();
  pending_flush_->done_callback = std::move(done_cb);

  // If flush is not supported FlushCompleted() will be called by
  // BitstreamBufferReady() when |active_encodes_| is empty.
  if (state_ == State::kFlushing && flush_support_.value()) {
    accelerator_->Flush(
        base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushCompleted,
                       base::Unretained(this)));
  }
}

void VideoEncodeAcceleratorAdapter::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);

  input_buffer_size_ =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, input_coded_size);

  output_handle_holder_ = output_pool_->MaybeAllocateBuffer(output_buffer_size);

  if (!output_handle_holder_) {
    InitCompleted(Status(StatusCode::kEncoderInitializationError));
    return;
  }

  base::UnsafeSharedMemoryRegion* region = output_handle_holder_->GetRegion();
  // There is always one output buffer.
  accelerator_->UseOutputBitstreamBuffer(
      BitstreamBuffer(0, region->Duplicate(), region->GetSize()));
  InitCompleted(Status());
}

void VideoEncodeAcceleratorAdapter::BitstreamBufferReady(
    int32_t buffer_id,
    const BitstreamBufferMetadata& metadata) {
  base::Optional<CodecDescription> desc;
  VideoEncoderOutput result;
  result.key_frame = metadata.key_frame;
  result.timestamp = metadata.timestamp;
  result.size = metadata.payload_size_bytes;

  DCHECK_EQ(buffer_id, 0);
  // There is always one output buffer.
  base::WritableSharedMemoryMapping* mapping =
      output_handle_holder_->GetMapping();
  DCHECK_LE(result.size, mapping->size());

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_) {
    uint8_t* src = static_cast<uint8_t*>(mapping->memory());
    size_t dst_size = result.size;
    size_t actual_output_size = 0;
    bool config_changed = false;
    std::unique_ptr<uint8_t[]> dst(new uint8_t[dst_size]);

    auto status =
        h264_converter_->ConvertChunk(base::span<uint8_t>(src, result.size),
                                      base::span<uint8_t>(dst.get(), dst_size),
                                      &config_changed, &actual_output_size);
    if (status.code() == StatusCode::kH264BufferTooSmall) {
      // Between AnnexB and AVCC bitstream formats, the start code length and
      // the nal size length can be different. See H.264 specification at
      // http://www.itu.int/rec/T-REC-H.264. Retry the conversion if the output
      // buffer size is too small.
      dst_size = actual_output_size;
      dst.reset(new uint8_t[dst_size]);
      status = h264_converter_->ConvertChunk(
          base::span<uint8_t>(src, result.size),
          base::span<uint8_t>(dst.get(), dst_size), &config_changed,
          &actual_output_size);
    }

    if (!status.is_ok()) {
      LOG(ERROR) << status.message();
      NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    result.size = actual_output_size;
    result.data = std::move(dst);

    if (config_changed) {
      const auto& config = h264_converter_->GetCurrentConfig();
      desc = CodecDescription();
      if (!config.Serialize(desc.value())) {
        NotifyError(VideoEncodeAccelerator::kPlatformFailureError);
        return;
      }
    }
  } else {
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    result.data.reset(new uint8_t[result.size]);
    memcpy(result.data.get(), mapping->memory(), result.size);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Give the buffer back to |accelerator_|
  base::UnsafeSharedMemoryRegion* region = output_handle_holder_->GetRegion();
  accelerator_->UseOutputBitstreamBuffer(
      BitstreamBuffer(buffer_id, region->Duplicate(), region->GetSize()));

  for (auto it = active_encodes_.begin(); it != active_encodes_.end(); ++it) {
    if ((*it)->timestamp == result.timestamp) {
      std::move((*it)->done_callback).Run(Status());
      active_encodes_.erase(it);
      break;
    }
  }
  output_cb_.Run(std::move(result), std::move(desc));
  if (active_encodes_.empty() && !flush_support_.value()) {
    // Manually call FlushCompleted(), since |accelerator_| won't do it for us.
    FlushCompleted(true);
  }
}

void VideoEncodeAcceleratorAdapter::NotifyError(
    VideoEncodeAccelerator::Error error) {
  if (state_ == State::kInitializing) {
    InitCompleted(
        Status(StatusCode::kEncoderInitializationError,
               "VideoEncodeAccelerator encountered an error")
            .WithData("VideoEncodeAccelerator::Error", int32_t{error}));
    return;
  }

  if (state_ == State::kFlushing)
    FlushCompleted(false);

  // Report the error to all encoding-done callbacks
  for (auto& encode : active_encodes_) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode,
               "VideoEncodeAccelerator encountered an error")
            .WithData("VideoEncodeAccelerator::Error", int32_t{error});
    std::move(encode->done_callback).Run(Status());
  }
  active_encodes_.clear();
  state_ = State::kNotInitialized;
}

void VideoEncodeAcceleratorAdapter::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {}

void VideoEncodeAcceleratorAdapter::InitCompleted(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);

  if (!status.is_ok()) {
    // Report the error to all encoding-done callbacks
    for (auto& encode : pending_encodes_)
      std::move(encode->done_callback).Run(status);

    if (pending_flush_)
      FlushCompleted(false);

    DCHECK(active_encodes_.empty());
    pending_encodes_.clear();
    state_ = State::kNotInitialized;
    return;
  }

  state_ = State::kReadyToEncode;
  flush_support_ = accelerator_->IsFlushSupported();
  gpu_resize_supported_ = accelerator_->IsGpuFrameResizeSupported();

  // Send off the encodes that came in while we were waiting for initialization.
  for (auto& encode : pending_encodes_) {
    EncodeOnAcceleratorThread(std::move(encode->frame), encode->key_frame,
                              std::move(encode->done_callback));
  }
  pending_encodes_.clear();

  // If a Flush() came in during initialization, transition to flushing now that
  // all the pending encodes have been sent.
  if (pending_flush_) {
    state_ = State::kFlushing;
    if (flush_support_.value()) {
      accelerator_->Flush(
          base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushCompleted,
                         base::Unretained(this)));
    }
  }
}

void VideoEncodeAcceleratorAdapter::FlushCompleted(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (!pending_flush_)
    return;

  auto status = success ? Status() : Status(StatusCode::kEncoderFailedFlush);
  std::move(pending_flush_->done_callback).Run(status);
  pending_flush_.reset();
  state_ = State::kReadyToEncode;
}

template <class T>
T VideoEncodeAcceleratorAdapter::WrapCallback(T cb) {
  DCHECK(callback_task_runner_);
  if (cb.is_null())
    return cb;
  return base::BindPostTask(callback_task_runner_, std::move(cb));
}

// Copy a frame into a shared mem buffer and resize it as the same time. Input
// frames can I420, NV12, or RGB -- they'll be converted to I420 if needed.
StatusOr<scoped_refptr<VideoFrame>>
VideoEncodeAcceleratorAdapter::PrepareCpuFrame(
    const gfx::Size& size,
    scoped_refptr<VideoFrame> src_frame) {
  auto handle = input_pool_->MaybeAllocateBuffer(input_buffer_size_);
  if (!handle)
    return Status(StatusCode::kEncoderFailedEncode);

  base::UnsafeSharedMemoryRegion* region = handle->GetRegion();
  base::WritableSharedMemoryMapping* mapping = handle->GetMapping();

  auto mapped_src_frame = src_frame->HasGpuMemoryBuffer()
                              ? ConvertToMemoryMappedFrame(src_frame)
                              : src_frame;
  auto shared_frame = VideoFrame::WrapExternalData(
      PIXEL_FORMAT_I420, options_.frame_size, gfx::Rect(size), size,
      mapping->GetMemoryAsSpan<uint8_t>().data(), mapping->size(),
      src_frame->timestamp());

  if (!shared_frame || !mapped_src_frame)
    return Status(StatusCode::kEncoderFailedEncode);

  shared_frame->BackWithSharedMemory(region);
  // Keep the SharedMemoryHolder until the frame is destroyed so that the
  // memory is not freed prematurely.
  shared_frame->AddDestructionObserver(BindToCurrentLoop(base::BindOnce(
      base::DoNothing::Once<
          std::unique_ptr<SharedMemoryPool::SharedMemoryHandle>>(),
      std::move(handle))));
  auto status =
      ConvertAndScaleFrame(*mapped_src_frame, *shared_frame, resize_buf_);
  if (!status.is_ok())
    return std::move(status).AddHere();

  return shared_frame;
}

// Copy a frame into a GPU buffer and resize it as the same time. Input frames
// can I420, NV12, or RGB -- they'll be converted to NV12 if needed.
StatusOr<scoped_refptr<VideoFrame>>
VideoEncodeAcceleratorAdapter::PrepareGpuFrame(
    const gfx::Size& size,
    scoped_refptr<VideoFrame> src_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK(src_frame);
  if (src_frame->HasGpuMemoryBuffer() &&
      src_frame->format() == PIXEL_FORMAT_NV12 &&
      (gpu_resize_supported_ || src_frame->visible_rect().size() == size)) {
    // Nothing to do here, the input frame is already what we need
    return src_frame;
  }

  auto gmb = gpu_factories_->CreateGpuMemoryBuffer(
      size, gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);

  if (!gmb)
    return Status(StatusCode::kEncoderFailedEncode);
  gmb->SetColorSpace(src_frame->ColorSpace());

  gpu::MailboxHolder empty_mailboxes[media::VideoFrame::kMaxPlanes];
  auto gpu_frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(size), size, std::move(gmb), empty_mailboxes,
      base::NullCallback(), src_frame->timestamp());
  gpu_frame->set_color_space(src_frame->ColorSpace());
  gpu_frame->metadata().MergeMetadataFrom(src_frame->metadata());

  // Don't be scared. ConvertToMemoryMappedFrame() doesn't copy pixel data
  // it just maps GPU buffer owned by |gpu_frame| and presents it as mapped
  // view in CPU memory. It allows us to use ConvertAndScaleFrame() without
  // having to tinker with libyuv and GpuMemoryBuffer memory views.
  // |mapped_gpu_frame| doesn't own anything, but unmaps the buffer when freed.
  auto mapped_gpu_frame = ConvertToMemoryMappedFrame(gpu_frame);
  auto mapped_src_frame = src_frame->HasGpuMemoryBuffer()
                              ? ConvertToMemoryMappedFrame(src_frame)
                              : src_frame;
  if (!mapped_gpu_frame || !mapped_src_frame)
    return Status(StatusCode::kEncoderFailedEncode);

  auto status =
      ConvertAndScaleFrame(*mapped_src_frame, *mapped_gpu_frame, resize_buf_);
  if (!status.is_ok())
    return std::move(status).AddHere();

  return gpu_frame;
}

}  // namespace media
