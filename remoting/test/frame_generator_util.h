// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FRAME_GENERATOR_UTIL_H_
#define REMOTING_TEST_FRAME_GENERATOR_UTIL_H_

#include <memory>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
namespace test {

// Loads test image from remoting/test/data.
std::unique_ptr<webrtc::DesktopFrame> LoadDesktopFrameFromPng(const char* name);

// Draws rectangle filled with the given |color|.
void DrawRect(webrtc::DesktopFrame* frame,
              webrtc::DesktopRect rect,
              uint32_t color);

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FRAME_GENERATOR_UTIL_H_
