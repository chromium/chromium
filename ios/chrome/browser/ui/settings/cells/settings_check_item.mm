// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
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
  [cell setInfoButtonHidden:self.infoButtonHidden];
  [cell setInfoButtonEnabled:self.enabled];
  self.indicatorHidden ? [cell hideActivityIndicator]
                       : [cell showActivityIndicator];
  if (self.enabled) {
    [cell setLeadingIconImage:self.leadingIcon
                    tintColor:self.leadingIconTintColor
              backgroundColor:self.leadingIconBackgroundColor
                 cornerRadius:self.leadingIconCornerRadius];
    [cell setTrailingImage:self.trailingImage
             withTintColor:self.trailingImageTintColor];
    cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    [cell setLeadingIconImage:self.leadingIcon
                    tintColor:[UIColor colorNamed:kTextSecondaryColor]
              backgroundColor:self.leadingIconBackgroundColor
                 cornerRadius:self.leadingIconCornerRadius];
    [cell setTrailingImage:self.trailingImage
             withTintColor:[UIColor colorNamed:kTextSecondaryColor]];
    cell.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
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
