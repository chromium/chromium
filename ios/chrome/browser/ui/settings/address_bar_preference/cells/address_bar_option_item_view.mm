// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_option_item_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Returns the palette to be used on bottom/top address bar selected state.
NSArray<UIColor*>* AddressBarSelectedOptionPalette() {
  return @[
    [UIColor colorNamed:kBlue600Color], [UIColor colorNamed:kBlue300Color]
  ];
}

// Returns the palette to be used on bottom/top address bar unselected state.
NSArray<UIColor*>* AddressBarUnselectedOptionPalette() {
  return @[
    [UIColor colorNamed:kGrey700Color], [UIColor colorNamed:kGrey400Color]
  ];
}

// The content view vertical spacing.
const CGFloat kContentViewSpacing = 16.0;
// The address bar option view top/bottom padding.
const CGFloat kAddressBarViewTopBottomPadding = 16.0;
// The size of top/bottom setting Address bar option symbol.
const CGFloat kAddressBarSymbolPointSize = 100.0;

}  // namespace

@implementation AddressBarOptionView {
  // The image view for the checkbox.
  UIImageView* _checkbox;
  // If set to YES, this option is for top address bar, otherwise it is for
  // bottom address bar.
  BOOL _topAddressBarOption;
  // The phone UIimage.
  UIImage* _phoneImage;
  // The phone image view.
  UIImageView* _phoneImageView;
}

- (instancetype)initWithSymbolName:(NSString*)symbolName
                         labelText:(NSString*)labelText {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    UILabel* label = [[UILabel alloc] init];
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    label.adjustsFontForContentSizeCategory = YES;
    [label setText:labelText];

    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kAddressBarSymbolPointSize
                            weight:UIImageSymbolWeightUltraLight
                             scale:UIImageSymbolScaleLarge];

    _phoneImage = CustomSymbolWithConfiguration(symbolName, configuration);

    _phoneImageView = [[UIImageView alloc]
        initWithImage:[self phoneImageByApplyingSelectedState]];

    _checkbox = [[UIImageView alloc]
        initWithImage:self.selected ? DefaultSettingsRootSymbol(
                                          kCheckmarkCircleFillSymbol)
                                    : DefaultSettingsRootSymbol(kCircleSymbol)];
    [self updateCheckBoxTintColor];

    UIStackView* contentView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ label, _phoneImageView, _checkbox ]];
    [contentView setAxis:UILayoutConstraintAxisVertical];
    contentView.distribution = UIStackViewDistributionEqualCentering;
    contentView.alignment = UIStackViewAlignmentCenter;
    contentView.translatesAutoresizingMaskIntoConstraints = false;
    [contentView setSpacing:kContentViewSpacing];
    [contentView setUserInteractionEnabled:NO];

    [label setContentHuggingPriority:UILayoutPriorityRequired
                             forAxis:UILayoutConstraintAxisVertical];

    [contentView setLayoutMarginsRelativeArrangement:YES];
    contentView.layoutMargins = UIEdgeInsetsMake(
        kAddressBarViewTopBottomPadding, 0, kAddressBarViewTopBottomPadding, 0);
    self.translatesAutoresizingMaskIntoConstraints = false;

    self.accessibilityLabel = labelText;

    [self addSubview:contentView];

    AddSameConstraints(contentView, contentView.superview);
  }

  return self;
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  [_phoneImageView setImage:[self phoneImageByApplyingSelectedState]];
  [_checkbox
      setImage:selected ? DefaultSettingsRootSymbol(kCheckmarkCircleFillSymbol)
                        : DefaultSettingsRootSymbol(kCircleSymbol)];
  [self updateCheckBoxTintColor];
}

#pragma mark - Private

// Returns a UIImage based on the selection state.
- (UIImage*)phoneImageByApplyingSelectedState {
  NSArray* colors = self.selected ? AddressBarSelectedOptionPalette()
                                  : AddressBarUnselectedOptionPalette();
  return [_phoneImage
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationWithPaletteColors:colors]];
}

// Updates the tintColor of the checkbox based on the selection state.
- (void)updateCheckBoxTintColor {
  if (self.selected) {
    _checkbox.tintColor = [UIColor colorNamed:kBlue600Color];
  } else {
    _checkbox.tintColor = [UIColor colorNamed:kGrey700Color];
  }
}

@end
