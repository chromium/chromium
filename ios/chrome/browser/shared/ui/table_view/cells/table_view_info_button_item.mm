// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TableViewInfoButtonItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewInfoButtonCell class];
    _accessibilityActivationPointOnButton = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewInfoButtonCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  }
  if (self.detailText) {
    cell.detailTextLabel.text = self.detailText;
    if (self.detailTextColor) {
      cell.detailTextLabel.textColor = self.detailTextColor;
    }
    [cell updatePaddingForDetailText:YES];
  } else {
    [cell updatePaddingForDetailText:NO];
  }
  [cell setStatusText:self.statusText];
  if (self.accessibilityHint) {
    cell.customizedAccessibilityHint = self.accessibilityHint;
  }
  if (self.accessibilityDelegate && !self.infoButtonIsHidden) {
    cell.accessibilityCustomActions = [self createAccessibilityActions];
  }
  cell.isButtonSelectedForVoiceOver = self.accessibilityActivationPointOnButton;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  [cell setIconImage:self.iconImage
            tintColor:self.iconTintColor
      backgroundColor:self.iconBackgroundColor
         cornerRadius:self.iconCornerRadius];

  // Updates if the cells UI button should be hidden.
  [cell hideUIButton:self.infoButtonIsHidden];
}

#pragma mark - Accessibility

// Creates custom accessibility actions.
- (NSArray*)createAccessibilityActions {
  NSMutableArray* customActions = [[NSMutableArray alloc] init];

  // Custom action for when the activation point is on the center of row.
  if (!self.accessibilityActivationPointOnButton) {
    UIAccessibilityCustomAction* tapButtonAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT)
                  target:self
                selector:@selector(handleTappedInfoButtonForItem)];
    [customActions addObject:tapButtonAction];
  }

  return customActions;
}

// Handles accessibility action for tapping outside the info button.
- (void)handleTappedInfoButtonForItem {
  [self.accessibilityDelegate handleTappedInfoButtonForItem:self];
}

@end
