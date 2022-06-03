// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_video_renderer.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

FakeVideoStub::FakeVideoStub() = default;
FakeVideoStub::~FakeVideoStub() = default;

void FakeVideoStub::set_on_frame_callback(
    const base::RepeatingClosure& on_frame_callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  on_frame_callback_ = on_frame_callback;
}

void FakeVideoStub::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    base::OnceClosure done) {
  CHECK(thread_checker_.CalledOnValidThread());
  received_packets_.push_back(std::move(video_packet));
  if (!done.is_null())
    std::move(done).Run();
  if (!on_frame_callback_.is_null())
    on_frame_callback_.Run();
}

FakeFrameConsumer::FakeFrameConsumer() = default;
FakeFrameConsumer::~FakeFrameConsumer() = default;

void FakeFrameConsumer::set_on_frame_callback(
    const base::RepeatingClosure& on_frame_callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  on_frame_callback_ = on_frame_callback;
}

std::unique_ptr<webrtc::DesktopFrame> FakeFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  CHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<webrtc::BasicDesktopFrame>(size);
}

void FakeFrameConsumer::DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                  base::OnceClosure done) {
  CHECK(thread_checker_.CalledOnValidThread());
  received_frames_.push_back(std::move(frame));
  if (done)
    std::move(done).Run();
  if (on_frame_callback_)
    on_frame_callback_.Run();
}

FrameConsumer::PixelFormat FakeFrameConsumer::GetPixelFormat() {
  CHECK(thread_checker_.CalledOnValidThread());
  return FORMAT_BGRA;
}

FakeFrameStatsConsumer::FakeFrameStatsConsumer() = default;
FakeFrameStatsConsumer::~FakeFrameStatsConsumer() = default;

void FakeFrameStatsConsumer::set_on_stats_callback(
    const base::RepeatingClosure& on_stats_callback) {
  on_stats_callback_ = on_stats_callback;
}

void FakeFrameStatsConsumer::OnVideoFrameStats(const FrameStats& stats) {
  CHECK(thread_checker_.CalledOnValidThread());
  received_stats_.push_back(stats);
  if (!on_stats_callback_.is_null())
    on_stats_callback_.Run();
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
  CHECK(thread_checker_.CalledOnValidThread());
  return &video_stub_;
}

FakeFrameConsumer* FakeVideoRenderer::GetFrameConsumer() {
  CHECK(thread_checker_.CalledOnValidThread());
  return &frame_consumer_;
}

FakeFrameStatsConsumer* FakeVideoRenderer::GetFrameStatsConsumer() {
  CHECK(thread_checker_.CalledOnValidThread());
  return &frame_stats_consumer_;
}

}  // namespace protocol
}  // namespace remoting
