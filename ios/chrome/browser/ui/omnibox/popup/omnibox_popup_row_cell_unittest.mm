// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Implements AutocompleteSuggestion protocol for use in tests. Can be populated
// directly into properties.
@interface FakeAutocompleteMatch : NSObject <AutocompleteSuggestion>
@property(nonatomic, assign) BOOL supportsDeletion;
@property(nonatomic, assign) BOOL hasAnswer;
@property(nonatomic, assign) BOOL isURL;
@property(nonatomic, assign, getter=isAppendable) BOOL appendable;
@property(nonatomic, strong) UIImage* suggestionTypeIcon;
@property(nonatomic, assign) BOOL isTabMatch;
@property(nonatomic, strong) NSAttributedString* text;
@property(nonatomic, strong) NSAttributedString* detailText;
@property(nonatomic, assign) NSInteger numberOfLines;
@property(nonatomic, assign) BOOL hasImage;
@property(nonatomic, assign) GURL imageURL;
@property(nonatomic, assign) GURL faviconPageURL;
@property(nonatomic, strong) id<OmniboxIcon> icon;

@end

@implementation FakeAutocompleteMatch
@end

namespace {

class OmniboxPopupRowCellTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    cell_ = [[OmniboxPopupRowCell alloc] init];
  }

  OmniboxPopupRowCell* cell_;
};

TEST_F(OmniboxPopupRowCellTest, ReadsAnswersInVoiceover) {
  FakeAutocompleteMatch* fakeAnswerMatch = [[FakeAutocompleteMatch alloc] init];
  fakeAnswerMatch.hasAnswer = YES;

  // The detail for answers is the suggested question, for example if user types
  // "how tall is" the detail text might be "how tall is the Eiffel tower".
  fakeAnswerMatch.detailText =
      [[NSAttributedString alloc] initWithString:@"question"];
  fakeAnswerMatch.text = [[NSAttributedString alloc] initWithString:@"answer"];

  [cell_ setupWithAutocompleteSuggestion:fakeAnswerMatch incognito:NO];

  EXPECT_NSEQ([cell_ accessibilityValue], @"question");
  EXPECT_NSEQ([cell_ accessibilityLabel], @"answer");
}

TEST_F(OmniboxPopupRowCellTest, ReadsNonAnswersInVoiceover) {
  FakeAutocompleteMatch* fakeNonAnswerMatch =
      [[FakeAutocompleteMatch alloc] init];
  fakeNonAnswerMatch.hasAnswer = NO;

  fakeNonAnswerMatch.detailText =
      [[NSAttributedString alloc] initWithString:@"detail"];
  fakeNonAnswerMatch.text = [[NSAttributedString alloc] initWithString:@"body"];

  [cell_ setupWithAutocompleteSuggestion:fakeNonAnswerMatch incognito:NO];

  EXPECT_NSEQ([cell_ accessibilityValue], @"detail");
  EXPECT_NSEQ([cell_ accessibilityLabel], @"body");
}

}  // namespace
