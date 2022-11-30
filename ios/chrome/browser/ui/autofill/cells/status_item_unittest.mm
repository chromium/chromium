// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/status_item.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/mac/foundation_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using StatusItemTest = PlatformTest;

// Tests that the cell subviews are set properly after a call to
// `configureCell:` in the different states.
TEST_F(StatusItemTest, ConfigureCell) {
  StatusItem* item = [[StatusItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  item.text = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[StatusCell class]]);

  StatusCell* statusCell = base::mac::ObjCCastStrict<StatusCell>(cell);
  EXPECT_FALSE(statusCell.textLabel.text);

  [item configureCell:statusCell];
  EXPECT_NSEQ(text, statusCell.textLabel.text);
  EXPECT_TRUE(statusCell.activityIndicator.animating);
  EXPECT_FALSE(statusCell.activityIndicator.hidden);
  EXPECT_TRUE(statusCell.verifiedImageView.hidden);
  EXPECT_TRUE(statusCell.errorImageView.hidden);

  item.state = StatusItemState::VERIFIED;
  [item configureCell:statusCell];
  EXPECT_NSEQ(text, statusCell.textLabel.text);
  EXPECT_TRUE(statusCell.activityIndicator.hidden);
  EXPECT_FALSE(statusCell.verifiedImageView.hidden);
  EXPECT_TRUE(statusCell.errorImageView.hidden);

  item.state = StatusItemState::ERROR;
  [item configureCell:statusCell];
  EXPECT_NSEQ(text, statusCell.textLabel.text);
  EXPECT_TRUE(statusCell.activityIndicator.hidden);
  EXPECT_TRUE(statusCell.verifiedImageView.hidden);
  EXPECT_FALSE(statusCell.errorImageView.hidden);

  item.state = StatusItemState::VERIFYING;
  [item configureCell:statusCell];
  EXPECT_NSEQ(text, statusCell.textLabel.text);
  EXPECT_TRUE(statusCell.activityIndicator.animating);
  EXPECT_FALSE(statusCell.activityIndicator.hidden);
  EXPECT_TRUE(statusCell.verifiedImageView.hidden);
  EXPECT_TRUE(statusCell.errorImageView.hidden);
}

}  // namespace
