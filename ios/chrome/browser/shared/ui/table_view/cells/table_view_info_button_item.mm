// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TableViewInfoButtonItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _accessibilityActivationPointOnButton = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* contentConfiguration =
      [[TableViewCellContentConfiguration alloc] init];
  contentConfiguration.title = self.text;
  contentConfiguration.titleColor = self.textColor;
  contentConfiguration.subtitle = self.detailText;
  contentConfiguration.subtitleColor = self.detailTextColor;
  contentConfiguration.trailingText = self.statusText;

  if (self.accessibilityDelegate) {
    cell.accessibilityCustomActions = [self createAccessibilityActions];
  }
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  if (self.iconImage) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = self.iconImage;
    symbolConfiguration.symbolBackgroundColor = self.iconBackgroundColor;
    symbolConfiguration.symbolTintColor = self.iconTintColor;

    contentConfiguration.leadingConfiguration = symbolConfiguration;
  }

  InfoButtonContentConfiguration* buttonConfiguration =
      [[InfoButtonContentConfiguration alloc] init];
  buttonConfiguration.target = self.target;
  buttonConfiguration.selector = self.selector;
  buttonConfiguration.tag = self.tag;
  buttonConfiguration.selectedForVoiceOver =
      self.accessibilityActivationPointOnButton;

  contentConfiguration.trailingConfiguration = buttonConfiguration;

  cell.contentConfiguration = contentConfiguration;
  cell.accessibilityLabel = contentConfiguration.accessibilityLabel;
  cell.accessibilityValue = contentConfiguration.accessibilityValue;
  cell.accessibilityHint =
      self.accessibilityHint ?: contentConfiguration.accessibilityHint;
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
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
