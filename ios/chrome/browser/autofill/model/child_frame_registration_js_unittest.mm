// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

class ChildFrameRegistrationJavascriptTest : public web::JavascriptTest {
 protected:
  ChildFrameRegistrationJavascriptTest() {}
  ~ChildFrameRegistrationJavascriptTest() override {}

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
    AddMessageScript();
    AddUserScript(@"child_frame_registration_test");
  }
};

TEST_F(ChildFrameRegistrationJavascriptTest, RegisterFrames) {
  NSString* html = @"<body> outer frame"
                    "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
                    "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
                    "</body>";

  ASSERT_TRUE(LoadHtml(html));

  id result = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.childFrameRegistrationTesting."
                  @"registerAllChildFrames();");

  ASSERT_TRUE(result);
  NSArray<NSString*>* result_array =
      base::apple::ObjCCast<NSArray<NSString*>>(result);
  ASSERT_TRUE(result_array);
  EXPECT_EQ(2u, [result_array count]);
  for (NSString* item in result_array) {
    ASSERT_EQ(32u, [item length]);
    uint64_t unused;
    EXPECT_TRUE(base::HexStringToUInt64(
        base::SysNSStringToUTF8([item substringToIndex:16]), &unused));
    EXPECT_TRUE(base::HexStringToUInt64(
        base::SysNSStringToUTF8([item substringFromIndex:16]), &unused));
  }
}

}  // namespace
