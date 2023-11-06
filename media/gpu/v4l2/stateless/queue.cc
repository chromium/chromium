// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/queue.h"

#include "base/containers/contains.h"
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

  uint32_t num_buffers_requested = BufferMinimumCount();

  const auto count = device_->RequestBuffers(buffer_type_, memory_type_,
                                             num_buffers_requested);
  if (!count) {
    return false;
  }

  DVLOGF(2) << num_buffers_requested << " buffers request " << *count
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
}  // namespace media
