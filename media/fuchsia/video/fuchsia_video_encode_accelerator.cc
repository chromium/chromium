// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/fuchsia/video/fuchsia_video_encode_accelerator.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <stddef.h>
#include <stdint.h>
#include <zircon/errors.h>
#include <zircon/types.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/vmo_buffer.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

// Hardcoded constants defined in the Amlogic driver.
// TODO(crbug.com/42050532): Get this values from platform API rather than
// hardcoding them.
constexpr int kMaxResolutionWidth = 1920;
constexpr int kMaxResolutionHeight = 1088;
constexpr size_t kMaxFrameRate = 60;
constexpr size_t kWidthAlignment = 16;
constexpr size_t kHeightAlignment = 2;
constexpr uint32_t kBytesPerRowAlignment = 32;

// Use 2 buffers for encoder input. Allocating more than one buffers ensures
// that when the decoder is done working on one packet it will have another one
// waiting in the queue. Limiting number of buffers to 2 allows to minimize
// required memory, without significant effect on performance.
constexpr size_t kInputBufferCount = 2;
constexpr uint32_t kOutputBufferCount = 1;

// Allocate 128KiB for SEI/SPS/PPS. (note that the same size is used for all
// codecs, not just H264).
constexpr size_t kOutputFrameConfigSize = 128 * 1024;

const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE,
    // TODO(crbug.com/40241992): Support HEVC codec.
};

fuchsia::sysmem::PixelFormatType GetPixelFormatType(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case PIXEL_FORMAT_I420:
      return fuchsia::sysmem::PixelFormatType::I420;
    case PIXEL_FORMAT_NV12:
      return fuchsia::sysmem::PixelFormatType::NV12;
    default:
      return fuchsia::sysmem::PixelFormatType::INVALID;
  }
}

}  // namespace

// Stores a queue of VideoFrames to be copied to VmoBuffers. VideoFrames can be
// queued before VmoBuffers are available. Queue will not start processing
// before Initialize() is called.
class FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue {
 public:
  using ProcessCB =
      base::RepeatingCallback<void(StreamProcessorHelper::IoPacket)>;

  VideoFrameWriterQueue() = default;

  VideoFrameWriterQueue(const VideoFrameWriterQueue&) = delete;
  VideoFrameWriterQueue& operator=(const VideoFrameWriterQueue&) = delete;

  // Enqueues a VideoFrame. Can be called before `Start()`. Immediately
  // processes `frame` if a VmoBuffer is available.
  void Enqueue(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Initialize the queue and starts processing if possible. `process_cb` is
  // called after each VideoFrame is copied.
  void Initialize(std::vector<VmoBuffer> buffers,
                  fuchsia::sysmem2::SingleBufferSettings buffer_settings,
                  fuchsia::media::FormatDetails initial_format_details,
                  gfx::Size coded_size,
                  ProcessCB process_cb);

 private:
  struct Item {
    Item(scoped_refptr<VideoFrame> frame, bool force_keyframe)
        : frame(std::move(frame)), force_keyframe(force_keyframe) {
      DCHECK(this->frame);
    }

    // Item is move-constructible for popping from the queue.
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    Item(Item&&) = default;
    Item& operator=(Item&&) = delete;

    scoped_refptr<VideoFrame> frame;
    const bool force_keyframe;
  };

  void ProcessQueue();

  // Marks the VmoBuffer at `buffer_index` to be available for copying.
  void ReleaseBuffer(size_t buffer_index);

  // Copies a VideoFrame from `item` to VmoBuffer at `buffer_index`.
  void CopyFrameToBuffer(const Item& item, size_t buffer_index);

  base::queue<Item> queue_;
  std::vector<VmoBuffer> buffers_;
  base::queue<size_t> free_buffer_indices_;
  fuchsia::media::FormatDetails format_details_;
  ProcessCB process_cb_;

  gfx::Size coded_size_;
  uint32_t dst_y_stride_ = 0;
  uint32_t dst_uv_stride_ = 0;
  uint32_t dst_y_plane_size_ = 0;
  size_t dst_size_ = 0;

  base::WeakPtrFactory<VideoFrameWriterQueue> weak_factory_{this};
};

// Stores a queue of IoPackets, whose data will be written to BitstreamBuffers.
// Packets can be queued before VmoBuffers are available and before any
// BitstreamBuffers are ready to be used. BitstreamBuffers can become ready
// before VmoBuffers are available. Queue will not start processing before
// Initialize() is called.
class FuchsiaVideoEncodeAccelerator::OutputPacketsQueue {
 public:
  using ProcessCB =
      base::RepeatingCallback<void(int32_t buffer_index,
                                   const BitstreamBufferMetadata& metadata)>;
  using ErrorCB = base::OnceCallback<void(EncoderStatus status)>;

