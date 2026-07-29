// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/settings_check_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// The size of trailing symbol icons.
constexpr NSInteger kTrailingSymbolImagePointSize = 22;

}  // namespace

@implementation SettingsCheckItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell {
  [super configureCell:cell];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = self.text;
  configuration.titleNumberOfLines = 1;
  configuration.subtitle = self.detailText;
  configuration.textDisabled = !self.enabled;

  if (self.leadingIcon) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = self.leadingIcon;
    symbolConfiguration.symbolTintColor =
        self.enabled ? self.leadingIconTintColor
                     : [UIColor colorNamed:kTextSecondaryColor];
    symbolConfiguration.symbolBackgroundColor = self.leadingIconBackgroundColor;

    configuration.leadingConfiguration = symbolConfiguration;
  }

  if (!self.indicatorHidden) {
    ActivityIndicatorContentConfiguration* activityConfiguration =
        [[ActivityIndicatorContentConfiguration alloc] init];

    configuration.trailingConfiguration = activityConfiguration;
  } else if (self.trailingImage) {
    ImageContentConfiguration* imageConfiguration =
        [[ImageContentConfiguration alloc] init];
    imageConfiguration.image = self.trailingImage;
    imageConfiguration.imageTintColor =
        self.enabled ? self.trailingImageTintColor
                     : [UIColor colorNamed:kTextSecondaryColor];
    imageConfiguration.imageSize =
        CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize);

    configuration.trailingConfiguration = imageConfiguration;
  } else if (!self.infoButtonHidden && self.infoButtonTarget) {
    InfoButtonContentConfiguration* infoButtonConfiguration =
        [[InfoButtonContentConfiguration alloc] init];
    infoButtonConfiguration.target = self.infoButtonTarget;
    infoButtonConfiguration.selector = self.infoButtonSelector;
    infoButtonConfiguration.tag = self.infoButtonTag;
    infoButtonConfiguration.enabled = self.enabled;

    infoButtonConfiguration.selectedForVoiceOver = NO;

    configuration.trailingConfiguration = infoButtonConfiguration;
  }

  cell.contentConfiguration = configuration;

  cell.isAccessibilityElement = YES;

  if (self.enabled) {
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }

  if (self.detailText) {
    cell.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
  } else {
    cell.accessibilityLabel = self.text;
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

#pragma mark - Setters

- (void)setWarningState:(WarningState)state {
  _warningState = state;
  [self setUpWarningTrailingImage];
}

#pragma mark - Private

// Sets up the trailing image and its tint color depending on the item's warning
// state.
- (void)setUpWarningTrailingImage {
  Symbol trailingSymbol = SymbolNone;
  NSString* trailingImageTintColorName;
  switch (self.warningState) {
    case WarningState::kSafe:
      trailingSymbol = SymbolCheckmarkCircleFill;
      trailingImageTintColorName = kGreen500Color;
      break;
    case WarningState::kWarning:
      trailingSymbol = SymbolErrorCircleFill;
      trailingImageTintColorName = kYellow500Color;
      break;
    case WarningState::kSevereWarning:
      trailingSymbol = SymbolErrorCircleFill;
      trailingImageTintColorName = kRed500Color;
      break;
  }
  if (trailingSymbol == SymbolNone) {
    self.trailingImage = nil;
    self.trailingImageTintColor = nil;
  } else {
    self.trailingImage = SymbolTemplateWithPointSize(
        trailingSymbol, kTrailingSymbolImagePointSize);
    self.trailingImageTintColor =
        [UIColor colorNamed:trailingImageTintColorName];
  }
}

@end
