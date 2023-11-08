// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The size of trailing symbol icons.
constexpr NSInteger kTrailingSymbolImagePointSize = 22;

}  // namespace

@implementation SettingsCheckItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsCheckCell class];
  }
  return self;
}

#pragma mark - TableViewItem

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

#pragma mark - Setters

- (void)setWarningState:(WarningState)state {
  _warningState = state;
  [self setUpWarningTrailingImage];
}

#pragma mark - Private

// Sets up the trailing image and its tint color depending on the item's warning
// state.
- (void)setUpWarningTrailingImage {
  NSString* trailingImageName;
  NSString* trailingImageTintColorName;
  switch (self.warningState) {
    case WarningState::kSafe:
      trailingImageName = kCheckmarkCircleFillSymbol;
      trailingImageTintColorName = kGreen500Color;
      break;
    case WarningState::kWarning:
      trailingImageName = kErrorCircleFillSymbol;
      trailingImageTintColorName = kYellow500Color;
      break;
    case WarningState::kSevereWarning:
      trailingImageName = kErrorCircleFillSymbol;
      trailingImageTintColorName = kRed500Color;
      break;
  }
  self.trailingImage = DefaultSymbolTemplateWithPointSize(
      trailingImageName, kTrailingSymbolImagePointSize);
  self.trailingImageTintColor = [UIColor colorNamed:trailingImageTintColorName];
}

@end
