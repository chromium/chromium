// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using AccountControlTableViewItemTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly after a call to
// `configureCell:`.
TEST_F(AccountControlTableViewItemTest, ImageViewAndTextLabels) {
  TableViewAccountItem* item = [[TableViewAccountItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* main_text = @"Main text";
  NSString* detail_text = @"Detail text";

  item.image = image;
  item.text = main_text;
  item.detailText = detail_text;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(main_text, configuration.title);
  EXPECT_NSEQ(detail_text, configuration.subtitle);

  NSObject<ChromeContentConfiguration>* leading_config =
      configuration.leadingConfiguration;
  ASSERT_TRUE(
      [leading_config isMemberOfClass:[ImageContentConfiguration class]]);

  ImageContentConfiguration* image_config =
      base::apple::ObjCCastStrict<ImageContentConfiguration>(leading_config);
  EXPECT_NSEQ(image, image_config.image);
}