  OutputPacketsQueue() = default;

  OutputPacketsQueue(const OutputPacketsQueue&) = delete;
  OutputPacketsQueue& operator=(const OutputPacketsQueue&) = delete;

  // Initialize the queue and starts processing if possible. `process_cb` is
  // called after data in each IoPacket is copied to BitstreamBuffer.
  void Initialize(std::vector<VmoBuffer> vmo_buffers,
                  ProcessCB process_cb,
                  ErrorCB error_cb);

  // Enqueues an IoPacket. Cannot be called before AcquireVmoBuffers(). Can be
  // called before BitstreamBuffers are ready. Immediately processes `packet` if
  // a BitstreamBuffer is available.
  void Enqueue(StreamProcessorHelper::IoPacket packet);

  // Add an available BitstreamBuffer. Starts processing the next packet in the
  // queue, if exists. Can be called before AcquireVmoBuffers().
  void UseBitstreamBuffer(BitstreamBuffer&& bitstream_buffer);

 private:
  void ProcessQueue();

  // Copies the data stored in VmoBuffer referred by `packet` to a
  // BitstreamBuffer. `metadata` is written with information from `packet`.
  // Returns `true` if no errors occurred.
  bool CopyPacketDataToBitstream(StreamProcessorHelper::IoPacket& packet,
                                 BitstreamBuffer& bitstream_buffer,
                                 BitstreamBufferMetadata* metadata);

