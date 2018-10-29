// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/encryption_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
}  // namespace

@interface EncryptionCell ()

// Returns the default text color used for the given |enabled| state.
+ (UIColor*)defaultTextColorForEnabledState:(BOOL)enabled;

@end

@implementation EncryptionItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [EncryptionCell class];
    self.enabled = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

- (void)configureCell:(EncryptionCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  cell.textLabel.text = self.text;
  cell.textLabel.textColor =
      [EncryptionCell defaultTextColorForEnabledState:self.enabled];
}

@end

@implementation EncryptionCell

@synthesize textLabel = _textLabel;

+ (UIColor*)defaultTextColorForEnabledState:(BOOL)enabled {
  MDCPalette* grey = [MDCPalette greyPalette];
  return enabled ? grey.tint900 : grey.tint500;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    [self.contentView addSubview:_textLabel];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
    ]];
    AddOptionalVerticalPadding(self.contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

- (void)layoutSubviews {
  // When the accessory type is None, the content view of the cell (and thus)
  // the labels inside it span larger than when there is a Checkmark accessory
  // type. That means that toggling the accessory type can induce a rewrapping
  // of the detail text, which is not visually pleasing. To alleviate that
  // issue, always lay out the cell as if there was a Checkmark accessory type.
  //
  // Force the accessory type to Checkmark for the duration of layout.
  MDCCollectionViewCellAccessoryType realAccessoryType = self.accessoryType;
  self.accessoryType = MDCCollectionViewCellAccessoryCheckmark;

  // Implement -layoutSubviews as per instructions in documentation for
  // +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  _textLabel.preferredMaxLayoutWidth =
      CGRectGetWidth(self.contentView.frame) - 2 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];

  // Restore the real accessory type at the end of the layout.
  self.accessoryType = realAccessoryType;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
  self.textLabel.text = nil;
  [EncryptionCell defaultTextColorForEnabledState:YES];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

@end
