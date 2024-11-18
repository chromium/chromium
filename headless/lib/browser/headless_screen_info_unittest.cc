// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen_info.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

TEST(ScreenInfoTest, Basic) {
  EXPECT_EQ(HeadlessScreenInfo::FromString(" \t ").error(),
            "Invalid screen info:  \t ");

  EXPECT_EQ(HeadlessScreenInfo::FromString(" xyz ").error(),
            "Invalid screen info:  xyz ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{").error(),
            "Invalid screen info: {");

  EXPECT_EQ(HeadlessScreenInfo::FromString("}").error(),
            "Invalid screen info: }");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(0, 0, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{  }").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(0, 0, 800, 600)}));
}

TEST(ScreenInfoTest, ScreenOrigin) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{100,200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString(" { 100,200 }").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{-100,200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(-100, 200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{100,-200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, -200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{-100,-200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(-100, -200, 800, 600)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100, 200}").error(),
            "Invalid screen info: 100, 200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz 100,200}").error(),
            "Invalid screen info: xyz 100,200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz100,200}").error(),
            "Invalid screen info: xyz100,200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100,200 xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100,200xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100+200 }").error(),
            "Invalid screen info: 100+200");
}

TEST(ScreenInfoTest, ScreenSize) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{100x200}").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString(" { 100x200 } ").value()[0],
            HeadlessScreenInfo({.bounds = gfx::Rect(100, 200)}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x 200}").error(),
            "Invalid screen info: 100x 200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz 100x200}").error(),
            "Invalid screen info: xyz 100x200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz100x200}").error(),
            "Invalid screen info: xyz100x200");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x200 xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100x200xyz}").error(),
            "Invalid screen info: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ 100 200 }").error(),
            "Invalid screen info: 100 200");
}

TEST(ScreenInfoTest, ScreenParameters) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz =}").error(),
            "Invalid screen info: xyz =");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz = 42}").error(),
            "Invalid screen info: xyz = 42");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=42}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=42}").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz= }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{xyz=42 }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz= }").error(),
            "Unknown screen info parameter: xyz");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ xyz=42 }").error(),
            "Unknown screen info parameter: xyz");
}

TEST(ScreenInfoTest, ScreenColorDepth) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=16 }").value()[0],
            HeadlessScreenInfo({.color_depth = 16}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth= 16 }").error(),
            "Invalid screen color depth: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=0 }").error(),
            "Invalid screen color depth: 0");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=x24 }").error(),
            "Invalid screen color depth: x24");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ colorDepth=24x }").error(),
            "Invalid screen color depth: 24x");
}

TEST(ScreenInfoTest, ScreenDevicePixelRatio) {
  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ devicePixelRatio=0.5}").value()[0],
      HeadlessScreenInfo({.device_pixel_ratio = 0.5f}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=4 }").value()[0],
            HeadlessScreenInfo({.device_pixel_ratio = 4.0f}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=0.1 }").error(),
            "Invalid screen device pixel ratio: 0.1");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio= 1.0 }").error(),
            "Invalid screen device pixel ratio: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ devicePixelRatio=x1.0 }").error(),
            "Invalid screen device pixel ratio: x1.0");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{devicePixelRatio=1.0x }").error(),
            "Invalid screen device pixel ratio: 1.0x");
}

TEST(ScreenInfoTest, ScreenIsInternal) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=1 }").value()[0],
            HeadlessScreenInfo({.is_internal = true}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=true }").value()[0],
            HeadlessScreenInfo({.is_internal = true}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=0 }").value()[0],
            HeadlessScreenInfo({.is_internal = false}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=false }").value()[0],
            HeadlessScreenInfo({.is_internal = false}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal= }").error(),
            "Invalid screen is internal: ");

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ isInternal=xyz }").error(),
            "Invalid screen is internal: xyz");
}

TEST(ScreenInfoTest, ScreenLabel) {
  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label=xyz}").value()[0],
            HeadlessScreenInfo({.label = "xyz"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='xyz'}").value()[0],
            HeadlessScreenInfo({.label = "xyz"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label=''}").value()[0],
            HeadlessScreenInfo({.label = ""}));

  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ label='primary screen'}").value()[0],
      HeadlessScreenInfo({.label = "primary screen"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='my \\'quoted\\' screen'}")
                .value()[0],
            HeadlessScreenInfo({.label = "my 'quoted' screen"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='\\'quoted\\' screen'}")
                .value()[0],
            HeadlessScreenInfo({.label = "'quoted' screen"}));

  EXPECT_EQ(
      HeadlessScreenInfo::FromString("{ label='\\'quoted\\''}").value()[0],
      HeadlessScreenInfo({.label = "'quoted'"}));

  EXPECT_EQ(HeadlessScreenInfo::FromString("{ label='\\'quoted\\'}").error(),
            "Invalid screen info: '\\'quoted\\'");
}

TEST(ScreenInfoTest, MultipleScreens) {
  // Explicit screen origin results in overlapped screens.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{0,0 600x800}").value(),
              testing::ElementsAre(
                  HeadlessScreenInfo(),
                  HeadlessScreenInfo({.bounds = gfx::Rect(600, 800)})));

  // Default screen origin results in side by side screens.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{}").value(),
              testing::ElementsAre(
                  HeadlessScreenInfo(),
                  HeadlessScreenInfo({.bounds = gfx::Rect(800, 0, 800, 600)})));

  // Screen info separators.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} {}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{},{}").value(),
              testing::SizeIs(2));

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} , {}").value(),
              testing::SizeIs(2));

  // Malformed.
  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{").error(),
              "Invalid screen info: {");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{}{ xyz").error(),
              "Invalid screen info: { xyz");

  EXPECT_THAT(HeadlessScreenInfo::FromString("xyz{}").error(),
              "Invalid screen info: xyz{}");

  EXPECT_THAT(HeadlessScreenInfo::FromString("xyz {}").error(),
              "Invalid screen info: xyz {}");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{}xyz").error(),
              "Invalid screen info: xyz");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} xyz }").error(),
              "Invalid screen info: xyz }");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{ {}").error(),
              "Invalid screen info: {");

  EXPECT_THAT(HeadlessScreenInfo::FromString("{} }").error(),
              "Invalid screen info: }");
}

}  // namespace
}  // namespace headless
