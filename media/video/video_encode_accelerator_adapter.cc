// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// HW encoders expect a nonzero bitrate, so |kVEADefaultBitratePerPixel| is used
// to estimate bits per second for ~30 fps with ~1/16 compression rate.
constexpr int kVEADefaultBitratePerPixel = 2;

Status SetUpVeaConfig(VideoCodecProfile profile,
                      const VideoEncoder::Options& opts,
                      VideoEncodeAccelerator::Config* config) {
  if (opts.width <= 0 || opts.height <= 0)
    return Status(StatusCode::kEncoderUnsupportedConfig,
                  "Negative width or height values");

  *config = VideoEncodeAccelerator::Config(
      PIXEL_FORMAT_I420, gfx::Size(opts.width, opts.height), profile,
      opts.bitrate.value_or(opts.width * opts.height *
                            kVEADefaultBitratePerPixel));
  return Status();
}

}  // namespace

class VideoEncodeAcceleratorAdapter::SharedMemoryPool
    : public base::RefCountedThreadSafe<
          VideoEncodeAcceleratorAdapter::SharedMemoryPool> {
 public:
  SharedMemoryPool(media::GpuVideoAcceleratorFactories* gpu_factories,
                   size_t region_size) {
    DCHECK(gpu_factories);
    gpu_factories_ = gpu_factories;
    region_size_ = region_size;
  }

  bool MaybeAllocateBuffer(int32_t* id) {
    if (!free_buffer_ids_.empty()) {
      *id = free_buffer_ids_.back();
      free_buffer_ids_.pop_back();
      return true;
    }

    if (!gpu_factories_)
      return false;

    base::UnsafeSharedMemoryRegion region =
        gpu_factories_->CreateSharedMemoryRegion(region_size_);
    if (!region.IsValid())
      return false;

    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid())
      return false;

    regions_.push_back(std::move(region));
    mappings_.push_back(std::move(mapping));
    if (regions_.size() >= std::numeric_limits<int32_t>::max() / 2) {
      // Suspiciously many buffers have been allocated already.
      return false;
    }
    *id = int32_t{regions_.size()} - 1;
    return true;
  }

  void ReleaseBuffer(int32_t id) { free_buffer_ids_.push_back(id); }

  base::WritableSharedMemoryMapping* GetMapping(int32_t buffer_id) {
    if (size_t{buffer_id} >= mappings_.size())
      return nullptr;
    return &mappings_[buffer_id];
  }

  base::UnsafeSharedMemoryRegion* GetRegion(int32_t buffer_id) {
    if (size_t{buffer_id} >= regions_.size())
      return nullptr;
    return &regions_[buffer_id];
  }

 private:
  friend class base::RefCountedThreadSafe<
      VideoEncodeAcceleratorAdapter::SharedMemoryPool>;
  ~SharedMemoryPool() = default;

  size_t region_size_;
  GpuVideoAcceleratorFactories* gpu_factories_;
  std::vector<base::UnsafeSharedMemoryRegion> regions_;
  std::vector<base::WritableSharedMemoryMapping> mappings_;
  std::vector<int32_t> free_buffer_ids_;
};

VideoEncodeAcceleratorAdapter::PendingOp::PendingOp() = default;
VideoEncodeAcceleratorAdapter::PendingOp::~PendingOp() = default;

VideoEncodeAcceleratorAdapter::VideoEncodeAcceleratorAdapter(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
    : gpu_factories_(gpu_factories),
      accelerator_task_runner_(gpu_factories_->GetTaskRunner()),
      callback_task_runner_(std::move(callback_task_runner)) {}

VideoEncodeAcceleratorAdapter::~VideoEncodeAcceleratorAdapter() {
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
}

void VideoEncodeAcceleratorAdapter::DestroyAsync(
    std::unique_ptr<VideoEncodeAcceleratorAdapter> self) {
  DCHECK(self);
  auto runner = self->accelerator_task_runner_;
  DCHECK(runner);
  if (!runner->BelongsToCurrentThread())
    runner->DeleteSoon(FROM_HERE, std::move(self));
}

void VideoEncodeAcceleratorAdapter::Initialize(VideoCodecProfile profile,
                                               const Options& options,
                                               OutputCB output_cb,
                                               StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->BelongsToCurrentThread());
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
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
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

  VideoEncodeAccelerator::Config vea_config;
  auto status = SetUpVeaConfig(profile, options, &vea_config);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  if (!accelerator_->Initialize(vea_config, this)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to initialize video encode accelerator.");
    std::move(done_cb).Run(status);
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX)
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  output_cb_ = std::move(output_cb);
  state_ = State::kInitializing;
  pending_init_ = std::make_unique<PendingOp>();
  pending_init_->done_callback = std::move(done_cb);
}

