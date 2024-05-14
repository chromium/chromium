// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/queue.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"

namespace {
// See http://crbug.com/255116.
constexpr int k1080pArea = 1920 * 1088;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
// The number of planes for a compressed buffer is always 1.
constexpr uint32_t kNumberInputPlanes = 1;

constexpr char kTracingCategory[] = "media,gpu";
constexpr char kV4L2OutputQueue[] = "V4L2 Output Buffer Queued Duration";
constexpr char kV4L2InputQueue[] = "V4L2 Input Buffer Queued Duration";
constexpr char kCompressedBufferIndex[] = "compressed buffer index";
constexpr char kDecodedBufferIndex[] = "decoded buffer index";

size_t BufferSizeForResolution(gfx::Size image_resolution) {
  return image_resolution.GetArea() > k1080pArea ? kInputBufferMaxSizeFor4k
                                                 : kInputBufferMaxSizeFor1080p;
}

}  // namespace

namespace media {

BaseQueue::BaseQueue(scoped_refptr<StatelessDevice> device,
                     BufferType buffer_type,
                     MemoryType memory_type)
    : device_(std::move(device)),
      buffer_type_(buffer_type),
      memory_type_(memory_type),
      num_planes_(1) {}

BaseQueue::~BaseQueue() {
  DVLOGF(3);

  StopStreaming();

  if (!buffers_.empty()) {
    DeallocateBuffers();
  }
}

bool BaseQueue::AllocateBuffers(uint32_t num_planes, size_t num_buffers) {
  DVLOGF(3);
  CHECK(device_);
  CHECK(num_planes);

  const auto count =
      device_->RequestBuffers(buffer_type_, memory_type_, num_buffers);

  if (!count) {
    LOG(ERROR) << "Requested " << num_buffers
               << " but was unable to allocate them from the driver.";
    return false;
  }

  DVLOGF(3) << num_buffers << " buffers request " << *count
            << " buffers allocated for " << Description() << " queue.";
  buffers_.reserve(*count);

  for (uint32_t index = 0; index < *count; ++index) {
    auto buffer =
        device_->QueryBuffer(buffer_type_, memory_type_, index, num_planes);
    if (!buffer) {
      LOG(ERROR) << "Failed to query buffer " << index << " of " << *count
                 << ".";
      buffers_ = std::vector<Buffer>();
      return false;
    }

    // Compressed buffers need to be mapped so that the data can be copied in.
    if (BufferType::kCompressedData == buffer_type_ &&
        MemoryType::kMemoryMapped == memory_type_) {
      if (!device_->MmapBuffer(*buffer)) {
        LOG(ERROR) << "Failed to map buffer # " << index;
        buffers_ = std::vector<Buffer>();
        return false;
      }
    }
    buffers_.push_back(std::move(*buffer));
    free_buffer_indices_.insert(index);
  }

  return true;
}

void BaseQueue::DeallocateBuffers() {
  DVLOGF(3);

  if (MemoryType::kMemoryMapped == memory_type_) {
    for (auto& buffer : buffers_) {
      device_->MunmapBuffer(buffer);
    }
  }
  buffers_.clear();

  const auto count = device_->RequestBuffers(buffer_type_, memory_type_, 0);
  LOG_IF(ERROR, !count) << "Failure to deallocate the buffers";
}

bool BaseQueue::StartStreaming() {
  DVLOGF(3);
  CHECK(device_);
  return device_->StreamOn(buffer_type_);
}

bool BaseQueue::StopStreaming() {
  DVLOGF(3);
  CHECK(device_);
  return device_->StreamOff(buffer_type_);
}

std::optional<uint32_t> BaseQueue::GetFreeBufferIndex() {
  if (free_buffer_indices_.empty()) {
    DVLOGF(5) << "No buffers available for " << Description();
    return std::nullopt;
  }

  auto it = free_buffer_indices_.begin();
  uint32_t index = *it;
  free_buffer_indices_.erase(index);

  DVLOGF(5) << "Now " << free_buffer_indices_.size()
            << " buffers available for " << Description();

  return index;
}

// static
std::unique_ptr<InputQueue> InputQueue::Create(
    scoped_refptr<StatelessDevice> device,
    const VideoCodec codec) {
  CHECK(device);

  return std::make_unique<InputQueue>(device, codec);
}

InputQueue::InputQueue(scoped_refptr<StatelessDevice> device, VideoCodec codec)
    : BaseQueue(device, BufferType::kCompressedData, MemoryType::kMemoryMapped),
      codec_(codec) {}

bool InputQueue::NeedToReallocateBuffers(const gfx::Size new_resolution) {
  const size_t buffer_size = BufferSizeForResolution(new_resolution);

  if (allocated_buffer_size_ >= buffer_size) {
    return false;
  }

  return true;
}

bool InputQueue::SetupFormat(const gfx::Size resolution) {
  DVLOGF(3);
  CHECK(device_);

  const auto range = device_->GetFrameResolutionRange(codec_);

  allocated_buffer_size_ = BufferSizeForResolution(range.second);

  if (!device_->SetInputFormat(codec_, resolution, allocated_buffer_size_)) {
    allocated_buffer_size_ = 0;
    return false;
  }

  return true;
}

bool InputQueue::PrepareBuffers(size_t num_buffers,
                                const gfx::Size resolution) {
  DVLOGF(3);

  const bool reallocate = NeedToReallocateBuffers(resolution);

  if (SetupFormat(resolution)) {
    if (reallocate) {
      return AllocateBuffers(kNumberInputPlanes, num_buffers);
    }
    return true;
  }

  return false;
}

const std::optional<Buffer> InputQueue::DequeueBuffer() {
  DVLOGF(4);
  const auto buffer =
      device_->DequeueBuffer(buffer_type_, memory_type_, num_planes_);

  if (!buffer) {
    return std::nullopt;
  }

  const uint32_t index = buffer->GetIndex();
  TRACE_EVENT_NESTABLE_ASYNC_END0(kTracingCategory, kV4L2InputQueue,
                                  TRACE_ID_LOCAL(index));

  DVLOGF(4) << "#" << index << " returned, now "
            << free_buffer_indices_.size() + 1 << " " << Description()
            << " available.";
  if (!free_buffer_indices_.insert(index).second) {
    NOTREACHED_IN_MIGRATION()
        << "There is no way that a reclaimed buffer is already "
           "present in the list";
  }

  return buffer;
}

bool InputQueue::SubmitCompressedFrameData(const void* data,
                                           size_t length,
                                           uint64_t frame_id,
                                           const base::ScopedFD& request_fd) {
  // Failing to acquire a buffer is a normal part of the process. All of the
  // input buffers can be full if the output buffers are not being cleared.
  auto buffer_index = GetFreeBufferIndex();
  if (!buffer_index) {
    // The caller is responsible for making sure that a buffer is present.
    NOTREACHED_IN_MIGRATION()
        << "No free buffers to submit a compressed frame with.";
    return false;
  }

  DVLOG(3) << "Submitting buffer " << buffer_index.value();

  Buffer& buffer = buffers_[buffer_index.value()];

  // Compressed input buffers only need one plane for data,
  // uncompressed output buffers may need more than one plane.
  if (1 != buffer.PlaneCount()) {
    LOG(ERROR) << "Compressed buffer has more than one plane: "
               << buffer.PlaneCount();
    return false;
  }

  // |frame_id| is used for two things:
  // 1. To track the buffer from compressed to uncompressed. The timestamp will
  //    be copied.
  // 2. This value is also used for reference frame management. Future frames
  //    can reference this one by using the |frame_id|.
  buffer.SetTimeAsFrameID(frame_id);
  if (!buffer.CopyDataIn(data, length)) {
    LOG(ERROR) << "Unable to copy compressed buffer into driver.";
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kTracingCategory, kV4L2InputQueue,
                                    TRACE_ID_LOCAL(buffer.GetIndex()),
                                    kCompressedBufferIndex, buffer.GetIndex());

  // This shouldn't happen. A buffer has been allocated and filled, there
  // should be nothing preventing it from getting queued.
  if (!device_->QueueBuffer(buffer, request_fd)) {
    LOG(ERROR) << "Failed to queue buffer.";
    return false;
  }

  return true;
}

std::string InputQueue::Description() {
  return "input";
}

std::unique_ptr<OutputQueue> OutputQueue::Create(
    scoped_refptr<StatelessDevice> device) {
  std::unique_ptr<OutputQueue> queue = std::make_unique<OutputQueue>(device);

  if (!queue->NegotiateFormat()) {
    return nullptr;
  }

  return queue;
}

OutputQueue::OutputQueue(scoped_refptr<StatelessDevice> device)
    : BaseQueue(device, BufferType::kDecodedFrame, MemoryType::kMemoryMapped),
      buffer_format_(BufferFormat(Fourcc(Fourcc::UNDEFINED),
                                  gfx::Size(0, 0),
                                  BufferType::kDecodedFrame)) {}

OutputQueue::~OutputQueue() {}

bool OutputQueue::NegotiateFormat() {
  DVLOGF(3);
  CHECK(device_);

  // should also have associated number of planes, or are they all 2?
  constexpr Fourcc kPreferredFormats[] = {
      Fourcc(Fourcc::NV12), Fourcc(Fourcc::MM21), Fourcc(Fourcc::MT2T)};

  const auto initial_format = device_->GetOutputFormat();
  if (!initial_format) {
    return false;
  }

  BufferFormat desired_format = *initial_format;

  if (!base::Contains(kPreferredFormats, initial_format->fourcc)) {
    for (const auto& preferred_fourcc : kPreferredFormats) {
      // Only change the fourcc between tries.
      desired_format.fourcc = preferred_fourcc;
      if (device_->TryOutputFormat(desired_format)) {
        break;
      }
    }
  }

  // If |initial_format| is not in the list of formats, and all of the formats
  // tried with |TryOutputFormat| fail, the last format tried will be used
  // for |SetOutputFormat|. In that case |SetOutputFormat| will fail.
  std::optional<BufferFormat> chosen_format =
      device_->SetOutputFormat(desired_format);

  if (chosen_format) {
    DVLOGF(3) << "Format " << chosen_format->ToString()
              << " chosen for output queue through negotiation. "
              << "Initial format was " << initial_format->ToString() << ".";
    buffer_format_ = *chosen_format;
    num_planes_ = buffer_format_.NumPlanes();
    return true;
  }

  LOG(ERROR) << "Unable to negotiate a format for the output queue with an "
                "initial format of "
             << initial_format->ToString() << " and desired format of "
             << desired_format.ToString();
  return false;
}

scoped_refptr<FrameResource> OutputQueue::CreateFrame(const Buffer& buffer) {
  const VideoPixelFormat video_format =
      buffer_format_.fourcc.ToVideoPixelFormat();
  const size_t num_color_planes = VideoFrame::NumPlanes(video_format);
  if (num_color_planes == 0) {
    LOG(ERROR) << "Unsupported video format for NumPlanes(): "
               << VideoPixelFormatToString(video_format);
    return nullptr;
  }

  if (buffer.PlaneCount() > num_color_planes) {
    LOG(ERROR) << "Number of planes for the format (" << buffer.PlaneCount()
               << ") should not be larger than number of color planes("
               << num_color_planes << ") for format "
               << VideoPixelFormatToString(video_format);
    return nullptr;
  }

  // TODO(b/322521142): Stride is needed for the layout, but |buffer| does not
  // contain that information. It only contains the length of a plane.
  // |buffer_format_| does contain that information, but it currently doesn't
  // have the correct |image_size|. |image_size| is being computed incorrectly
  // for MT2T.
  std::vector<ColorPlaneLayout> color_planes;
  for (uint32_t i = 0; i < num_color_planes; ++i) {
    color_planes.emplace_back(buffer_format_.planes[i].stride, 0u,
                              buffer.PlaneLength(i));
  }

  // This code has been developed for exclusively for MM21 and MT2T. For other
  // formats such as NV12 and YUV420 there would be color plane duplications, or
  // a VideoFrameLayout::CreateWithPlanes.
  CHECK_EQ(buffer.PlaneCount(), buffer_format_.NumPlanes());
  CHECK_EQ(buffer.PlaneCount(), 2u);

  std::vector<base::ScopedFD> dmabuf_fds = device_->ExportAsDMABUF(buffer);
  if (dmabuf_fds.empty()) {
    LOG(ERROR) << "Failed to get DMABUFs of V4L2 buffer";
    return nullptr;
  }

  for (const auto& dmabuf_fd : dmabuf_fds) {
    if (!dmabuf_fd.is_valid()) {
      LOG(ERROR) << "Fail to get DMABUFs of V4L2 buffer - invalid fd";
      return nullptr;
    }
  }

  // Some V4L2 devices expect buffers to be page-aligned. We cannot detect
  // such devices individually, so set this as a video frame layout property.
  constexpr size_t buffer_alignment = 0x1000;
  auto layout = VideoFrameLayout::CreateMultiPlanar(
      video_format, buffer_format_.resolution, std::move(color_planes),
      buffer_alignment);

  if (!layout) {
    LOG(ERROR) << "Unable to create a video frame layout for "
               << VideoPixelFormatToString(video_format);
    return nullptr;
  }

  // TODO(nhebert): Migrate to NativePixmap-backed FrameResource when it is
  // ready.
  return VideoFrameResource::Create(VideoFrame::WrapExternalDmabufs(
      *layout, gfx::Rect(buffer_format_.resolution), buffer_format_.resolution,
      std::move(dmabuf_fds), base::TimeDelta()));
}

bool OutputQueue::PrepareBuffers(size_t num_buffers) {
  DVLOGF(3);

  if (!AllocateBuffers(buffer_format_.NumPlanes(), num_buffers)) {
    return false;
  }

  // Create a FrameResource for each buffer
  frames_.reserve(buffers_.size());

  // FrameResource objects are by VideoDecoderPipeline to encapsulate decoded
  // buffers. They wrap the underlying DMABUF of elements of |buffers_|. The
  // the index of the encapsulating FrameResource in |frames_| is aligned to the
  // corresponding buffer's index in |buffers_|.
  for (const auto& buffer : buffers_) {
    auto frame = CreateFrame(buffer);
    if (!frame) {
      return false;
    }
    frames_.push_back(std::move(frame));
  }

  return true;
}

const std::optional<Buffer> OutputQueue::DequeueBuffer() {
  const auto buffer =
      device_->DequeueBuffer(buffer_type_, memory_type_, num_planes_);

  if (!buffer) {
    return std::nullopt;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(kTracingCategory, kV4L2OutputQueue,
                                  TRACE_ID_LOCAL(buffer->GetIndex()));

  // Once the buffer is dequeued it needs to be tracked. The index is all that
  // is needed to track the buffer. That index is what will be used when passing
  // the buffer off. The time is need to tell which buffer should be passed off.
  // With MPEG codecs display order can be different then decode order. For
  // this reason the most recently decoded buffer may not be displayed right
  // away.
  //
  // The input and output queues are independent. When the input buffer is done
  // being decoded the timestamp is copied over to the output buffer. When
  // this frame is ready to be displayed the timestamp is what will be
  // needed. Because of the detached nature of the queues there is no way to
  // know which output buffer index corresponds to the input buffer. Using
  // the timestamp this can be found.
  const uint64_t frame_id = buffer->GetTimeAsFrameID();
  const uint32_t index = buffer->GetIndex();
  const auto result = decoded_and_dequeued_frames_.insert({frame_id, index});

  DVLOGF(4) << "Inserted buffer " << index << " with a frame id of "
            << frame_id;

  CHECK(result.second) << "Buffer already in map";

  return buffer;
}

scoped_refptr<FrameResource> OutputQueue::GetFrame(uint64_t frame_id) {
  DVLOGF(5) << "Attempting to use frame with id : " << frame_id;
  // The frame_id is copied from the input buffer to the output buffer. This is
  // the only way to know which output buffer contains the decoded picture for
  // a given compressed input buffer.
  auto it = decoded_and_dequeued_frames_.find(frame_id);
  if (it != decoded_and_dequeued_frames_.end()) {
    const uint32_t index = it->second;
    DVLOGF(4) << "Found match (" << index << ") for frame id of (" << frame_id
              << ").";
    return frames_[index];
  }

  // The corresponding frame may not have been dequeued when this function has
  // been called. This is not an error, but expected. When this occurs the
  // caller should try again after waiting for another buffer to be dequeued.
  return nullptr;
}

bool OutputQueue::QueueBuffer() {
  auto buffer_index = GetFreeBufferIndex();
  if (!buffer_index) {
    return false;
  }

  const uint32_t index = buffer_index.value();

  if (!device_->QueueBuffer(buffers_[index], base::ScopedFD(-1))) {
    return false;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kTracingCategory, kV4L2OutputQueue,
                                    TRACE_ID_LOCAL(index), kDecodedBufferIndex,
                                    index);

  DVLOGF(4) << "Queued buffer : " << index;

  return true;
}

void OutputQueue::ReturnBuffer(uint64_t frame_id) {
  DVLOGF(4) << "Returning frame id : " << frame_id;

  auto it = decoded_and_dequeued_frames_.find(frame_id);
  if (it == decoded_and_dequeued_frames_.end()) {
    NOTREACHED_IN_MIGRATION() << "Decoded buffer with frame id (" << frame_id
                              << ") not previously dequeued.";
  }

  const uint32_t buffer_index = it->second;
  DVLOGF(4) << "Buffer (" << buffer_index << ") returned.";
  decoded_and_dequeued_frames_.erase(it);

  if (!free_buffer_indices_.insert(buffer_index).second) {
    NOTREACHED_IN_MIGRATION()
        << "There is no way that a reclaimed buffer is already "
           "present in the list";
  }
}

std::string OutputQueue::Description() {
  return "output";
}

// static
std::unique_ptr<RequestQueue> RequestQueue::Create(
    scoped_refptr<StatelessDevice> device) {
  CHECK(device);

  return std::make_unique<RequestQueue>(device);
}

RequestQueue::RequestQueue(scoped_refptr<StatelessDevice> device)
    : device_(std::move(device)) {}

RequestQueue::~RequestQueue() {}

base::ScopedFD RequestQueue::CreateRequestFD() {
  DVLOGF(4);
  return device_->CreateRequestFD();
}

bool RequestQueue::SetHeadersForFormatNegotiation(void* ctrls) {
  DVLOGF(4);
  return device_->SetHeaders(ctrls, base::ScopedFD(-1));
}

bool RequestQueue::QueueRequest(void* ctrls, const base::ScopedFD& request_fd) {
  DVLOGF(4);

  if (!device_->SetHeaders(ctrls, request_fd)) {
    LOG(ERROR) << "Unable to set headers to V4L2 at fd: " << request_fd.get();
    return false;
  }

  if (!device_->QueueRequest(request_fd)) {
    LOG(ERROR) << "Unable to queue request at fd :" << request_fd.get();
    return false;
  }

  return true;
}

}  // namespace media
