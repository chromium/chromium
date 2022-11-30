// Copyright 2022 The Chromium Authors
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

TEST(X11CrtcResizerTest, ShiftToMakeRoomHorizontally) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 0, .width = 100, .height = 100},
      {.x = 100, .y = 0, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {300, 200});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 300, 200));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(300, 0, 100, 100));
}

TEST(X11CrtcResizerTest, ShiftToMakeRoomVertically) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 0, .width = 100, .height = 100},
      {.x = 0, .y = 100, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {300, 200});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 300, 200));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(0, 200, 100, 100));
}

TEST(X11CrtcResizerTest, HorizontalLayoutPreferred) {
  X11CrtcResizer resizer(nullptr, nullptr);
  // Simple diagonal placement, neither horizontal nor vertical.
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 100, .width = 50, .height = 50},
      {.x = 100, .y = 0, .width = 50, .height = 50},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {60, 60});

  auto result = resizer.GetCrtcsForTest();
  // 1st monitor should be moved up to the origin and resized. 2nd monitor
  // should be placed to its right.
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 60, 60));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(60, 0, 50, 50));
}

TEST(X11CrtcResizerTest, RightAlignmentKept) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 50, .y = 0, .width = 50, .height = 50},
      {.x = 0, .y = 100, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {200, 200});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 200, 200));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(100, 200, 100, 100));
}

TEST(X11CrtcResizerTest, BottomAlignmentKept) {
  X11CrtcResizer resizer(nullptr, nullptr);
  resizer.SetCrtcsForTest({
      {.x = 0, .y = 50, .width = 50, .height = 50},
      {.x = 100, .y = 0, .width = 100, .height = 100},
  });

  resizer.UpdateActiveCrtcs(Crtc(1), Mode(0), {200, 200});

  auto result = resizer.GetCrtcsForTest();
  ExpectEqual(result[0], DesktopRect::MakeXYWH(0, 0, 200, 200));
  ExpectEqual(result[1], DesktopRect::MakeXYWH(200, 100, 100, 100));
}

}  // namespace remoting