  base::queue<StreamProcessorHelper::IoPacket> queue_;
  base::queue<BitstreamBuffer> bitstream_buffers_;
  std::vector<VmoBuffer> vmo_buffers_;
  ProcessCB process_cb_;
  ErrorCB error_cb_;
};

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::Enqueue(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  queue_.emplace(std::move(frame), force_keyframe);

  if (!buffers_.empty()) {
    ProcessQueue();
  }
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::Initialize(
    std::vector<VmoBuffer> buffers,
    fuchsia::sysmem2::SingleBufferSettings buffer_settings,
    fuchsia::media::FormatDetails initial_format_details,
    gfx::Size coded_size,
    ProcessCB process_cb) {
  DCHECK(buffers_.empty());
  DCHECK(!buffers.empty());

  buffers_ = std::move(buffers);
  format_details_ = std::move(initial_format_details);
  coded_size_ = coded_size;
  process_cb_ = std::move(process_cb);

  // Calculate the stride and size of each frame based on `buffer_settings`.
  // Frames must fit within the buffer.
  const auto& image_constraints = buffer_settings.image_format_constraints();
  dst_y_stride_ =
      base::bits::AlignUp(std::max(image_constraints.min_bytes_per_row(),
                                   static_cast<uint32_t>(coded_size_.width())),
                          image_constraints.bytes_per_row_divisor());
  dst_uv_stride_ = (dst_y_stride_ + 1) / 2;
  dst_y_plane_size_ = coded_size_.height() * dst_y_stride_;
  dst_size_ = dst_y_plane_size_ + dst_y_plane_size_ / 2;

  // Initialially, all buffers are free to use.
  for (size_t i = 0; i < buffers_.size(); i++) {
    free_buffer_indices_.push(i);
  }

  ProcessQueue();
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::ProcessQueue() {
  DCHECK(!buffers_.empty());

  while (!queue_.empty() && !free_buffer_indices_.empty()) {
    Item item = std::move(queue_.front());
    queue_.pop();
    size_t buffer_index = std::move(free_buffer_indices_.front());
    free_buffer_indices_.pop();

    CopyFrameToBuffer(item, buffer_index);

    auto packet = StreamProcessorHelper::IoPacket(
        buffer_index, /*offset=*/0, dst_size_, item.frame->timestamp(),
        /*unit_end=*/false, /*key_frame=*/false,
        base::BindOnce(&VideoFrameWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), buffer_index));
    if (item.force_keyframe) {
      fuchsia::media::FormatDetails format_details;
      zx_status_t status = format_details_.Clone(&format_details);
      ZX_DCHECK(status == ZX_OK, status) << "Clone FormatDetails";

      format_details.mutable_encoder_settings()->h264().set_force_key_frame(
          true);
      packet.set_format(std::move(format_details));
    }

    process_cb_.Run(std::move(packet));
  }
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::ReleaseBuffer(
    size_t free_buffer_index) {
  DCHECK(!buffers_.empty());

  free_buffer_indices_.push(free_buffer_index);
  ProcessQueue();
}

void FuchsiaVideoEncodeAccelerator::VideoFrameWriterQueue::CopyFrameToBuffer(
    const Item& item,
    size_t buffer_index) {
  DCHECK_LE(dst_size_, buffers_[buffer_index].size());

  uint8_t* dst_y = buffers_[buffer_index].GetWritableMemory().data();
  uint8_t* dst_u = dst_y + dst_y_plane_size_;
  uint8_t* dst_v = dst_u + dst_y_plane_size_ / 4;

  auto& frame = item.frame;
  CHECK_LE(frame->coded_size().width(), coded_size_.width());
  CHECK_LE(frame->coded_size().height(), coded_size_.height());

  int result = libyuv::I420Copy(
      frame->data(VideoFrame::Plane::kY), frame->stride(VideoFrame::Plane::kY),
      frame->data(VideoFrame::Plane::kU), frame->stride(VideoFrame::Plane::kU),
      frame->data(VideoFrame::Plane::kV), frame->stride(VideoFrame::Plane::kV),
      dst_y, dst_y_stride_, dst_u, dst_uv_stride_, dst_v, dst_uv_stride_,
      frame->coded_size().width(), frame->coded_size().height());
  DCHECK_EQ(result, 0);
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::Enqueue(
    StreamProcessorHelper::IoPacket packet) {
  queue_.push(std::move(packet));

  if (!bitstream_buffers_.empty() && !vmo_buffers_.empty()) {
    ProcessQueue();
  }
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::UseBitstreamBuffer(
    BitstreamBuffer&& buffer) {
  bitstream_buffers_.push(std::move(buffer));
  if (!queue_.empty()) {
    ProcessQueue();
  }
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::Initialize(
    std::vector<VmoBuffer> vmo_buffers,
    ProcessCB process_cb,
    ErrorCB error_cb) {
  DCHECK(vmo_buffers_.empty());
  DCHECK(!vmo_buffers.empty());

  vmo_buffers_ = std::move(vmo_buffers);
  process_cb_ = std::move(process_cb);
  error_cb_ = std::move(error_cb);
}

void FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::ProcessQueue() {
  DCHECK(!vmo_buffers_.empty());

  while (!queue_.empty() && !bitstream_buffers_.empty()) {
    int bitstream_buffer_id = bitstream_buffers_.front().id();

    BitstreamBufferMetadata metadata;
    bool success = CopyPacketDataToBitstream(
        queue_.front(), bitstream_buffers_.front(), &metadata);
    if (!success)
      return;

    queue_.pop();
    bitstream_buffers_.pop();

    process_cb_.Run(bitstream_buffer_id, metadata);
  }
}

bool FuchsiaVideoEncodeAccelerator::OutputPacketsQueue::
    CopyPacketDataToBitstream(StreamProcessorHelper::IoPacket& packet,
                              BitstreamBuffer& bitstream_buffer,
                              BitstreamBufferMetadata* metadata) {
  if (packet.size() > bitstream_buffer.size()) {
    std::move(error_cb_).Run(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Encoded output is too large. Packet size: %zu "
                            "Bitstream buffer size: %zu",
                            packet.size(), bitstream_buffer.size())});
    return false;
  }

  base::UnsafeSharedMemoryRegion region = bitstream_buffer.TakeRegion();
  base::WritableSharedMemoryMapping mapping =
      region.MapAt(bitstream_buffer.offset(), packet.size());
  if (!mapping.IsValid()) {
    std::move(error_cb_).Run({EncoderStatus::Codes::kSystemAPICallError,
                              "Failed to map BitstreamBuffer memory."});
    return false;
  }

  VmoBuffer& vmo_buffer = vmo_buffers_[packet.buffer_index()];

  metadata->payload_size_bytes =
      vmo_buffer.Read(packet.offset(), mapping.GetMemoryAsSpan<uint8_t>());
  metadata->key_frame = packet.key_frame();
  metadata->timestamp = packet.timestamp();
  return true;
}

FuchsiaVideoEncodeAccelerator::FuchsiaVideoEncodeAccelerator()
    : sysmem_allocator_("CrFuchsiaHWVideoEncoder") {}

FuchsiaVideoEncodeAccelerator::~FuchsiaVideoEncodeAccelerator() {
  DCHECK(!encoder_);
}

VideoEncodeAccelerator::SupportedProfiles
FuchsiaVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;

  SupportedProfile profile;
  profile.max_framerate_numerator = kMaxFrameRate;
  profile.max_framerate_denominator = 1;
  profile.rate_control_modes = VideoEncodeAccelerator::kConstantMode |
                               VideoEncodeAccelerator::kVariableMode;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  for (const auto& supported_profile : kSupportedProfiles) {
    profile.profile = supported_profile;
    profiles.push_back(profile);
  }
  return profiles;
}

bool FuchsiaVideoEncodeAccelerator::Initialize(
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int width = config.input_visible_size.width(),
      height = config.input_visible_size.height();
  if (width % kWidthAlignment != 0 || height % kHeightAlignment != 0) {
    MEDIA_LOG(ERROR, media_log)
        << "Fuchsia MediaCodec is only tested with resolutions that have width "
           "alignment "
        << kWidthAlignment << " and height alignment " << kHeightAlignment;
    return false;
  }

  if (width <= 0 || height <= 0) {
    return false;
  }
  if (width > kMaxResolutionWidth || height > kMaxResolutionHeight) {
    return false;
  }

  // TODO(crbug.com/40241991): Support NV12 pixel format.
  if (config.input_format != PIXEL_FORMAT_I420) {
    return false;
  }
  // TODO(crbug.com/40241992): Support HEVC codec.
  if (config.output_profile != H264PROFILE_BASELINE) {
    return false;
  }

  vea_client_ = client;
  media_log_ = std::move(media_log);
  config_ = std::make_unique<Config>(config);

  input_queue_ = std::make_unique<VideoFrameWriterQueue>();
  output_queue_ = std::make_unique<OutputPacketsQueue>();

  fuchsia::mediacodec::CodecFactoryPtr codec_factory =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::mediacodec::CodecFactory>();

  fuchsia::mediacodec::CreateEncoder_Params encoder_params;
  encoder_params.set_require_hw(true);
  encoder_params.set_input_details(CreateFormatDetails(*config_));

  fuchsia::media::StreamProcessorPtr stream_processor;
  codec_factory->CreateEncoder(std::move(encoder_params),
                               stream_processor.NewRequest());
  encoder_ = std::make_unique<StreamProcessorHelper>(
      std::move(stream_processor), this);

  // Output buffer size is calculated based on the input size with MinCR of 2,
  // plus config size.
  size_t allocation_size = VideoFrame::AllocationSize(
      config.input_format, config_->input_visible_size);
  auto output_buffer_size = allocation_size / 2 + kOutputFrameConfigSize;

  vea_client_->RequireBitstreamBuffers(
      /*input_count=*/1, /*input_coded_size=*/config_->input_visible_size,
      output_buffer_size);
  return true;
}

void FuchsiaVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->UseBitstreamBuffer(std::move(buffer));
}

void FuchsiaVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                           bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config_);
  DCHECK_EQ(frame->format(), PIXEL_FORMAT_I420);
  DCHECK(!frame->coded_size().IsEmpty());
  CHECK(frame->IsMappable());

