// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_view_controller.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_option_item_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

/// Leading and trailing padding for the `addressBarView`.
constexpr CGFloat kAddressViewHorizontalPadding = 11;
/// The size of the logo image.
constexpr const CGFloat kLogoSize = 45;
/// The top margin percentage of the header view.
constexpr const CGFloat kHeaderTopMarginPercentage = 0.05;
/// The bottom margin of the header view.
constexpr const CGFloat kHeaderBottomMargin = 31;
/// The inset of the shadow in the header view.
constexpr const CGFloat kHeaderShadowInset = 11;

/// Padding between the subtitle and the `addressBarView`.
constexpr const CGFloat kSubtitleBottomMargin = 17;

}  // namespace

@interface OmniboxPositionChoiceViewController ()

/// Haptic feedback generator for selection change.
@property(nonatomic, readonly, strong)
    UISelectionFeedbackGenerator* feedbackGenerator;

@end

@implementation OmniboxPositionChoiceViewController {
  /// The view for the top address bar preference option.
  AddressBarOptionView* _topAddressBar;
  /// The view for the bottom address bar preference option.
  AddressBarOptionView* _bottomAddressBar;
}

@synthesize feedbackGenerator = _feedbackGenerator;

#pragma mark - UIViewController

- (instancetype)init {
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
  }
  return self;
}

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier;

  self.shouldHideBanner = YES;
  self.usePromoStyleBackground = YES;
  self.hideHeaderOnTallContent = YES;

  self.headerImageType = PromoStyleImageType::kImageWithShadow;
  self.headerViewForceStyleLight = YES;
  self.headerImageShadowInset = kHeaderShadowInset;
  self.noBackgroundHeaderImageTopMarginPercentage = kHeaderTopMarginPercentage;
  self.headerImageBottomMargin = kHeaderBottomMargin;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoSize));
#else
  UIImage* logo = CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.headerImage = logo;

  self.titleHorizontalMargin = 0;
  self.subtitleBottomMargin = kSubtitleBottomMargin;
  self.titleText = l10n_util::GetNSString(IDS_IOS_OMNIBOX_POSITION_PROMO_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_POSITION_PROMO_VALIDATE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_POSITION_PROMO_IPH_SUBTITLE);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_POSITION_PROMO_DISCARD);

  [_topAddressBar addTarget:self
                     action:@selector(didTapTopAddressBarView)
           forControlEvents:UIControlEventTouchUpInside];
  [_topAddressBar addTarget:self
                     action:@selector(didTouchDownAddressBarOption)
           forControlEvents:UIControlEventTouchDown];

  [_bottomAddressBar addTarget:self
                        action:@selector(didTapBottomAddressBarView)
              forControlEvents:UIControlEventTouchUpInside];
  [_bottomAddressBar addTarget:self
                        action:@selector(didTouchDownAddressBarOption)
              forControlEvents:UIControlEventTouchDown];

  NSArray* addressBarOptions = @[ _topAddressBar, _bottomAddressBar ];

  UIStackView* addressBarView =
      [[UIStackView alloc] initWithArrangedSubviews:addressBarOptions];
  addressBarView.translatesAutoresizingMaskIntoConstraints = NO;
  addressBarView.distribution = UIStackViewDistributionFillEqually;
  [self.specificContentView addSubview:addressBarView];

  AddSameConstraintsToSidesWithInsets(
      addressBarView, self.specificContentView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kAddressViewHorizontalPadding, 0,
                                  kAddressViewHorizontalPadding));

  [NSLayoutConstraint activateConstraints:@[
    [self.specificContentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:addressBarView.bottomAnchor],
  ]];

  [super viewDidLoad];
}

- (UISelectionFeedbackGenerator*)feedbackGenerator {
  if (!_feedbackGenerator) {
    _feedbackGenerator = [[UISelectionFeedbackGenerator alloc] init];
  }
  return _feedbackGenerator;
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
  [self.feedbackGenerator selectionChanged];
  [self.mutator selectTopOmnibox];
}

/// Notifies the mutator to update the selected omnibox position to bottom.
- (void)didTapBottomAddressBarView {
  if (_bottomAddressBar.selected) {
    return;
  }
  [self.feedbackGenerator selectionChanged];
  [self.mutator selectBottomOmnibox];
}

- (void)didTouchDownAddressBarOption {
  [self.feedbackGenerator prepare];
}

@end
