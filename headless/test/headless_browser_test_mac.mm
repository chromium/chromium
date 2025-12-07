// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace headless {
namespace {

class HeadlessCustomScreenSizeBrowserTest : public HeadlessBrowserTest {
 public:
  static constexpr int kScreenWidth = 1234;
  static constexpr int kScreenHeight = 5678;

  HeadlessCustomScreenSizeBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string screen_info =
        base::StringPrintf("{%dx%d}", kScreenWidth, kScreenHeight);
    command_line->AppendSwitchASCII(switches::kScreenInfo, screen_info);
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessCustomScreenSizeBrowserTest,
                       ScreenCoordinateConversion) {
  display::Screen& screen = CHECK_DEREF(display::Screen::Get());
  display::Display primary_display = screen.GetPrimaryDisplay();
  ASSERT_THAT(primary_display.bounds(),
              testing::Eq(gfx::Rect(0, 0, kScreenWidth, kScreenHeight)));

  // If NSScreen.frame is not overridden to return HeadlessScreen.bounds(), the
  // Cocoa vertical coorditates conversion will be done using physical screen
  // height which most probably is not 5678, so converted vertical coordinate
  // will not be what we expect.
  const gfx::Rect rect(100, 200, 300, 400);
  NSRect ns_rect = gfx::ScreenRectToNSRect(rect);
  EXPECT_EQ(ns_rect.origin.x, rect.x());
  EXPECT_EQ(ns_rect.origin.y, kScreenHeight - rect.y() - rect.height());
  EXPECT_EQ(ns_rect.size.width, rect.width());
  EXPECT_EQ(ns_rect.size.height, rect.height());
}

}  // namespace
}  // namespace headless
