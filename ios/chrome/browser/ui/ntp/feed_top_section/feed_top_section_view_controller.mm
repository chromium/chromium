// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Content stack padding.
const CGFloat kContentStackHorizontalPadding = 18;
const CGFloat kContentStackVerticalPadding = 9;
// Border radius of the promo container.
const CGFloat kPromoViewContainerBorderRadius = 15;

// Returns an array of constraints to make all sides of `innerView` and
// `outerView` match, with `innerView` inset by `insets`.
NSArray<NSLayoutConstraint*>* SameConstraintsWithInsets(
    id<EdgeLayoutGuideProvider> innerView,
    id<EdgeLayoutGuideProvider> outerView,
    NSDirectionalEdgeInsets insets) {
  return @[
    [innerView.leadingAnchor constraintEqualToAnchor:outerView.leadingAnchor
                                            constant:insets.leading],
    [innerView.trailingAnchor constraintEqualToAnchor:outerView.trailingAnchor
                                             constant:-insets.trailing],
    [innerView.topAnchor constraintEqualToAnchor:outerView.topAnchor
                                        constant:insets.top],
    [innerView.bottomAnchor constraintEqualToAnchor:outerView.bottomAnchor
                                           constant:-insets.bottom],
  ];
}
}  // namespace

@interface FeedTopSectionViewController ()

// A vertical StackView which contains all the elements of the top section.
@property(nonatomic, strong) UIStackView* contentStack;

// The signin promo view.
@property(nonatomic, strong) SigninPromoView* promoView;

// View to contain the signin promo.
@property(nonatomic, strong) UIView* promoViewContainer;

// Stores the current UI constraints for the stack view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* contentStackConstraints;

@end

@implementation FeedTopSectionViewController

- (instancetype)init {
  self = [super init];
  if (self) {
    // Create `contentStack` early so we can set up the promo before
    // `viewDidLoad`.
    _contentStack = [[UIStackView alloc] init];
    _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStack.axis = UILayoutConstraintAxisVertical;
    _contentStack.distribution = UIStackViewDistributionFill;
    // TODO(crbug.com/1331010): Update background color for the view.
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentStack];
}

#pragma mark - FeedTopSectionConsumer

// Creates the `PromoViewContainer` and adds the SignInPromo.
- (void)createPromoViewContainer {
  DCHECK(!self.promoViewContainer);
  DCHECK(!self.promoView);
  self.promoViewContainer = [[UIView alloc] init];
  self.promoViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.promoViewContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];

  // TODO(b/287118358): Cleanup IsMagicStackEnabled() code from the sync promo
  // after experiment.
  if (IsMagicStackEnabled()) {
    self.promoViewContainer.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
  }
  self.promoViewContainer.layer.cornerRadius = kPromoViewContainerBorderRadius;

  self.promoView = [self createPromoView];
  // Add the subview to the promoViewContainer.
  [self.promoViewContainer addSubview:self.promoView];
  [self.contentStack addArrangedSubview:self.promoViewContainer];
  AddSameConstraints(self.promoViewContainer, self.promoView);
}

- (void)updateSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)configurator {
  [configurator configureSigninPromoView:self.promoView
                               withStyle:GetTopOfFeedPromoStyle()];
}

#pragma mark - Properties

- (void)setSigninPromoDelegate:(id<SigninPromoViewDelegate>)delegate {
  _signinPromoDelegate = delegate;
  self.promoView.delegate = _signinPromoDelegate;
}

#pragma mark - Private

// Returns insets to add a margin around the stackview if there are items
// to display in the stackview. Otherwise returns NSDirectionalEdgeInsetsZero.
// `visible` indicates whether or not the Feed Top Section is visible.
- (NSDirectionalEdgeInsets)stackViewInsetsForTopSectionVisible:(BOOL)visible {
  if (!visible) {
    return NSDirectionalEdgeInsetsZero;
  }
  return NSDirectionalEdgeInsetsMake(
      kContentStackVerticalPadding, kContentStackHorizontalPadding,
      kContentStackVerticalPadding, kContentStackHorizontalPadding);
}

// Applies constraints to the stack view for a specific visibility. `visible`
// indicates whether or not the Feed Top Section is visible.
- (void)applyStackViewConstraintsForTopSectionVisible:(BOOL)visible {
  if (self.contentStackConstraints) {
    [NSLayoutConstraint deactivateConstraints:self.contentStackConstraints];
  }
  self.contentStack.hidden = !visible;
  self.view.hidden = !visible;
  self.contentStackConstraints = SameConstraintsWithInsets(
      self.contentStack, self.view,
      [self stackViewInsetsForTopSectionVisible:visible]);
  [NSLayoutConstraint activateConstraints:self.contentStackConstraints];
}

- (void)showSigninPromo {
  // Check if the promoViewContainer does not exist. Might not exist if the
  // promo has been "hidden", which involves removing the container.
  if (!self.promoViewContainer && !self.promoView) {
    [self createPromoViewContainer];
  }
  [self applyStackViewConstraintsForTopSectionVisible:YES];
  [self.ntpDelegate updateFeedLayout];
}

- (void)hideSigninPromo {
  [self.contentStack willRemoveSubview:self.promoViewContainer];
  [self.promoViewContainer willRemoveSubview:self.promoView];
  [self.promoView removeFromSuperview];
  [self.promoViewContainer removeFromSuperview];
  self.promoViewContainer = nil;
  self.promoView = nil;
  [self applyStackViewConstraintsForTopSectionVisible:NO];
  [self.ntpDelegate updateFeedLayout];
}

// Configures and creates a signin promo view.
- (SigninPromoView*)createPromoView {
  SigninPromoView* promoView =
      [[SigninPromoView alloc] initWithFrame:CGRectZero];
  promoView.translatesAutoresizingMaskIntoConstraints = NO;
  promoView.delegate = self.signinPromoDelegate;

  SigninPromoViewConfigurator* configurator =
      self.delegate.signinPromoConfigurator;
  SigninPromoViewStyle promoViewStyle = GetTopOfFeedPromoStyle();
  [configurator configureSigninPromoView:promoView withStyle:promoViewStyle];

  promoView.textLabel.text =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_FEED_PROMO)
          : l10n_util::GetNSString(IDS_IOS_NTP_FEED_SIGNIN_COMPACT_PROMO_BODY);
  return promoView;
}

@end
