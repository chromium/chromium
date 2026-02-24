// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web {

class CrashKeysJsTest : public web::JavascriptTest {
 protected:
  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddUserScript(@"crash_keys_test");

    ASSERT_TRUE(LoadHtml(@"<p>"));
  }

  // Helper to set a crash key via JS.
  void SetCrashKey(NSString* key, NSString* value) {
    NSString* script = [NSString
        stringWithFormat:@"__gCrWeb.getRegisteredApi('crash_keys_tests')."
                         @"getFunction('setCrashKey')('%@', '%@')",
                         key, value];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Helper to clear a single crash key.
  void ClearCrashKey(NSString* key) {
    NSString* script = [NSString
        stringWithFormat:@"__gCrWeb.getRegisteredApi('crash_keys_tests')."
                         @"getFunction('clearCrashKey')('%@')",
                         key];
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Helper to clear all crash keys.
  void ClearAllCrashKeys() {
    web::test::ExecuteJavaScriptInWebView(
        web_view(), @"__gCrWeb.getRegisteredApi('crash_keys_tests')."
                    @"getFunction('clearAllCrashKeys')()");
  }

  // Helper to retrieve all crash keys.
  NSDictionary* GetCrashKeys() {
    return web::test::ExecuteJavaScript(
        web_view(), @"__gCrWeb.getRegisteredApi('crash_keys_tests')."
                    @"getFunction('getCrashKeys')()");
  }
};

// Tests set and get crash keys.
TEST_F(CrashKeysJsTest, SetAndGetCrashKey) {
  SetCrashKey(@"crash_key_1", @"value1");
  NSDictionary* keys = GetCrashKeys();
  EXPECT_NSEQ(@"value1", keys[@"crash_key_1"]);
  EXPECT_EQ(1u, [keys count]);

  SetCrashKey(@"crash_key_2", @"value2");
  keys = GetCrashKeys();
  EXPECT_EQ(2u, [keys count]);

  EXPECT_NSEQ(@"value1", keys[@"crash_key_1"]);
  EXPECT_NSEQ(@"value2", keys[@"crash_key_2"]);
}

// Tests clearing a specific crash key.
TEST_F(CrashKeysJsTest, ClearCrashKey) {
  SetCrashKey(@"crash_key_1", @"value1");
  SetCrashKey(@"crash_key_2", @"value2");

  NSDictionary* keys = GetCrashKeys();
  EXPECT_EQ(2u, [keys count]);
  ClearCrashKey(@"crash_key_1");
  keys = GetCrashKeys();

  EXPECT_EQ(1u, [keys count]);
  EXPECT_FALSE(keys[@"crash_key_1"]);
  EXPECT_NSEQ(@"value2", keys[@"crash_key_2"]);
}

// Tests clearing all crash keys.
TEST_F(CrashKeysJsTest, ClearAllCrashKeys) {
  SetCrashKey(@"crash_key_1", @"value1");
  SetCrashKey(@"crash_key_2", @"value2");

  NSDictionary* keys = GetCrashKeys();
  EXPECT_EQ(2u, [keys count]);
  ClearAllCrashKeys();
  keys = GetCrashKeys();

  EXPECT_EQ(0u, [keys count]);
}

}  // namespace web
