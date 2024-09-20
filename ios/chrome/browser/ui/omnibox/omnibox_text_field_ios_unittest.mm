// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/uikit_ui_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class OmniboxTextFieldIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // This rect is fairly arbitrary. The text field just needs a non-zero width
    // so that the pre-edit label's text alignment can be tested.
    CGRect rect = CGRectMake(0, 0, 100, 20);
    textfield_ = [[OmniboxTextFieldIOS alloc] initWithFrame:rect];
    [GetAnyKeyWindow() addSubview:textfield_];
  }

  void TearDown() override { [textfield_ removeFromSuperview]; }

  void ExpectRectEqual(CGRect expectedRect, CGRect actualRect) {
    EXPECT_EQ(expectedRect.origin.x, actualRect.origin.x);
    EXPECT_EQ(expectedRect.origin.y, actualRect.origin.y);
    EXPECT_EQ(expectedRect.size.width, actualRect.size.width);
    EXPECT_EQ(expectedRect.size.height, actualRect.size.height);
  }

  // Verifies that the `selectedNSRange` function properly converts from opaque
  // UITextRanges to NSRanges.  This function selects blocks of text in the text
  // field and compares the field's actual selected text to the converted
  // NSRange.
  void VerifySelectedNSRanges(NSString* text) {
    // The NSRange conversion mechanism only works when the field is first
    // responder.
    [textfield_ setText:text];
    [textfield_ becomeFirstResponder];
    ASSERT_TRUE([textfield_ isFirstResponder]);

    // `i` and `j` hold the start and end offsets of the range that is currently
    // being tested.  This function iterates through all possible combinations
    // of `i` and `j`.
    NSInteger i = 0;
    NSInteger j = i + 1;
    UITextPosition* beginning = [textfield_ beginningOfDocument];
    UITextPosition* start =
        [textfield_ positionFromPosition:[textfield_ beginningOfDocument]
                                  offset:i];

    // In order to avoid making any assumptions about the length of the text in
    // the field, this test operates by incrementing the `i` and `j` offsets and
    // converting them to opaque UITextPositions.  If either `i` or `j` are
    // invalid offsets for the current field text,
    // `positionFromPosition:offset:` is documented to return nil.  This is used
    // as a signal to stop incrementing that offset and reset (or end the test).
    while (start) {
      UITextPosition* end = [textfield_ positionFromPosition:beginning
                                                      offset:j];
      while (end) {
        [textfield_
            setSelectedTextRange:[textfield_ textRangeFromPosition:start
                                                        toPosition:end]];

        // There are two ways to get the selected text:
        // 1) Ask the field for it directly.
        // 2) Compute the selected NSRange and use that to extract a substring
        //    from the field's text.
        // This block of code ensures that the two methods give identical text.
        NSRange nsrange = [textfield_ selectedNSRange];
        NSString* nstext = [[textfield_ text] substringWithRange:nsrange];
        UITextRange* uirange = [textfield_ selectedTextRange];
        NSString* uitext = [textfield_ textInRange:uirange];
        EXPECT_NSEQ(nstext, uitext);

        // Increment `j` and `end` for the next iteration of the inner while
        // loop.
        ++j;
        end = [textfield_ positionFromPosition:beginning offset:j];
      }

      // Increment `i` and `start` for the next iteration of the outer while
      // loop.  This also requires `j` to be reset.
      ++i;
      j = i + 1;
      start = [textfield_ positionFromPosition:beginning offset:i];
    }

    [textfield_ resignFirstResponder];
  }

  OmniboxTextFieldIOS* textfield_;
};

// TODO:(crbug.com/1156541): Re-enable this test on devices.
#if TARGET_OS_SIMULATOR
#define MAYBE_SelectedRanges SelectedRanges
#else
#define MAYBE_SelectedRanges FLAKY_SelectedRanges
#endif
TEST_F(OmniboxTextFieldIOSTest, MAYBE_SelectedRanges) {
  if (@available(iOS 17, *)) {
    // TODO:(crbug.com/1468176): Failing on iOS17 beta 5.
    return;
  }
  base::FilePath test_data_directory;
  ASSERT_TRUE(base::PathService::Get(ios::DIR_TEST_DATA, &test_data_directory));
  base::FilePath test_file = test_data_directory.Append(
      FILE_PATH_LITERAL("omnibox/selected_ranges.txt"));
  ASSERT_TRUE(base::PathExists(test_file));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file, &contents));
  std::vector<std::string> test_strings = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i < test_strings.size(); ++i) {
    if (test_strings[i].size() > 0) {
      VerifySelectedNSRanges(base::SysUTF8ToNSString(test_strings[i]));
    }
  }
}

TEST_F(OmniboxTextFieldIOSTest, SelectExitsPreEditState) {
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ isPreEditing]);
  [textfield_ select:nil];
  EXPECT_FALSE([textfield_ isPreEditing]);
}

TEST_F(OmniboxTextFieldIOSTest, SelectAllExitsPreEditState) {
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ isPreEditing]);
  [textfield_ selectAll:nil];
  EXPECT_FALSE([textfield_ isPreEditing]);
}

TEST_F(OmniboxTextFieldIOSTest, CopyInPreedit) {
  id delegateMock = OCMProtocolMock(@protocol(OmniboxTextFieldDelegate));
  NSString* testString = @"omnibox test string";
  [textfield_ setText:testString];
  textfield_.delegate = delegateMock;
  [textfield_ becomeFirstResponder];
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ canPerformAction:@selector(copy:) withSender:nil]);
  [delegateMock onCopy];
  [textfield_ copy:nil];
  EXPECT_NSEQ(textfield_.text, testString);
  [delegateMock verify];
}

TEST_F(OmniboxTextFieldIOSTest, CutInPreedit) {
  id delegateMock = OCMProtocolMock(@protocol(OmniboxTextFieldDelegate));
  NSString* testString = @"omnibox test string";
  [textfield_ setText:testString];
  textfield_.delegate = delegateMock;
  [textfield_ becomeFirstResponder];
  [textfield_ enterPreEditState];
  EXPECT_TRUE([textfield_ canPerformAction:@selector(cut:) withSender:nil]);
  [delegateMock onCopy];
  [textfield_ cut:nil];
  EXPECT_NSEQ(textfield_.text, @"");
  [delegateMock verify];
}

}  // namespace
