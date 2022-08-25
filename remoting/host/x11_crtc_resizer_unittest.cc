// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_crtc_resizer.h"

#include "inttypes.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

using webrtc::DesktopRect;
using Crtc = x11::RandR::Crtc;
using Mode = x11::RandR::Mode;

namespace {

std::string PrettyPrint(const DesktopRect& rect) {
  // Example output: 10x20+30-40
  return base::StringPrintf("%" PRId32 "x%" PRId32 "%+" PRId32 "%+" PRId32,
                            rect.width(), rect.height(), rect.left(),
                            rect.top());
}

void ExpectEqual(const DesktopRect& rect1, const DesktopRect& rect2) {
  if (!rect1.equals(rect2)) {
    ADD_FAILURE() << "Expected equality of [" << PrettyPrint(rect1) << "] and ["
                  << PrettyPrint(rect2) << "]";
  }
}

}  // namespace

namespace remoting {

TEST(X11CrtcResizerTest, ResizeInPlace) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 0, .width = 100, .height = 100},
      {.x = 100, .y = 100, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {10, 20});
  resizer.UpdateActiveCrtcs(Crtc(2), Mode(0), {30, 40});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 10, 20));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(100, 100, 30, 40));
}

TEST(X11CrtcResizerTest, ShiftToMakeRoom) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 0, .width = 100, .height = 100},
      {.x = 100, .y = 0, .width = 100, .height = 100},
      {.x = 0, .y = 100, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {300, 200});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 300, 200));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(300, 0, 100, 100));
  ExpectEqual(result[2], DesktopRect::MakeXYWH(0, 200, 100, 100));
}

}  // namespace remoting
