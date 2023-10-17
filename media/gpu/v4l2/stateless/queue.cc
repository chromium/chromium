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

  num_planes_ = kNumberInputPlanes;
  return true;
}

}  // namespace media
