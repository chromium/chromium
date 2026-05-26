// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

// Test fixture for aim_cobrowse.ts in the Page Content World.
class AimCobrowseJavaScriptTest : public web::JavascriptTest {
 protected:
  void SetUp() override {
    JavascriptTest::SetUp();
    AddGCrWebScript();
    AddUserScript(@"aim_cobrowse");
    ASSERT_TRUE(LoadHtml(@"<html><body></body></html>"));
  }
};

// Tests that __gAimCobrowse API is registered correctly on the window.
TEST_F(AimCobrowseJavaScriptTest, AimCobrowseExposedOnWindow) {
  id typeof_g_aim_cobrowse =
      web::test::ExecuteJavaScript(web_view(), @"typeof window.__gAimCobrowse");
  EXPECT_NSEQ(@"object", typeof_g_aim_cobrowse);

  id typeof_send_web_to_native = web::test::ExecuteJavaScript(
      web_view(), @"typeof window.__gAimCobrowse.sendWebToNative");
  EXPECT_NSEQ(@"function", typeof_send_web_to_native);
}
