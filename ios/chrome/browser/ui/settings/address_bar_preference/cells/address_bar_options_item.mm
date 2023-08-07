// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_options_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_option_item_view.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_preference_service_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The cell leading padding.
const CGFloat kCellLeadingPadding = 32;
// The cell trailing padding.
const CGFloat kCellTrailingPadding = 31;

}  // namespace

@implementation AddressBarOptionsItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AddressBarOptionsCell class];
  }
  return self;
}

- (void)configureCell:(AddressBarOptionsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.addressBarpreferenceServiceDelegate =
      _addressBarpreferenceServiceDelegate;
  cell.bottomAddressBarOptionSelected = _bottomAddressBarOptionSelected;
}

@end

@implementation AddressBarOptionsCell {
  // The view for the top address bar preference option.
  AddressBarOptionView* _topAddressBar;
  // The view for the bottom address bar preference option.
  AddressBarOptionView* _bottomAddressBar;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  UIStackView* addressBarSettingInterfaceContentView =
      [self addressBarPreferenceOptionsContent];
  [self.contentView addSubview:addressBarSettingInterfaceContentView];

  [NSLayoutConstraint activateConstraints:@[
    [addressBarSettingInterfaceContentView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kCellLeadingPadding],
    [addressBarSettingInterfaceContentView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kCellTrailingPadding],
    [addressBarSettingInterfaceContentView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor],
    [addressBarSettingInterfaceContentView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor]
  ]];

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _addressBarpreferenceServiceDelegate = nil;
}

- (void)setBottomAddressBarOptionSelected:(BOOL)value {
  _bottomAddressBarOptionSelected = value;
  [_topAddressBar setSelected:!_bottomAddressBarOptionSelected];
  [_bottomAddressBar setSelected:_bottomAddressBarOptionSelected];
}

#pragma mark - Private

// Returns a UI stack view that displays an UI stack view with two Address bar
// preference options views.
- (UIStackView*)addressBarPreferenceOptionsContent {
  _topAddressBar =
      [[AddressBarOptionView alloc] initWithSymbolName:kTopOmniboxOptionSymbol
                                             labelText:@"Top"];
  _bottomAddressBar = [[AddressBarOptionView alloc]
      initWithSymbolName:kBottomOmniboxOptionSymbol
               labelText:@"Bottom"];

  [_topAddressBar setSelected:!_bottomAddressBarOptionSelected];
  [_bottomAddressBar setSelected:_bottomAddressBarOptionSelected];

  UIStackView* addressBarView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _topAddressBar, _bottomAddressBar ]];

  [_topAddressBar addTarget:self
                     action:@selector(onSelectTopAddressBar)
           forControlEvents:UIControlEventTouchUpInside];

  [_bottomAddressBar addTarget:self
                        action:@selector(onSelectBottomAddressBar)
              forControlEvents:UIControlEventTouchUpInside];

  addressBarView.translatesAutoresizingMaskIntoConstraints = NO;

  [addressBarView setDistribution:UIStackViewDistributionFillEqually];

  return addressBarView;
}

// Notifies the address bar preference service to update the state to top
// address bar.
- (void)onSelectTopAddressBar {
  [_addressBarpreferenceServiceDelegate didSelectTopAddressBarPreference];
}

// Notifies the address bar preference service to update the state to bottom
// address bar.
- (void)onSelectBottomAddressBar {
  [_addressBarpreferenceServiceDelegate didSelectBottomAddressBarPreference];
}

@end
