// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/target_account_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TargetAccountItemTest = PlatformTest;
}

// Tests that the email UILabel and avatar is set properly after a call to
// `configureCell:`.
TEST_F(TargetAccountItemTest, ItemProperties) {
  NSString* email = @"test@gmail.com";

  TargetAccountItem* item = [[TargetAccountItem alloc] initWithType:0];
  item.email = email;
  item.avatar = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TargetAccountCell class]]);

  TargetAccountCell* targetAccountCell =
      base::mac::ObjCCastStrict<TargetAccountCell>(cell);
  EXPECT_FALSE(targetAccountCell.emailLabel.text);
  EXPECT_FALSE(targetAccountCell.avatarBadge.image);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(email, targetAccountCell.emailLabel.text);
  EXPECT_FALSE(targetAccountCell.avatarBadge.isHidden);
}
