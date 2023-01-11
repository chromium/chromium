// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/scroll_frame_generator.h"

#include "remoting/test/frame_generator_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace test {

namespace {
int kScrollSpeedPixelsPerSecond = 500;
}  // namespace

ScrollFrameGenerator::ScrollFrameGenerator()
    : base_frame_(LoadDesktopFrameFromPng("test_frame2.png")),
      start_time_(base::TimeTicks::Now()) {}
ScrollFrameGenerator::~ScrollFrameGenerator() = default;

std::unique_ptr<webrtc::DesktopFrame> ScrollFrameGenerator::GenerateFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  base::TimeTicks now = base::TimeTicks::Now();
  int position = static_cast<int>(kScrollSpeedPixelsPerSecond *
                                  (now - start_time_).InSecondsF()) %
                 base_frame_->size().height();
  std::unique_ptr<webrtc::DesktopFrame> result(
      new webrtc::BasicDesktopFrame(base_frame_->size()));

  int top_height = base_frame_->size().height() - position;

  result->CopyPixelsFrom(*base_frame_, webrtc::DesktopVector(0, position),
                         webrtc::DesktopRect::MakeLTRB(
                             0, 0, base_frame_->size().width(), top_height));
  result->CopyPixelsFrom(
      *base_frame_, webrtc::DesktopVector(0, 0),
      webrtc::DesktopRect::MakeLTRB(0, top_height, base_frame_->size().width(),
                                    base_frame_->size().height()));

  result->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(result->size()));

  return result;
}

protocol::InputEventTimestamps ScrollFrameGenerator::TakeLastEventTimestamps() {
  base::TimeTicks now = base::TimeTicks::Now();
  return protocol::InputEventTimestamps{now, now};
}

}  // namespace test
}  // namespace remoting
