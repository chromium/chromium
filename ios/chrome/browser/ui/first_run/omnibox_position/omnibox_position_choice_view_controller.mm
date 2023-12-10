// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mutator.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_option_item_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation OmniboxPositionChoiceViewController {
  /// The view for the top address bar preference option.
  AddressBarOptionView* _topAddressBar;
  /// The view for the bottom address bar preference option.
  AddressBarOptionView* _bottomAddressBar;
  /// Whether the screen is being shown in the FRE.
  BOOL _isFirstRun;
}

#pragma mark - UIViewController

- (instancetype)initWithFirstRun:(BOOL)isFirstRun {
  self = [super init];
  if (self) {
    _topAddressBar = [[AddressBarOptionView alloc]
        initWithSymbolName:kTopOmniboxOptionSymbol
                 labelText:l10n_util::GetNSString(
                               IDS_IOS_TOP_ADDRESS_BAR_OPTION)];
    _bottomAddressBar = [[AddressBarOptionView alloc]
        initWithSymbolName:kBottomOmniboxOptionSymbol
                 labelText:l10n_util::GetNSString(
                               IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION)];
    _isFirstRun = isFirstRun;
  }
  return self;
}

- (void)viewDidLoad {
  CHECK(IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAny));
  // TODO(crbug.com/1503638): Implement this and remove placeholder text.
  self.view.accessibilityIdentifier =
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier;
  self.bannerName = @"default_browser_screen_banner";
  self.titleText = @"**Tailor to Your Needs**";
  self.subtitleText = @"**Decide the position of the search bar to tailor your "
                      @"needs and browsing habits**";
  if (_isFirstRun) {
    self.primaryActionString = @"**Finish**";
    self.secondaryActionString = nil;
  } else {
    self.primaryActionString = @"**Confirm**";
    self.secondaryActionString = @"**No, thanks**";
  }

  [_topAddressBar addTarget:self
                     action:@selector(didTapTopAddressBarView)
           forControlEvents:UIControlEventTouchUpInside];

  [_bottomAddressBar addTarget:self
                        action:@selector(didTapBottomAddressBarView)
              forControlEvents:UIControlEventTouchUpInside];

  NSArray* addressBarOptions = @[ _topAddressBar, _bottomAddressBar ];
  if (DefaultSelectedOmniboxPosition() == ToolbarType::kSecondary) {
    addressBarOptions = @[ _bottomAddressBar, _topAddressBar ];
  }

  UIStackView* addressBarView =
      [[UIStackView alloc] initWithArrangedSubviews:addressBarOptions];
  addressBarView.translatesAutoresizingMaskIntoConstraints = NO;
  addressBarView.distribution = UIStackViewDistributionFillEqually;
  [self.specificContentView addSubview:addressBarView];

  AddSameConstraintsToSides(
      self.specificContentView, addressBarView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

  [NSLayoutConstraint activateConstraints:@[
    [self.specificContentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:addressBarView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - OmniboxPositionChoiceConsumer

- (void)setSelectedToolbarForOmnibox:(ToolbarType)position {
  _topAddressBar.selected = position == ToolbarType::kPrimary;
  _bottomAddressBar.selected = position == ToolbarType::kSecondary;
}

#pragma mark - Private

/// Notifies the mutator to update the selected omnibox position to top.
- (void)didTapTopAddressBarView {
  if (_topAddressBar.selected) {
    return;
  }
  [self.mutator selectTopOmnibox];
}

/// Notifies the mutator to update the selected omnibox position to bottom.
- (void)didTapBottomAddressBarView {
  if (_bottomAddressBar.selected) {
    return;
  }
  [self.mutator selectBottomOmnibox];
}

@end