void VideoEncodeAcceleratorAdapter::Encode(scoped_refptr<VideoFrame> frame,
                                           bool key_frame,
                                           StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->BelongsToCurrentThread());
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
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
  if (state_ != State::kReadyToEncode) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode, "Encoder can't encode now.");
    std::move(done_cb).Run(status);
    return;
  }

  if (!frame->IsMappable() || frame->format() != media::PIXEL_FORMAT_I420) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode, "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("format", frame->format());
    std::move(done_cb).Run(std::move(status));
    return;
  }

  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    int32_t buffer_id;
    if (!input_pool_->MaybeAllocateBuffer(&buffer_id)) {
      auto status = Status(StatusCode::kEncoderFailedEncode,
                           "Can't allocate a shared input buffer");
      std::move(done_cb).Run(std::move(status));
      return;
    }

    base::UnsafeSharedMemoryRegion* region = input_pool_->GetRegion(buffer_id);
    base::WritableSharedMemoryMapping* mapping =
        input_pool_->GetMapping(buffer_id);

    auto shared_frame = VideoFrame::WrapExternalData(
        media::PIXEL_FORMAT_I420, frame->coded_size(), frame->visible_rect(),
        frame->natural_size(), mapping->GetMemoryAsSpan<uint8_t>().data(),
        mapping->size(), frame->timestamp());

    if (!shared_frame) {
      auto status = Status(StatusCode::kEncoderFailedEncode,
                           "Can't allocate a shared frame");
      std::move(done_cb).Run(std::move(status));
      return;
    }

    shared_frame->BackWithSharedMemory(region);
    shared_frame->AddDestructionObserver(
        media::BindToCurrentLoop(base::BindOnce(
            &SharedMemoryPool::ReleaseBuffer, input_pool_, buffer_id)));
    libyuv::I420Copy(frame->visible_data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->visible_data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->visible_data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     shared_frame->visible_data(media::VideoFrame::kYPlane),
                     shared_frame->stride(media::VideoFrame::kYPlane),
                     shared_frame->visible_data(media::VideoFrame::kUPlane),
                     shared_frame->stride(media::VideoFrame::kUPlane),
                     shared_frame->visible_data(media::VideoFrame::kVPlane),
                     shared_frame->stride(media::VideoFrame::kVPlane),
                     frame->visible_rect().width(),
                     frame->visible_rect().height());
    frame = std::move(shared_frame);
  }

  auto pending_encode = std::make_unique<PendingOp>();
  pending_encode->done_callback = std::move(done_cb);
  pending_encode->timestamp = frame->timestamp();
  pending_encodes_.push_back(std::move(pending_encode));
  accelerator_->Encode(frame, key_frame);
}

void VideoEncodeAcceleratorAdapter::ChangeOptions(const Options& options,
                                                  StatusCB done_cb) {}

void VideoEncodeAcceleratorAdapter::Flush(StatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->BelongsToCurrentThread());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread,
                     base::Unretained(this), WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread(StatusCB done_cb) {
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
  if (state_ != State::kReadyToEncode) {
    auto status =
        Status(StatusCode::kEncoderFailedFlush, "Encoder can't flush now");
    std::move(done_cb).Run(status);
    return;
  }

  if (pending_encodes_.empty()) {
    // Not pending encodes, nothing to flush.
    std::move(done_cb).Run(Status());
    return;
  }

  state_ = State::kFlushing;
  pending_flush_ = std::make_unique<PendingOp>();
  pending_flush_->done_callback = std::move(done_cb);

  // If flush is not supported FlushCompleted() will be called by
  // BitstreamBufferReady() when |pending_encodes_| is empty.
  if (flush_support_) {
    accelerator_->Flush(
        base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushCompleted,
                       base::Unretained(this)));
  }
}

void VideoEncodeAcceleratorAdapter::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  output_pool_ = base::MakeRefCounted<SharedMemoryPool>(gpu_factories_,
                                                        output_buffer_size);

  size_t input_buffer_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, input_coded_size);
  input_pool_ =
      base::MakeRefCounted<SharedMemoryPool>(gpu_factories_, input_buffer_size);

  int32_t buffer_id;
  if (!output_pool_->MaybeAllocateBuffer(&buffer_id)) {
    InitCompleted(Status(StatusCode::kEncoderInitializationError));
    return;
  }

  base::UnsafeSharedMemoryRegion* region = output_pool_->GetRegion(buffer_id);
  accelerator_->UseOutputBitstreamBuffer(
      BitstreamBuffer(buffer_id, region->Duplicate(), region->GetSize()));
  InitCompleted(Status());
  flush_support_ = accelerator_->IsFlushSupported();
}

void VideoEncodeAcceleratorAdapter::BitstreamBufferReady(
    int32_t buffer_id,
    const BitstreamBufferMetadata& metadata) {
  base::Optional<CodecDescription> desc;
  VideoEncoderOutput result;
  result.key_frame = metadata.key_frame;
  result.timestamp = metadata.timestamp;
  result.size = metadata.payload_size_bytes;

  base::WritableSharedMemoryMapping* mapping =
      output_pool_->GetMapping(buffer_id);
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
  base::UnsafeSharedMemoryRegion* region = output_pool_->GetRegion(buffer_id);
  accelerator_->UseOutputBitstreamBuffer(
      BitstreamBuffer(buffer_id, region->Duplicate(), region->GetSize()));

  for (auto it = pending_encodes_.begin(); it != pending_encodes_.end(); ++it) {
    if ((*it)->timestamp == result.timestamp) {
      std::move((*it)->done_callback).Run(Status());
      pending_encodes_.erase(it);
      break;
    }
  }
  output_cb_.Run(std::move(result), std::move(desc));
  if (pending_encodes_.empty() && !flush_support_) {
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
  for (auto& encode : pending_encodes_) {
    auto status =
        Status(StatusCode::kEncoderFailedEncode,
               "VideoEncodeAccelerator encountered an error")
            .WithData("VideoEncodeAccelerator::Error", int32_t{error});
    std::move(encode->done_callback).Run(Status());
  }
  pending_encodes_.clear();
  state_ = State::kNotInitialized;
}

void VideoEncodeAcceleratorAdapter::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {}

void VideoEncodeAcceleratorAdapter::InitCompleted(Status status) {
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
  if (!pending_init_)
    return;

  state_ = status.is_ok() ? State::kReadyToEncode : State::kNotInitialized;
  std::move(pending_init_->done_callback).Run(std::move(status));
  pending_init_.reset();
}

void VideoEncodeAcceleratorAdapter::FlushCompleted(bool success) {
  DCHECK(accelerator_task_runner_->BelongsToCurrentThread());
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
  return media::BindToLoop(callback_task_runner_.get(), std::move(cb));
}

}  // namespace media
