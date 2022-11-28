// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/cyclic_frame_generator.h"

#include <ostream>

#include "base/numerics/safe_conversions.h"
#include "base/time/default_tick_clock.h"
#include "remoting/test/frame_generator_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace test {

CyclicFrameGenerator::ChangeInfo::ChangeInfo() = default;
CyclicFrameGenerator::ChangeInfo::ChangeInfo(ChangeType type,
                                             base::TimeTicks timestamp)
    : type(type), timestamp(timestamp) {}

// static
scoped_refptr<CyclicFrameGenerator> CyclicFrameGenerator::Create() {
  std::vector<std::unique_ptr<webrtc::DesktopFrame>> frames;
  frames.push_back(LoadDesktopFrameFromPng("test_frame1.png"));
  frames.push_back(LoadDesktopFrameFromPng("test_frame2.png"));
  return new CyclicFrameGenerator(std::move(frames));
}

CyclicFrameGenerator::CyclicFrameGenerator(
    std::vector<std::unique_ptr<webrtc::DesktopFrame>> reference_frames)
    : reference_frames_(std::move(reference_frames)),
      clock_(base::DefaultTickClock::GetInstance()),
      started_time_(clock_->NowTicks()) {
  CHECK(!reference_frames_.empty());
  screen_size_ = reference_frames_[0]->size();
  for (const auto& frame : reference_frames_) {
    CHECK(screen_size_.equals(frame->size()))
        << "All reference frames should have the same size.";
  }
}

CyclicFrameGenerator::~CyclicFrameGenerator() = default;

void CyclicFrameGenerator::SetTickClock(const base::TickClock* tick_clock) {
  clock_ = tick_clock;
  started_time_ = clock_->NowTicks();
}

std::unique_ptr<webrtc::DesktopFrame> CyclicFrameGenerator::GenerateFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  base::TimeTicks now = clock_->NowTicks();

  int frame_id = base::ClampFloor((now - started_time_) / cursor_blink_period_);
  int reference_frame =
      base::ClampFloor((now - started_time_) / frame_cycle_period_) %
      reference_frames_.size();
  bool cursor_state = frame_id % 2;

  auto frame = std::make_unique<webrtc::BasicDesktopFrame>(screen_size_);
  frame->CopyPixelsFrom(*reference_frames_[reference_frame],
                        webrtc::DesktopVector(),
                        webrtc::DesktopRect::MakeSize(screen_size_));

  // Render the cursor.
  webrtc::DesktopRect cursor_rect =
      webrtc::DesktopRect::MakeXYWH(20, 20, 2, 20);
  if (cursor_state) {
    DrawRect(frame.get(), cursor_rect, 0);
  }

  if (last_reference_frame_ != reference_frame) {
    // The whole frame has changed.
    frame->mutable_updated_region()->AddRect(
        webrtc::DesktopRect::MakeSize(screen_size_));
    last_frame_type_ = ChangeType::FULL;
  } else if (last_cursor_state_ != cursor_state) {
    // Cursor state has changed.
    frame->mutable_updated_region()->AddRect(cursor_rect);
    last_frame_type_ = ChangeType::CURSOR;
  } else {
    // No changes.
    last_frame_type_ = ChangeType::NO_CHANGES;
  }

  last_reference_frame_ = reference_frame;
  last_cursor_state_ = cursor_state;

  return frame;
}

CyclicFrameGenerator::ChangeInfoList CyclicFrameGenerator::GetChangeList(
    base::TimeTicks timestamp) {
  int frame_id =
      base::ClampFloor((timestamp - started_time_) / cursor_blink_period_);
  CHECK_GE(frame_id, last_identifier_frame_);

  ChangeInfoList result;
  const int frames_in_cycle =
      base::ClampFloor(frame_cycle_period_ / cursor_blink_period_);
  for (int i = last_identifier_frame_ + 1; i <= frame_id; ++i) {
    ChangeType type =
        (i % frames_in_cycle == 0) ? ChangeType::FULL : ChangeType::CURSOR;
    result.emplace_back(type, started_time_ + i * cursor_blink_period_);
  }
  last_identifier_frame_ = frame_id;

  return result;
}

protocol::InputEventTimestamps CyclicFrameGenerator::TakeLastEventTimestamps() {
  base::TimeTicks now = clock_->NowTicks();
  return protocol::InputEventTimestamps{now, now};
}

}  // namespace test
}  // namespace remoting
