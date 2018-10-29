// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PaymentsSelectorEditItem

@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;
@synthesize name = _name;
@synthesize value = _value;
@synthesize autofillUIType = _autofillUIType;
@synthesize required = _required;
@synthesize nameFont = _nameFont;
@synthesize nameColor = _nameColor;
@synthesize valueFont = _valueFont;
@synthesize valueColor = _valueColor;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewDetailCell class];
  }
  return self;
}

- (UIFont*)nameFont {
  if (!_nameFont) {
    if (@available(iOS 11, *)) {
      _nameFont = [[UIFontMetrics defaultMetrics]
          scaledFontForFont:[MDCTypography body2Font]];
    } else {
      _nameFont = [MDCTypography body2Font];
    }
  }
  return _nameFont;
}

- (UIColor*)nameColor {
  if (!_nameColor) {
    _nameColor = [[MDCPalette greyPalette] tint900];
  }
  return _nameColor;
}

- (UIFont*)valueFont {
  if (!_valueFont) {
    if (@available(iOS 11, *)) {
      _valueFont = [[UIFontMetrics defaultMetrics]
          scaledFontForFont:[MDCTypography body1Font]];
    } else {
      _valueFont = [MDCTypography body1Font];
    }
  }
  return _valueFont;
}

- (UIColor*)valueColor {
  if (!_valueColor) {
    _valueColor = [[MDCPalette greyPalette] tint500];
  }
  return _valueColor;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewDetailCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  NSString* textLabelFormat = self.required ? @"%@*" : @"%@";
  cell.textLabel.text = [NSString stringWithFormat:textLabelFormat, self.name];
  cell.detailTextLabel.text = self.value;

  // Styling.
  cell.textLabel.font = self.nameFont;
  cell.textLabel.textColor = self.nameColor;
  cell.detailTextLabel.font = self.valueFont;
  cell.detailTextLabel.textColor = self.valueColor;
  if (@available(iOS 11, *)) {
    cell.textLabel.adjustsFontForContentSizeCategory = YES;
    cell.detailTextLabel.adjustsFontForContentSizeCategory = YES;
  }

  [cell updateConstraintsIfNeeded];
}

@end
