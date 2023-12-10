// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/queue.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
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
}  // namespace

namespace media {

BaseQueue::BaseQueue(scoped_refptr<StatelessDevice> device,
                     BufferType buffer_type,
                     MemoryType memory_type)
    : device_(std::move(device)),
      buffer_type_(buffer_type),
      memory_type_(memory_type) {}

BaseQueue::~BaseQueue() {
  DVLOGF(4);

  StopStreaming();

  if (!buffers_.empty()) {
    DeallocateBuffers();
  }
}

bool BaseQueue::AllocateBuffers(uint32_t num_planes) {
  DVLOGF(4);
  CHECK(device_);
  CHECK(num_planes);

  const auto count =
      device_->RequestBuffers(buffer_type_, memory_type_, BufferMinimumCount());

  if (!count) {
    return false;
  }

  DVLOGF(2) << BufferMinimumCount() << " buffers request " << *count
            << " buffers allocated for " << Description() << " queue.";
  buffers_.reserve(*count);

  for (uint32_t index = 0; index < *count; ++index) {
    auto buffer =
        device_->QueryBuffer(buffer_type_, memory_type_, index, num_planes);
    if (!buffer) {
      DVLOGF(1) << "Failed to query buffer " << index << " of " << *count
                << ".";
      buffers_ = std::vector<Buffer>();
      return false;
    }

    if (BufferType::kCompressedData == buffer_type_ &&
        MemoryType::kMemoryMapped == memory_type_) {
      device_->MmapBuffer(*buffer);
    }
    buffers_.push_back(std::move(*buffer));
    free_buffer_indices_.insert(index);
  }

  return true;
}

bool BaseQueue::DeallocateBuffers() {
  buffers_.clear();

  const auto count = device_->RequestBuffers(buffer_type_, memory_type_, 0);
  if (!count) {
    return false;
  }

  return true;
}

bool BaseQueue::StartStreaming() {
  CHECK(device_);
  return device_->StreamOn(buffer_type_);
}

bool BaseQueue::StopStreaming() {
  CHECK(device_);
  return device_->StreamOff(buffer_type_);
}

absl::optional<uint32_t> BaseQueue::GetFreeBufferIndex() {
  // TODO(frkoenig): This is an expected error, there will be times that all of
  // the buffers will be in the queue. For now give it a high severity for
  // visibility.
  if (free_buffer_indices_.empty()) {
    DVLOGF(1) << "No buffers available for " << Description();
    return absl::nullopt;
  }

  auto it = free_buffer_indices_.begin();
  uint32_t index = *it;
  free_buffer_indices_.erase(index);

  DVLOGF(3) << free_buffer_indices_.size() << " buffers available for "
            << Description();

  return index;
}

// static
std::unique_ptr<InputQueue> InputQueue::Create(
    scoped_refptr<StatelessDevice> device,
    const VideoCodec codec,
    const gfx::Size resolution) {
  CHECK(device);
  std::unique_ptr<InputQueue> queue =
      std::make_unique<InputQueue>(device, codec);

  if (!queue->SetupFormat(resolution)) {
    return nullptr;
  }

  return queue;
}

InputQueue::InputQueue(scoped_refptr<StatelessDevice> device, VideoCodec codec)
    : BaseQueue(device, BufferType::kCompressedData, MemoryType::kMemoryMapped),
      codec_(codec) {}

bool InputQueue::SetupFormat(const gfx::Size resolution) {
  DVLOGF(4);
  CHECK(device_);

  const auto range = device_->GetFrameResolutionRange(codec_);

  size_t encoded_buffer_size = range.second.GetArea() > k1080pArea
                                   ? kInputBufferMaxSizeFor4k
                                   : kInputBufferMaxSizeFor1080p;
  if (!device_->SetInputFormat(codec_, resolution, encoded_buffer_size)) {
    return false;
  }

  return true;
}

bool InputQueue::PrepareBuffers() {
  DVLOGF(4);
  return AllocateBuffers(kNumberInputPlanes);
}

void InputQueue::Reclaim() {
  DVLOGF(4) << Description();
  CHECK(device_);

  // There may be more than one buffer available. Keep trying to dequeue buffers
  // until there aren't anymore.
  while (true) {
    auto buffer =
        device_->DequeueBuffer(buffer_type_, memory_type_, kNumberInputPlanes);
    if (!buffer) {
      return;
    }

    const uint32_t index = buffer->GetIndex();
    DVLOGF(3) << "#" << index << " returned, now "
              << free_buffer_indices_.size() + 1 << " " << Description()
              << " available.";
    if (!free_buffer_indices_.insert(index).second) {
      // There is no way that a reclaimed buffer is already present in the list.
      NOTREACHED();
    }
  }
}

bool InputQueue::SubmitCompressedFrameData(void* ctrls,
                                           const void* data,
                                           size_t length,
                                           uint32_t frame_id) {
  // Failing to acquire a buffer is a normal part of the process. All of the
  // input buffers can be full if the output buffers are not being cleared.
  auto buffer_index = GetFreeBufferIndex();
  if (!buffer_index) {
    DVLOGF(1) << "No free buffers to submit a compressed frame with.";
    // TODO(frkoenig): This is a place holder. Correct error handling needs
    // to be implemented. It may be better to obtain the buffer and pass it
    // into this method so that retry is more straight forward.
    NOTREACHED();
    return false;
  }

  DVLOG(3) << "Submitting buffer " << buffer_index.value();

  Buffer& buffer = buffers_[buffer_index.value()];

  // Compressed input buffers only need one plane for data,
  // uncompressed output buffers may need more than one plane.
  if (1 != buffer.PlaneCount()) {
    DVLOGF(1) << "Compressed buffer has more than one plane: "
              << buffer.PlaneCount();
    return false;
  }

  // Each request needs an FD. A pool of FD's can be reused, but require
  // reinitialization after use. Instead a scoped FD is created, which will
  // be closed at the end of this function. This is fine as the driver will
  // keep the FD open until it is done using it.
  const base::ScopedFD request_fd = device_->CreateRequestFD();

  // |frame_id| is used for two things:
  // 1. To track the buffer from compressed to uncompressed. The timestamp will
  //    be copied.
  // 2. This value is also used for reference frame management. Future frames
  //    can reference this one by using the |frame_id|.
  buffer.SetTimeAsFrameID(frame_id);
  buffer.CopyDataIn(data, length);

  // This shouldn't happen. A buffer has been allocated and filled, there
  // should be nothing preventing it from getting queued.
  if (!device_->QueueBuffer(buffer, request_fd)) {
    DVLOGF(1) << "Failed to queue buffer.";
    return false;
  }

  // Headers submission failure should never happen. There is no way to
  // recover from this error.
  if (!device_->SetHeaders(ctrls, request_fd)) {
    DVLOGF(1) << "Unable to set headers to V4L2 at fd: " << request_fd.get();
    return false;
  }

  // Everything has been allocated and this is the final submission. To error
  // out here would mean that the driver is not in a state to decode video.
  if (!device_->QueueRequest(request_fd)) {
    DVLOGF(1) << "Unable to queue request at fd :" << request_fd.get();
    return false;
  }

  return true;
}

std::string InputQueue::Description() {
  return "input";
}

uint32_t InputQueue::BufferMinimumCount() {
  // TODO: This number has been cargo culting around for a while. One buffer
  // could be enough as there is buffering elsewhere in the system. This
  // number should be revisited after end to end playback is completed and
  // performance tuning is done.
  return 8;
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
    : BaseQueue(device, BufferType::kRawFrames, MemoryType::kMemoryMapped),
      buffer_format_(BufferFormat(Fourcc(Fourcc::UNDEFINED),
                                  gfx::Size(0, 0),
                                  BufferType::kRawFrames)) {}

OutputQueue::~OutputQueue() {}

bool OutputQueue::NegotiateFormat() {
  DVLOGF(4);
  CHECK(device_);

  // should also have associated number of planes, or are they all 2?
  constexpr Fourcc kPreferredFormats[] = {
      Fourcc(Fourcc::NV12), Fourcc(Fourcc::MM21), Fourcc(Fourcc::MT2T)};

  const auto initial_format = device_->GetOutputFormat();
  if (!initial_format) {
    return false;
  }

  if (!base::Contains(kPreferredFormats, initial_format->fourcc)) {
    for (const auto& preferred_fourcc : kPreferredFormats) {
      BufferFormat try_format = *initial_format;
      try_format.fourcc = preferred_fourcc;
      if (device_->TryOutputFormat(try_format)) {
        auto chosen_format = device_->SetOutputFormat(try_format);
        if (chosen_format) {
          DVLOGF(2) << "Preferred format " << chosen_format->fourcc.ToString()
                    << " choosen for output queue through negotiation. "
                    << "Initial format was "
                    << initial_format->fourcc.ToString() << ".";
          buffer_format_ = *chosen_format;
          return true;
        } else {
          return false;
        }
      }
    }
  } else {
    DVLOGF(2) << "Initial format " << initial_format->fourcc.ToString()
              << " choosen for output queue.";
    auto chosen_format = device_->SetOutputFormat(*initial_format);
    if (chosen_format) {
      buffer_format_ = *chosen_format;
      return true;
    }
  }

  return false;
}

scoped_refptr<VideoFrame> OutputQueue::CreateVideoFrame(uint32_t index) {
  const VideoPixelFormat video_format =
      buffer_format_.fourcc.ToVideoPixelFormat();
  const size_t num_color_planes = VideoFrame::NumPlanes(video_format);
  if (num_color_planes == 0) {
    DVLOGF(1) << "Unsupported video format for NumPlanes(): "
              << VideoPixelFormatToString(video_format);
    return nullptr;
  }

  if (buffer_format_.NumPlanes() > num_color_planes) {
    DVLOGF(1) << "Number of planes for the format ("
              << buffer_format_.NumPlanes()
              << ") should not be larger than number of color planes("
              << num_color_planes << ") for format "
              << VideoPixelFormatToString(video_format);
    return nullptr;
  }

  std::vector<ColorPlaneLayout> color_planes;
  for (const auto& plane : buffer_format_.planes) {
    color_planes.emplace_back(plane.stride, 0u, plane.image_size);
  }

  // This code has been developed for exclusively for MM21. For other formats
  // such as NV12 and YUV420 there would be color plane duplications, or
  // a VideoFrameLayout::CreateWithPlanes.
  CHECK_EQ(static_cast<size_t>(buffer_format_.NumPlanes()), num_color_planes);
  CHECK_EQ(buffer_format_.NumPlanes(), 2u);

  std::vector<base::ScopedFD> dmabuf_fds =
      device_->ExportAsDMABUF(index, buffer_format_.NumPlanes());
  if (dmabuf_fds.empty()) {
    DVLOGF(1) << "Failed to get DMABUFs of V4L2 buffer";
    return nullptr;
  }

  for (const auto& dmabuf_fd : dmabuf_fds) {
    if (!dmabuf_fd.is_valid()) {
      DVLOGF(1) << "Fail to get DMABUFs of V4L2 buffer - invalid fd";
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
    return nullptr;
  }

  return VideoFrame::WrapExternalDmabufs(
      *layout, gfx::Rect(buffer_format_.resolution), buffer_format_.resolution,
      std::move(dmabuf_fds), base::TimeDelta());
}

bool OutputQueue::PrepareBuffers() {
  DVLOGF(4);

  if (!AllocateBuffers(buffer_format_.NumPlanes())) {
    return false;
  }

  // Create a video frame for each buffer
  video_frames_.reserve(buffers_.size());

  // VideoFrames are used to display the decoded buffers. They wrap the
  // underlying DMABUF by referencing the index of the V4L2 buffers;
  for (uint32_t index = 0; index < buffers_.size(); ++index) {
    auto video_frame = CreateVideoFrame(index);
    if (!video_frame) {
      return false;
    }
    video_frames_.push_back(std::move(video_frame));
  }

  // Queue all buffers after allocation in anticipation of being used.
  for (auto index = free_buffer_indices_.begin();
       index != free_buffer_indices_.end();) {
    if (!device_->QueueBuffer(buffers_[*index], base::ScopedFD(-1))) {
      DVLOGF(1) << "Failed to queue buffer.";
      return false;
    }
    free_buffer_indices_.erase(index++);
  }

  return true;
}

bool OutputQueue::DequeueBuffer() {
  CHECK(device_);
  // The assumption is there is only one buffer in flight at a time.
  const auto buffer = device_->DequeueBuffer(buffer_type_, memory_type_,
                                             buffer_format_.NumPlanes());

  // So there should never be more than one buffer ready to dequeue. If there
  // is another buffer to be dequeued then this will fail.
  // TODO: Should this be a return value? The function is a bool, so this could
  // be returned but it would incur an extra dequeue attempt
  DCHECK(absl::nullopt == device_->DequeueBuffer(buffer_type_, memory_type_,
                                                 buffer_format_.NumPlanes()));

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
  const auto result = decoded_and_dequeued_frames_.insert(
      {buffer->GetTimeAsFrameID(), buffer->GetIndex()});

  DVLOGF(3) << "Inserted buffer (" << buffer->GetIndex()
            << ") with a frame id of " << buffer->GetTimeAsFrameID();

  return result.second;
}

absl::optional<uint32_t> OutputQueue::UseDequeuedBuffer(uint64_t frame_id) {
  DVLOGF(3) << "Attempting to use frame with id : " << frame_id;
  // The frame_id is copied from the input buffer to the output buffer. This is
  // the only way to know which output buffer contains the decoded picture for
  // a given compressed input buffer.
  auto it = decoded_and_dequeued_frames_.find(frame_id);
  if (it != decoded_and_dequeued_frames_.end()) {
    const uint32_t index = it->second;
    decoded_and_dequeued_frames_.erase(it);
    DVLOGF(3) << "Found match (" << index << ") for frame id of (" << frame_id
              << ").";
    return index;
  }

  // The corresponding frame may not have been dequeued when this function has
  // been called. This is not an error, but expected. When this occurs the
  // caller should try again after waiting for another buffer to be dequeued.
  return absl::nullopt;
}

scoped_refptr<VideoFrame> OutputQueue::DecodedFrameByIndex(uint32_t index) {
  DVLOGF(4) << Description() << " index : " << index;
  CHECK(device_);

  // This frame can not be re-enqueued right away.
  // 1. it hasn't been displayed yet. the underlying buffer is passed on
  //    to the display/image processor. it can only be requeued after that
  //    has occurred.
  // 2. the frame can be used as a reference frame. it can only re-enqueued
  //    after all of the frames that reference it are decoded (but not
  //    necessarily displayed)

  return video_frames_[index];
}

bool OutputQueue::QueueBufferByIndex(uint32_t index) {
  DVLOGF(4) << Description() << " index : " << index;
  if (!device_->QueueBuffer(buffers_[index], base::ScopedFD(-1))) {
    NOTREACHED() << "Failed to queue buffer.";
    return false;
  }

  free_buffer_indices_.erase(index);

  return true;
}

std::string OutputQueue::Description() {
  return "output";
}

uint32_t OutputQueue::BufferMinimumCount() {
  return 4;
}

}  // namespace media
