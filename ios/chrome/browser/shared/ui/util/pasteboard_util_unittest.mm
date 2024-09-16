// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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
  NSData* text_as_data = [test_text dataUsingEncoding:NSUTF8StringEncoding];
  GURL test_url(kTestURL);
  NSString* url_as_string = base::SysUTF8ToNSString(kTestURL);
  NSData* url_as_data = [url_as_string dataUsingEncoding:NSUTF8StringEncoding];
  NSURL* test_ns_url = [NSURL URLWithString:url_as_string];

  StoreInPasteboard(test_text, test_url);

  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasStrings);
  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasURLs);

  NSString* plainTextType = UTTypeUTF8PlainText.identifier;

  // URL is stored as the first pasteboard item as both URL and plain text; the
  // latter being required on iOS 12 to be pasted in text boxes in other apps.
  NSIndexSet* firstIndex = [NSIndexSet indexSetWithIndex:0];
  EXPECT_TRUE(
      [test_ns_url isEqual:[UIPasteboard.generalPasteboard
                               valuesForPasteboardType:UTTypeURL.identifier
                                             inItemSet:firstIndex][0]]);
  EXPECT_NSEQ(url_as_data, [UIPasteboard.generalPasteboard
                               dataForPasteboardType:plainTextType
                                           inItemSet:firstIndex][0]);

  // The additional text is stored as the second item.
  NSIndexSet* secondIndex = [NSIndexSet indexSetWithIndex:1];
  EXPECT_NSEQ(text_as_data, [UIPasteboard.generalPasteboard
                                dataForPasteboardType:plainTextType
                                            inItemSet:secondIndex][0]);
  EXPECT_NSEQ(test_text, [UIPasteboard.generalPasteboard
                             valuesForPasteboardType:UTTypeText.identifier
                                           inItemSet:secondIndex][0]);
}

// Tests that clearing the pasteboard does remove pasteboard items.
TEST_F(PasteboardUtilTest, ClearPasteboardWorks) {
  // Get something stored in the pasteboard.
  NSString* test_text = base::SysUTF8ToNSString(kTestText);
  GURL test_url(kTestURL);
  StoreInPasteboard(test_text, test_url);

  // Clear and assert.
  ClearPasteboard();
  EXPECT_FALSE(UIPasteboard.generalPasteboard.hasURLs);
  EXPECT_FALSE(UIPasteboard.generalPasteboard.hasStrings);
}

}  // namespace
