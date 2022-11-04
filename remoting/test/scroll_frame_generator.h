// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_
#define REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_

#include <memory>
#include <unordered_map>

#include "base/time/time.h"
#include "remoting/protocol/input_event_timestamps.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {
namespace test {

class ScrollFrameGenerator : public protocol::InputEventTimestampsSource {
 public:
  ScrollFrameGenerator();

  ScrollFrameGenerator(const ScrollFrameGenerator&) = delete;
  ScrollFrameGenerator& operator=(const ScrollFrameGenerator&) = delete;

  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::SharedMemoryFactory* shared_memory_factory);

  // InputEventTimestampsSource interface.
  protocol::InputEventTimestamps TakeLastEventTimestamps() override;

 private:
  ~ScrollFrameGenerator() override;

  std::unique_ptr<webrtc::DesktopFrame> base_frame_;
  base::TimeTicks start_time_;

  std::unordered_map<int, base::TimeTicks> frame_timestamp_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_
