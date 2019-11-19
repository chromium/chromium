// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/ios/ios_util.h"
#include "base/macros.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for accessibility.js testing.
class FontSizeJsTest : public web::WebJsTest<web::WebTestWithWebState> {
 public:
  FontSizeJsTest()
      : web::WebJsTest<web::WebTestWithWebState>(@[ @"accessibility" ]) {}

  // Find DOM element by |element_id| and get computed font size in px.
  float GetElementFontSize(NSString* element_id) {
    NSNumber* res = ExecuteJavaScriptWithFormat(
        @"parseFloat(getComputedStyle(document.getElementById('%@'))."
        @"getPropertyValue('font-size'));",
        element_id);
    return res.floatValue;
  }

  // Wraps |html| in <html> and loads. Adds <meta name='viewport'
  // content='initial-scale=1.0'> to avoid implicit font size inflation (e.g.
  // for <div style='font-size:10px'>d<div style='font-size:10px'>d</div></div>
  // the |GetElementFontSize| returns 17px instead of 10px under default
  // viewport and '-webkit-text-size-adjust=auto'). Setting
  // '-webkit-text-size-adjust=none' also works.
  void LoadHtml(NSString* html) {
    LoadHtmlAndInject(
        [NSString stringWithFormat:@"<html><meta name='viewport' "
                                   @"content='initial-scale=1.0'>%@</html>",
                                   html]);
  }

  // Executes JavaScript "__gCrWeb.accessibility.adjustFontSize(|scale|)" to
  // adjust font size to |scale|% and return if it is executed without
  // exception.
  bool AdjustFontSize(int scale) WARN_UNUSED_RESULT {
    id script_result = ExecuteJavaScriptWithFormat(
        @"__gCrWeb.accessibility.adjustFontSize(%d); true;", scale);
    return [script_result isEqual:@YES];
  }

  DISALLOW_COPY_AND_ASSIGN(FontSizeJsTest);
};

// Tests that __gCrWeb.accessibility.adjustFontSize works for any scale.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForScale) {
  // TODO(crbug.com/983776): This test fails on ipad since beta5 due to a
  // simulator bug. Re-enable this once the bug is fixed.
  if (base::ios::IsRunningOnIOS13OrLater() &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  float original_size = 0;
  float current_size = 0;

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(20));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 20 / 100);

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(50));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 50 / 100);

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(90));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 90 / 100);

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(150));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 150 / 100);

  LoadHtml(@"<div id='e'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(200));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 200 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(20));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 20 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(50));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 50 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(90));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 90 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(150));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 150 / 100);

  LoadHtml(@"<h1 id='e'>h</h1>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(200));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 200 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(20));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 20 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(50));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 50 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(90));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 90 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(150));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 150 / 100);

  LoadHtml(@"<span id='e'>s</span>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(200));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 200 / 100);
}

// Tests that __gCrWeb.accessibility.adjustFontSize works for any CSS unit.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForUnit) {
  // TODO(crbug.com/983776): This test fails on ipad since beta5 due to a
  // simulator bug. Re-enable this once the bug is fixed.
  if (base::ios::IsRunningOnIOS13OrLater() &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  float original_size = 0;
  float current_size = 0;

  LoadHtml(@"<div id='e' style='font-size: xx-large'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 1cm'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 5mm'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 1in'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 10px'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 18pt'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 2pc'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 2.5em'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 0.8rem'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);

  LoadHtml(@"<div id='e' style='font-size: 70%'>d</div>");
  original_size = GetElementFontSize(@"e");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size = GetElementFontSize(@"e");
  EXPECT_FLOAT_EQ(current_size, original_size * 110 / 100);
}

// Tests that __gCrWeb.accessibility.adjustFontSize works for nested elements.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForNestedElements) {
  // TODO(crbug.com/983776): This test fails on ipad since beta5 due to a
  // simulator bug. Re-enable this once the bug is fixed.
  if (base::ios::IsRunningOnIOS13OrLater() &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  float original_size_1 = 0;
  float original_size_2 = 0;
  float current_size_1 = 0;
  float current_size_2 = 0;

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:xx-large'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:1cm'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:5mm'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:1in'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:18pt'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:2pc'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:2.5em'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:0.8rem'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:70%'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:10px'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:xx-large'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(
      @"<div id='e1' style='font-size: 10px'>d<div id='e2' "
      @"style='font-size:inherit'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);
}
