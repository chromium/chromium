// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_video_capture_client.h"

#include <string>
#include <utility>

using testing::_;
using testing::Invoke;

namespace media {
namespace unittest_internal {

MockVideoCaptureClient::MockVideoCaptureClient() {
  ON_CALL(*this, OnError(_, _, _))
      .WillByDefault(Invoke(this, &MockVideoCaptureClient::DumpError));
}

MockVideoCaptureClient::~MockVideoCaptureClient() {
  if (quit_cb_) {
    std::move(quit_cb_).Run();
  }
}

void MockVideoCaptureClient::SetFrameCb(base::OnceClosure frame_cb) {
  frame_cb_ = std::move(frame_cb);
}

void MockVideoCaptureClient::SetQuitCb(base::OnceClosure quit_cb) {
  quit_cb_ = std::move(quit_cb);
}

void MockVideoCaptureClient::DumpError(media::VideoCaptureError,
                                       const base::Location& location,
                                       const std::string& message) {
  DPLOG(ERROR) << location.ToString() << " " << message;
}

void MockVideoCaptureClient::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    int rotation,
    bool flip_y,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    int frame_feedback_id) {
  ASSERT_GT(length, 0);
  ASSERT_TRUE(data);
  if (frame_cb_)
    std::move(frame_cb_).Run();
}

void MockVideoCaptureClient::OnIncomingCapturedGfxBuffer(
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    int frame_feedback_id) {
  ASSERT_TRUE(buffer);
  ASSERT_GT(buffer->GetSize().width() * buffer->GetSize().height(), 0);
  if (frame_cb_)
    std::move(frame_cb_).Run();
}

void MockVideoCaptureClient::OnIncomingCapturedExternalBuffer(
    CapturedExternalVideoBuffer buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    const gfx::Rect& visible_rect) {
  if (frame_cb_)
    std::move(frame_cb_).Run();
}

// Trampoline methods to workaround GMOCK problems with std::unique_ptr<>.
VideoCaptureDevice::Client::ReserveResult
MockVideoCaptureClient::ReserveOutputBuffer(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    int frame_feedback_id,
    VideoCaptureDevice::Client::Buffer* buffer,
    int* require_new_buffer_id,
    int* retire_old_buffer_id) {
  DoReserveOutputBuffer();
  NOTREACHED_IN_MIGRATION() << "This should never be called";
  return ReserveResult::kSucceeded;
}

void MockVideoCaptureClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time) {
  DoOnIncomingCapturedBuffer();
}

void MockVideoCaptureClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_time,
    gfx::Rect visible_rect,
    const VideoFrameMetadata& additional_metadata) {
  DoOnIncomingCapturedVideoFrame();
}

}  // namespace unittest_internal
}  // namespace media
