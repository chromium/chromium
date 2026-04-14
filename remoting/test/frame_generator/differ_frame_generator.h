// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FRAME_GENERATOR_DIFFER_FRAME_GENERATOR_H_
#define REMOTING_TEST_FRAME_GENERATOR_DIFFER_FRAME_GENERATOR_H_

#include <memory>

#include "remoting/test/frame_generator/frame_generator.h"

namespace webrtc {
class DesktopFrame;
class SharedDesktopFrame;
}  // namespace webrtc

namespace remoting {

// A FrameGenerator wrapper that calculates the updated_region() by comparing
// the current frame with the previous one.
class DifferFrameGenerator : public FrameGenerator {
 public:
  explicit DifferFrameGenerator(std::unique_ptr<FrameGenerator> base_generator);
  ~DifferFrameGenerator() override;

  // FrameGenerator implementation.
  // Note: This implementation clears the updated_region() of the underlying
  // frame and replaces it with its own block-based calculation.
  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame() override;

 private:
  std::unique_ptr<FrameGenerator> base_generator_;
  std::unique_ptr<webrtc::SharedDesktopFrame> last_frame_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FRAME_GENERATOR_DIFFER_FRAME_GENERATOR_H_
