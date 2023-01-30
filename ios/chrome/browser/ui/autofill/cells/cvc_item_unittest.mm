// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/cvc_item.h"

#import "base/mac/foundation_util.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_item+private.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CVCItemTest = PlatformTest;

// Tests that the cell subviews are set properly after a call to
// `configureCell:` in the different states possible.
TEST_F(CVCItemTest, ConfigureCell) {
  CVCItem* item = [[CVCItem alloc] initWithType:0];
  NSString* instructionsText = @"Instructions Test Text";
  NSString* errorMessage = @"Test Error Message";
  NSString* monthText = @"01";
  NSString* yearText = @"01";
  NSString* CVCText = @"123";

  item.instructionsText = instructionsText;
  item.errorMessage = errorMessage;
  item.monthText = monthText;
  item.yearText = yearText;
  item.CVCText = CVCText;
  item.CVCImageResourceID = IDR_CREDIT_CARD_CVC_HINT_FRONT_AMEX;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[CVCCell class]]);

  CVCCell* CVC = base::mac::ObjCCastStrict<CVCCell>(cell);
  EXPECT_EQ(0U, CVC.instructionsTextLabel.text.length);
  EXPECT_EQ(0U, CVC.errorLabel.text.length);
  EXPECT_EQ(0U, CVC.monthInput.text.length);
  EXPECT_EQ(0U, CVC.yearInput.text.length);
  EXPECT_EQ(0U, CVC.CVCInput.text.length);
  EXPECT_NSEQ(nil, CVC.CVCImageView.image);

  [item configureCell:CVC];
  EXPECT_NSEQ(instructionsText, CVC.instructionsTextLabel.text);
  EXPECT_NSEQ(errorMessage, CVC.errorLabel.text);
  EXPECT_NSEQ(monthText, CVC.monthInput.text);
  EXPECT_NSEQ(yearText, CVC.yearInput.text);
  EXPECT_NSEQ(CVCText, CVC.CVCInput.text);
  EXPECT_NSNE(nil, CVC.CVCImageView.image);
  EXPECT_TRUE(CVC.dateContainerView.hidden);
  EXPECT_TRUE(CVC.buttonForNewCard.hidden);

  item.showDateInput = YES;
  [item configureCell:CVC];
  EXPECT_FALSE(CVC.dateContainerView.hidden);
  EXPECT_TRUE(CVC.buttonForNewCard.hidden);

  item.showNewCardButton = YES;
  [item configureCell:CVC];
  EXPECT_FALSE(CVC.buttonForNewCard.hidden);
}

}  // namespace
