// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using PassphraseErrorItemTest = PlatformTest;

// Tests that the text label is set properly after a call to `configureCell:`.
TEST_F(PassphraseErrorItemTest, ConfigureCell) {
  PassphraseErrorItem* item = [[PassphraseErrorItem alloc] initWithType:0];
  PassphraseErrorCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[PassphraseErrorCell class]]);
  EXPECT_NSEQ(nil, cell.textLabel.text);
  NSString* text = @"This is an error";

  item.text = text;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, cell.textLabel.text);
}

}  // namespace
