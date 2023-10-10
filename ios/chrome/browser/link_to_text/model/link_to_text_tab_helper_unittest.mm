// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"

#import "base/gtest_prod_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class LinkToTextTabHelperTest : public PlatformTest {
 protected:
  LinkToTextTabHelper* tab_helper() {
    return LinkToTextTabHelper::FromWebState(&fake_web_state_);
  }

  void CreateTabHelper() {
    LinkToTextTabHelper::CreateForWebState(&fake_web_state_);
  }

  web::FakeWebState fake_web_state_;
};

TEST_F(LinkToTextTabHelperTest, IsOnlyBoundaryChars) {
  CreateTabHelper();

  // Simple string of characters in various alphabets
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@"Chrome"));
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@"クローム"));
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@"鉻"));
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@"хром"));

  // Various combinations of boundary/non-boundary
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@" .hello"));
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@"goodbye !\t"));
  EXPECT_FALSE(tab_helper()->IsOnlyBoundaryChars(@" .,! some _ words !."));

  // Assorted strings of only punctuation/whitespace, plus empty
  EXPECT_TRUE(tab_helper()->IsOnlyBoundaryChars(@"     "));
  EXPECT_TRUE(tab_helper()->IsOnlyBoundaryChars(@"!!.\\/-"));
  EXPECT_TRUE(tab_helper()->IsOnlyBoundaryChars(@"（」。、\n"));
  EXPECT_TRUE(tab_helper()->IsOnlyBoundaryChars(@""));

  // Special case: if a string starts with a ton of whitespace, we won't find a
  // subsequent word character because the regex execution is limited.
  NSString* longString =
      [[@"" stringByPaddingToLength:500 withString:@" "
                    startingAtIndex:0] stringByAppendingString:@"a"];
  EXPECT_TRUE(tab_helper()->IsOnlyBoundaryChars(longString));
}