  // Fuchsia VEA ignores the frame's `visible_rect` and encodes the whole
  // `coded_size`. So we need to check that `coded_size` fits in the allocated
  // buffer based on `input_visible_size`. This check should not fail due to
  // the frame's alignment, as `input_visible_size.width()` must be aligned to
  // `kWidthAlignment`.
  //
  // TODO(crbug.com/40245141): Encode only the `visible_rect` of a frame.
  if (frame->coded_size().width() > config_->input_visible_size.width() ||
      frame->coded_size().height() > config_->input_visible_size.height()) {
    OnError({EncoderStatus::Codes::kInvalidInputFrame,
             base::StringPrintf(
                 "Input frame size %s is larger than configured size %s",
                 frame->coded_size().ToString().c_str(),
                 config_->input_visible_size.ToString().c_str())});
    return;
  }

  input_queue_->Enqueue(std::move(frame), force_keyframe);
}

void FuchsiaVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  // TODO(crbug.com/40241995): Implement RequestEncodingParameterChange.
  NOTIMPLEMENTED();
}

void FuchsiaVideoEncodeAccelerator::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReleaseEncoder();
  delete this;
}

bool FuchsiaVideoEncodeAccelerator::IsFlushSupported() {
  // TODO(crbug.com/40242985): Implement Flush.
  return false;
}

