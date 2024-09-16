// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stddef.h>

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

// Test fixture for accessibility.js testing.
class FontSizeJsTest : public PlatformTest {
 public:
  FontSizeJsTest() : web_client_(std::make_unique<web::FakeWebClient>()) {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  FontSizeJsTest(const FontSizeJsTest&) = delete;
  FontSizeJsTest& operator=(const FontSizeJsTest&) = delete;

  // Find DOM element by `element_id` and get computed font size in px.
  float GetElementFontSize(NSString* element_id) {
    NSNumber* res = web::test::ExecuteJavaScript(
        [NSString
            stringWithFormat:
                @"parseFloat(getComputedStyle(document.getElementById('%@'))."
                @"getPropertyValue('font-size'));",
                element_id],
        web_state());
    return res.floatValue;
  }

  // Wraps `html` in <html> and loads. Adds <meta name='viewport'
  // content='initial-scale=1.0'> to avoid implicit font size inflation (e.g.
  // for <div style='font-size:10px'>d<div style='font-size:10px'>d</div></div>
  // the `GetElementFontSize` returns 17px instead of 10px under default
  // viewport and '-webkit-text-size-adjust=auto'). Setting
  // '-webkit-text-size-adjust=none' also works.
  void LoadHtml(NSString* html) {
    web::test::LoadHtml(
        [NSString stringWithFormat:@"<html><style>"
                                   @"html { -webkit-text-size-adjust: none }"
                                   @"</style><meta name='viewport' "
                                   @"content='initial-scale=1.0'>%@</html>",
                                   html],
        web_state());

    // Main web injection should have occurred.
    ASSERT_NSEQ(@"object",
                web::test::ExecuteJavaScript(@"typeof __gCrWeb", web_state()));
    web::test::ExecuteJavaScript(web::test::GetPageScript(@"font_size"),
                                 web_state());
  }

  // Executes JavaScript "__gCrWeb.font_size.adjustFontSize(`scale`)" to
  // adjust font size to `scale`% and return if it is executed without
  // exception.
  [[nodiscard]] bool AdjustFontSize(int scale) {
    id script_result = web::test::ExecuteJavaScript(
        [NSString
            stringWithFormat:@"__gCrWeb.font_size.adjustFontSize(%d); true;",
                             scale],
        web_state());
    return [script_result isEqual:@YES];
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that __gCrWeb.font_size.adjustFontSize works for any scale.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForScale) {
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

// Tests that __gCrWeb.font_size.adjustFontSize works for any CSS unit.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForUnit) {
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

// Tests that __gCrWeb.font_size.adjustFontSize works for nested elements.
TEST_F(FontSizeJsTest, TestAdjustFontSizeForNestedElements) {
  float original_size_1 = 0;
  float original_size_2 = 0;
  float current_size_1 = 0;
  float current_size_2 = 0;

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:xx-large'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:1cm'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:5mm'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:1in'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:18pt'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:2pc'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:2.5em'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:0.8rem'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:70%'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:10px'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:xx-large'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);

  LoadHtml(@"<div id='e1' style='font-size: 10px'>d<div id='e2' "
           @"style='font-size:inherit'>d</div></div>");
  original_size_1 = GetElementFontSize(@"e1");
  original_size_2 = GetElementFontSize(@"e2");
  ASSERT_TRUE(AdjustFontSize(110));
  current_size_1 = GetElementFontSize(@"e1");
  current_size_2 = GetElementFontSize(@"e2");
  EXPECT_FLOAT_EQ(current_size_1, original_size_1 * 110 / 100);
  EXPECT_FLOAT_EQ(current_size_2, original_size_2 * 110 / 100);
}
