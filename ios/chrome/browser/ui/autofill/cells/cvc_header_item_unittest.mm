// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "cvc_header_item.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class CVCHeaderItemTest : public PlatformTest {
 public:
  CVCHeaderItemTest() {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableVirtualCards);
  }

  CVCHeaderItemTest(const CVCHeaderItemTest&) = delete;
  CVCHeaderItemTest& operator=(const CVCHeaderItemTest&) = delete;
  ~CVCHeaderItemTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the header subviews are set properly after a call to
// `ConfigureHeaderFooterView:`.
TEST_F(CVCHeaderItemTest, ConfigureHeaderFooterView) {
  NSString* title_text = @"Title Test Text";
  NSString* instructions_text = @"Instructions Test Text";

  CVCHeaderItem* header_item =
      [[CVCHeaderItem alloc] initWithType:0
                                titleText:title_text
                         instructionsText:instructions_text];

  id view = [[[header_item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[CVCHeaderView class]]);

  CVCHeaderView* header_view = base::apple::ObjCCastStrict<CVCHeaderView>(view);
  EXPECT_EQ(0U, header_view.titleLabel.text.length);
  EXPECT_EQ(0U, header_view.instructionsLabel.text.length);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [header_item configureHeaderFooterView:header_view withStyler:styler];
  EXPECT_NSEQ(title_text, header_view.titleLabel.text);
  EXPECT_NSEQ(instructions_text, header_view.instructionsLabel.text);
}

}  // namespace