bool FuchsiaVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return false;
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorAllocateInputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  input_buffer_collection_ = sysmem_allocator_.AllocateNewCollection();
  input_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::SetInputBufferCollectionToken,
                     base::Unretained(encoder_.get())));

  fuchsia::sysmem2::BufferCollectionConstraints constraints =
      VmoBuffer::GetRecommendedConstraints(kInputBufferCount,
                                           /*min_buffer_size=*/std::nullopt,
                                           /*writable=*/true);
  input_buffer_collection_->Initialize(std::move(constraints),
                                       "VideoEncoderInput");
  input_buffer_collection_->AcquireBuffers(
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnInputBuffersAcquired,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnInputBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config_);

  const auto& image_constraints = buffer_settings.image_format_constraints();
  int coded_width = base::bits::AlignUp(
      std::max(image_constraints.min_size().width,
               image_constraints.required_max_size().width),
      image_constraints.size_alignment().width);
  int coded_height = base::bits::AlignUp(
      std::max(image_constraints.min_size().height, image_constraints.required_max_size().height),
      image_constraints.size_alignment().height);
  CHECK_GE(coded_width, config_->input_visible_size.width());
  CHECK_GE(coded_height, config_->input_visible_size.height());

  input_queue_->Initialize(
      std::move(buffers), fidl::Clone(buffer_settings),
      CreateFormatDetails(*config_), gfx::Size(coded_width, coded_height),
      base::BindRepeating(&StreamProcessorHelper::Process,
                          base::Unretained(encoder_.get())));
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorAllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_buffer_collection_ = sysmem_allocator_.AllocateNewCollection();
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&StreamProcessorHelper::CompleteOutputBuffersAllocation,
                     base::Unretained(encoder_.get())));

  fuchsia::sysmem2::BufferCollectionConstraints constraints;
  constraints.mutable_usage()->set_cpu(fuchsia::sysmem2::CPU_USAGE_READ);
  constraints.set_min_buffer_count_for_shared_slack(kOutputBufferCount);
  output_buffer_collection_->Initialize(std::move(constraints),
                                        "VideoEncoderOutput");
  output_buffer_collection_->AcquireBuffers(
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnOutputBuffersAcquired,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnOutputBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->Initialize(
      std::move(buffers),
      base::BindRepeating(&VideoEncodeAccelerator::Client::BitstreamBufferReady,
                          base::Unretained(vea_client_.get())),
      base::BindOnce(&FuchsiaVideoEncodeAccelerator::OnError,
                     base::Unretained(this)));
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorOutputFormat(
    fuchsia::media::StreamOutputFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* format_details = format.mutable_format_details();
  if (!format_details->has_domain() || !format_details->domain().is_video() ||
      !format_details->domain().video().is_compressed()) {
    OnError({EncoderStatus::Codes::kEncoderFailedEncode,
             "Received invalid format from stream processor."});
  }
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorEndOfStream() {
  // StreamProcessor should not return EoS when Flush is not supported.
  NOTIMPLEMENTED();
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  output_queue_->Enqueue(std::move(packet));
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorNoKey() {
  // This method is only used for decryption.
  NOTREACHED_IN_MIGRATION();
}

void FuchsiaVideoEncodeAccelerator::OnStreamProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnError({EncoderStatus::Codes::kEncoderFailedEncode,
           "Encountered stream processor error."});
}

void FuchsiaVideoEncodeAccelerator::ReleaseEncoder() {
  // Drop queues and buffers before encoder, as their callbacks can reference
  // the encoder.
  input_queue_.reset();
  output_queue_.reset();
  input_buffer_collection_.reset();
  output_buffer_collection_.reset();

  encoder_.reset();
}

void FuchsiaVideoEncodeAccelerator::OnError(EncoderStatus status) {
  CHECK(!status.is_ok());
  LOG(ERROR) << "FuchsiaVideoEncodeAccelerator failed, error_code="
             << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (media_log_) {
    MEDIA_LOG(ERROR, media_log_) << status.message();
  }
  ReleaseEncoder();
  if (vea_client_) {
    vea_client_->NotifyErrorStatus(status);
  }
}

fuchsia::media::FormatDetails
FuchsiaVideoEncodeAccelerator::CreateFormatDetails(
    VideoEncodeAccelerator::Config& config) {
  DCHECK(config.input_visible_size.width() > 0);
  DCHECK(config.input_visible_size.height() > 0);

  uint32_t width = static_cast<uint32_t>(config.input_visible_size.width()),
           height = static_cast<uint32_t>(config.input_visible_size.height());

  DCHECK(width % kWidthAlignment == 0);
  DCHECK(height % kHeightAlignment == 0);

  fuchsia::media::FormatDetails format_details;
  format_details.set_format_details_version_ordinal(1);

  fuchsia::media::VideoUncompressedFormat uncompressed;
  uncompressed.image_format = fuchsia::sysmem::ImageFormat_2{
      .pixel_format = fuchsia::sysmem::PixelFormat{.type = GetPixelFormatType(
                                                       config.input_format)},
      .coded_width = width,
      .coded_height = height,
      .bytes_per_row = base::bits::AlignUp(width, kBytesPerRowAlignment),
      .display_width = width,
      .display_height = height,
  };
  fuchsia::media::VideoFormat video_format;
  video_format.set_uncompressed(std::move(uncompressed));
  fuchsia::media::DomainFormat domain;
  domain.set_video(std::move(video_format));
  format_details.set_domain(std::move(domain));

  // For now, hardcode mime type for H264.
  // TODO(crbug.com/40241992): Support HEVC codec.
  DCHECK(config.output_profile == H264PROFILE_BASELINE);
  format_details.set_mime_type("video/h264");
  fuchsia::media::H264EncoderSettings h264_settings;
  if (config.bitrate.target_bps() != 0) {
    h264_settings.set_bit_rate(config.bitrate.target_bps());
  }
  h264_settings.set_frame_rate(config.framerate);
  format_details.set_timebase(base::Time::kNanosecondsPerSecond /
                              config.framerate);

  if (config.gop_length.has_value()) {
    h264_settings.set_gop_size(config.gop_length.value());
  }
  h264_settings.set_force_key_frame(false);

  fuchsia::media::EncoderSettings encoder_settings;
  encoder_settings.set_h264(std::move(h264_settings));
  format_details.set_encoder_settings(std::move(encoder_settings));

  return format_details;
}

}  // namespace media
