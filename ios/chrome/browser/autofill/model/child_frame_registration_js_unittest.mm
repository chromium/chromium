// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

using ::testing::IsSubsetOf;
using ::testing::SizeIs;

// Extract all tokens from the JS registration call `result` while skipping the
// tokens that can't be deserialized.
std::vector<autofill::RemoteFrameToken> ExtractTokensFromResult(id result) {
  if (!result) {
    return {};
  }
  NSArray<NSString*>* result_array =
      base::apple::ObjCCast<NSArray<NSString*>>(result);
  if (!result_array) {
    return {};
  }

  std::vector<autofill::RemoteFrameToken> extracted_tokens;
  for (NSString* item in result_array) {
    std::optional<base::UnguessableToken> token =
        autofill::DeserializeJavaScriptFrameId(base::SysNSStringToUTF8(item));
    if (token) {
      extracted_tokens.emplace_back(*token);
    }
  }

  return extracted_tokens;
}

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

// Tests that child frames register themselves correctly with their host frame.
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

// Tests that the registration tokens are cached and can be reused.
TEST_F(ChildFrameRegistrationJavascriptTest, RegisterFrames_Cache) {
  NSString* html = @"<body> outer frame"
                    "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
                    "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
                    "</body>";

  ASSERT_TRUE(LoadHtml(html));

  // Do first registration and extract the tokens.
  id result1 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.childFrameRegistrationTesting."
                  @"registerAllChildFrames();");
  ASSERT_TRUE(result1);
  std::vector<autofill::RemoteFrameToken> remote_tokens_round1 =
      ExtractTokensFromResult(result1);
  EXPECT_THAT(remote_tokens_round1, SizeIs(2));

  // Do second registration and extract the tokens.
  id result2 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.childFrameRegistrationTesting."
                  @"registerAllChildFrames();");
  ASSERT_TRUE(result2);
  std::vector<autofill::RemoteFrameToken> remote_tokens_round2 =
      ExtractTokensFromResult(result2);
  EXPECT_THAT(remote_tokens_round2, SizeIs(2));

  // Verify that the cached tokens were reused when registring a second time.
  EXPECT_THAT(remote_tokens_round2, IsSubsetOf(remote_tokens_round1));
}

}  // namespace
