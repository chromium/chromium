// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/pasteboard_util.h"

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kTestText[] = "Some test text";
const char kTestURL[] = "https://www.chromium.org/";

class PasteboardUtilTest : public PlatformTest {
 public:
  PasteboardUtilTest() {}

  void SetUp() override { ClearPasteboard(); }

  void TearDown() override { ClearPasteboard(); }
};

// Tests that the StoreInPasteboard function properly adds two items to the
// general pasteboard.
TEST_F(PasteboardUtilTest, StoreInPasteboardWorks) {
  NSString* test_text = base::SysUTF8ToNSString(kTestText);
  GURL test_url(kTestURL);
  NSURL* test_ns_url = [NSURL URLWithString:base::SysUTF8ToNSString(kTestURL)];

  StoreInPasteboard(test_text, test_url);

  // Additional text is stored as the first pasteboard item.
  ASSERT_TRUE([UIPasteboard generalPasteboard].hasStrings);
  EXPECT_TRUE(
      [test_text isEqualToString:UIPasteboard.generalPasteboard.string]);

  // URL is stored as the second pasteboard item, but can be accessed as the
  // first (and only) URL item.
  ASSERT_TRUE([UIPasteboard generalPasteboard].hasURLs);
  EXPECT_TRUE([test_ns_url isEqual:UIPasteboard.generalPasteboard.URLs[0]]);
}

// Tests that the minimum line height attribute is reflected in GetLineHeight().
TEST_F(PasteboardUtilTest, ClearPasteboardWorks) {
  // Get something stored in the pasteboard.
  NSString* test_text = base::SysUTF8ToNSString(kTestText);
  GURL test_url(kTestURL);
  StoreInPasteboard(test_text, test_url);

  // Clear and assert.
  ClearPasteboard();
  EXPECT_FALSE([UIPasteboard generalPasteboard].hasURLs);
  EXPECT_FALSE([UIPasteboard generalPasteboard].hasStrings);
}

}  // namespace
