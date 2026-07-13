// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/mock_camera_device.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/video/video_capture_device_info.h"
#include "services/video_capture/public/mojom/constants.mojom-forward.h"

namespace content {
namespace {

constexpr media::VideoPixelFormat kMockCameraPixelFormat =
    media::PIXEL_FORMAT_I420;

constexpr int kDefaultFrameRate = 5;

base::TimeDelta FrameIntervalForFrameRate(double frame_rate) {
  if (frame_rate <= 0.0) {
    frame_rate = kDefaultFrameRate;
  }

  return base::Milliseconds(
      std::max<int64_t>(1, static_cast<int64_t>(1000.0 / frame_rate)));
}

media::mojom::PlaneStridesPtr CreateI420PlaneStrides(const gfx::Size& size) {
  std::vector<uint32_t> mojo_strides(media::VideoFrame::kMaxPlanes);
  const std::vector<size_t> strides =
      media::VideoFrame::ComputeStrides(kMockCameraPixelFormat, size);

  for (size_t plane = 0; plane < strides.size(); ++plane) {
    mojo_strides[plane] = base::checked_cast<uint32_t>(strides[plane]);
  }

  return media::mojom::PlaneStrides::New(std::move(mojo_strides));
}

size_t PlaneByteSize(size_t plane,
                     const gfx::Size& size,
                     const std::vector<size_t>& strides) {
  const gfx::Size plane_size =
      media::VideoFrame::PlaneSize(kMockCameraPixelFormat, plane, size);

  return strides[plane] * base::checked_cast<size_t>(plane_size.height());
}

bool FillBlackI420Frame(base::span<uint8_t> data, const gfx::Size& size) {
  const std::vector<size_t> strides =
      media::VideoFrame::ComputeStrides(kMockCameraPixelFormat, size);
  const size_t y_plane_size = PlaneByteSize(0, size, strides);
  const size_t u_plane_size = PlaneByteSize(1, size, strides);
  const size_t v_plane_size = PlaneByteSize(2, size, strides);
  const size_t required_size = y_plane_size + u_plane_size + v_plane_size;

  if (data.size() < required_size) {
    return false;
  }

  // Limited-range black for I420/YUV.
  std::ranges::fill(data.first(y_plane_size), uint8_t{0x10});
  std::ranges::fill(data.subspan(y_plane_size, u_plane_size), uint8_t{0x80});
  std::ranges::fill(data.subspan(y_plane_size + u_plane_size, v_plane_size),
                    uint8_t{0x80});
  return true;
}

media::mojom::VideoFrameInfoPtr CreateFrameInfo(const gfx::Size& size,
                                                base::TimeDelta timestamp,
                                                base::TimeTicks reference_time,
                                                double frame_rate) {
  auto frame_info = media::mojom::VideoFrameInfo::New();

  frame_info->timestamp = timestamp;
  frame_info->pixel_format = kMockCameraPixelFormat;
  frame_info->coded_size = size;
  frame_info->visible_rect = gfx::Rect(size);
  frame_info->natural_size = size;
  frame_info->strides = CreateI420PlaneStrides(size);
  frame_info->metadata.frame_rate = frame_rate;
  frame_info->metadata.reference_time = reference_time;

  return frame_info;
}

}  // namespace

MockCameraDevice::MockCameraDevice(MockCameraConfig config)
    : config_(std::move(config)), start_time_(base::TimeTicks::Now()) {}

MockCameraDevice::~MockCameraDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();

  // Closing these Mojo endpoints is what unregisters the virtual device from
  // VirtualDeviceEnabledDeviceFactory.
  device_.reset();
  producer_receiver_.reset();

  buffers_.clear();
}

media::VideoCaptureDeviceInfo MockCameraDevice::BuildDeviceInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media::VideoCaptureDeviceInfo device_info;

  device_info.descriptor.device_id = config_.device_id;
  device_info.descriptor.set_display_name(config_.label);
  device_info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

  device_info.supported_formats = {
      media::VideoCaptureFormat(config_.size, config_.frame_rate,
                                kMockCameraPixelFormat),
  };

  return device_info;
}

mojo::PendingRemote<video_capture::mojom::Producer>
MockCameraDevice::BindProducerRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<video_capture::mojom::Producer> producer;
  producer_receiver_.Bind(producer.InitWithNewPipeAndPassReceiver());

  return producer;
}

mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
MockCameraDevice::BindDeviceReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return device_.BindNewPipeAndPassReceiver();
}

void MockCameraDevice::StartProducingFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(device_.is_bound());

  start_time_ = base::TimeTicks::Now();
  PushFrame();
}

void MockCameraDevice::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle,
    OnNewBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(buffer_handle->is_unsafe_shmem_region());

  base::WritableSharedMemoryMapping mapping =
      buffer_handle->get_unsafe_shmem_region().Map();
  CHECK(mapping.IsValid());
  buffers_[buffer_id] = std::move(mapping);

  std::move(callback).Run();
}

void MockCameraDevice::OnBufferRetired(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  buffers_.erase(buffer_id);
}

void MockCameraDevice::PushFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(device_.is_bound());

  last_frame_request_time_ = base::TimeTicks::Now();
  device_->RequestFrameBuffer(
      config_.size, kMockCameraPixelFormat,
      CreateI420PlaneStrides(config_.size),
      base::BindOnce(&MockCameraDevice::OnFrameBufferReady,
                     weak_factory_.GetWeakPtr()));
}

void MockCameraDevice::OnFrameBufferReady(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer_id == video_capture::mojom::kInvalidBufferId) {
    ScheduleNextFrame();
    return;
  }

  auto it = buffers_.find(buffer_id);
  CHECK(it != buffers_.end());
  CHECK(it->second.IsValid());

  base::span<uint8_t> mapped_memory = it->second.GetMemoryAsSpan<uint8_t>();
  const bool created_black_frame =
      FillBlackI420Frame(mapped_memory, config_.size);
  CHECK(created_black_frame);

  const base::TimeTicks reference_time = base::TimeTicks::Now();
  auto frame_info = CreateFrameInfo(config_.size, reference_time - start_time_,
                                    reference_time, config_.frame_rate);
  device_->OnFrameReadyInBuffer(buffer_id, std::move(frame_info));

  // Match the existing SharedMemoryDeviceExerciser: keep producing frames on a
  // low-rate timer and let RequestFrameBuffer() return kInvalidBufferId when
  // the pipeline cannot accept a frame yet.
  ScheduleNextFrame();
}

void MockCameraDevice::ScheduleNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks next_frame_time =
      last_frame_request_time_ + FrameIntervalForFrameRate(config_.frame_rate);
  const base::TimeDelta delay =
      std::max(base::TimeDelta(), next_frame_time - base::TimeTicks::Now());

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MockCameraDevice::PushFrame, weak_factory_.GetWeakPtr()),
      delay);
}

}  // namespace content
