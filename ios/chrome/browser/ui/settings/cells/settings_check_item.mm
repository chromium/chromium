// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsCheckItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsCheckCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(SettingsCheckCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  if (self.enabled) {
    [cell setInfoButtonHidden:self.infoButtonHidden];
    [cell setLeadingImage:self.leadingImage
            withTintColor:self.leadingImageTintColor];
    [cell setTrailingImage:self.trailingImage
             withTintColor:self.trailingImageTintColor];
    self.indicatorHidden ? [cell hideActivityIndicator]
                         : [cell showActivityIndicator];
    cell.textLabel.textColor = UIColor.cr_labelColor;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    [cell setLeadingImage:self.leadingImage
            withTintColor:UIColor.cr_secondaryLabelColor];
    [cell setTrailingImage:nil withTintColor:nil];
    [cell hideActivityIndicator];
    [cell setInfoButtonHidden:YES];
    cell.textLabel.textColor = UIColor.cr_secondaryLabelColor;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  cell.isAccessibilityElement = YES;

  if (self.detailText) {
    cell.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
  } else {
    cell.accessibilityLabel = self.text;
  }
}

@end
