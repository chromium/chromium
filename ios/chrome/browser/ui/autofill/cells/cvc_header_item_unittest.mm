// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "cvc_header_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using CVCHeaderItemTest = PlatformTest;

// Tests that the header subviews are set properly after a call to
// `ConfigureHeaderFooterView:`.
TEST_F(CVCHeaderItemTest, ConfigureHeaderFooterView) {
  CVCHeaderItem* header_item = [[CVCHeaderItem alloc] initWithType:0];
  NSString* instructions_text = @"Instructions Test Text";

  header_item.instructionsText = instructions_text;

  id view = [[[header_item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[CVCHeaderView class]]);

  CVCHeaderView* header_view = base::mac::ObjCCastStrict<CVCHeaderView>(view);
  EXPECT_EQ(0U, header_view.instructionsLabel.text.length);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [header_item configureHeaderFooterView:header_view withStyler:styler];
  EXPECT_NSEQ(instructions_text, header_view.instructionsLabel.text);
}

}  // namespace
