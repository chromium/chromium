// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FRAME_GENERATOR_FRAME_GENERATOR_H_
#define REMOTING_TEST_FRAME_GENERATOR_FRAME_GENERATOR_H_

#include <memory>

#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// Interface for generating desktop frames for testing purposes.
class FrameGenerator {
 public:
  virtual ~FrameGenerator() = default;

  // Generates and returns a new desktop frame. This method may be slow and
  // blocking, so for performance profiling, consider generating and storing
  // the frames in memory beforehand.
  virtual std::unique_ptr<webrtc::DesktopFrame> GenerateFrame() = 0;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FRAME_GENERATOR_FRAME_GENERATOR_H_
