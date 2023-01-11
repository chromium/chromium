// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_video_renderer.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting::protocol {

FakeVideoStub::FakeVideoStub() = default;
FakeVideoStub::~FakeVideoStub() = default;

void FakeVideoStub::set_on_frame_callback(
    const base::RepeatingClosure& on_frame_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_frame_callback_ = on_frame_callback;
}

void FakeVideoStub::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  received_packets_.push_back(std::move(video_packet));
  if (!done.is_null()) {
    std::move(done).Run();
  }
  if (!on_frame_callback_.is_null()) {
    on_frame_callback_.Run();
  }
}

FakeFrameConsumer::FakeFrameConsumer() = default;
FakeFrameConsumer::~FakeFrameConsumer() = default;

void FakeFrameConsumer::set_on_frame_callback(
    const base::RepeatingClosure& on_frame_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_frame_callback_ = on_frame_callback;
}

std::unique_ptr<webrtc::DesktopFrame> FakeFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<webrtc::BasicDesktopFrame>(size);
}

void FakeFrameConsumer::DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                  base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  received_frames_.push_back(std::move(frame));
  if (done) {
    std::move(done).Run();
  }
  if (on_frame_callback_) {
    on_frame_callback_.Run();
  }
}

FrameConsumer::PixelFormat FakeFrameConsumer::GetPixelFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return FORMAT_BGRA;
}

FakeFrameStatsConsumer::FakeFrameStatsConsumer() = default;
FakeFrameStatsConsumer::~FakeFrameStatsConsumer() = default;

void FakeFrameStatsConsumer::set_on_stats_callback(
    const base::RepeatingClosure& on_stats_callback) {
  on_stats_callback_ = on_stats_callback;
}

void FakeFrameStatsConsumer::OnVideoFrameStats(const FrameStats& stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  received_stats_.push_back(stats);
  if (!on_stats_callback_.is_null()) {
    on_stats_callback_.Run();
  }
}

FakeVideoRenderer::FakeVideoRenderer() = default;
FakeVideoRenderer::~FakeVideoRenderer() = default;

bool FakeVideoRenderer::Initialize(
    const ClientContext& client_context,
    protocol::FrameStatsConsumer* stats_consumer) {
  return true;
}

void FakeVideoRenderer::OnSessionConfig(const SessionConfig& config) {}

FakeVideoStub* FakeVideoRenderer::GetVideoStub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return &video_stub_;
}

FakeFrameConsumer* FakeVideoRenderer::GetFrameConsumer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return &frame_consumer_;
}

FakeFrameStatsConsumer* FakeVideoRenderer::GetFrameStatsConsumer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return &frame_stats_consumer_;
}

}  // namespace remoting::protocol
