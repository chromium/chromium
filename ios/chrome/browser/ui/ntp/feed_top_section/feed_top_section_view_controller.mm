// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Content stack padding.
const CGFloat kContentStackHorizontalPadding = 18;
const CGFloat kContentStackVerticalPadding = 9;
// Border radius of the promo container.
const CGFloat kPromoViewContainerBorderRadius = 15;
// The image name of the promo view's icon.
NSString* const kPromoViewImageName = @"ntp_feed_signin_promo_icon";

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
  [self applyStackViewConstraints];
}

#pragma mark - FeedTopSectionConsumer

- (void)setShouldShowSigninPromo:(BOOL)shouldShowSigninPromo {
  if (_shouldShowSigninPromo == shouldShowSigninPromo) {
    return;
  }
  // TODO(crbug.com/1331010): Handle targeting of the promo.
  _shouldShowSigninPromo = shouldShowSigninPromo;
  [self updateFeedSigninPromoVisibility];
}

- (void)updateFeedSigninPromoVisibility {
  if (self.shouldShowSigninPromo) {
    DCHECK(!self.promoViewContainer);
    DCHECK(!self.promoView);
    self.promoViewContainer = [[UIView alloc] init];
    self.promoViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
    self.promoViewContainer.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    self.promoViewContainer.layer.cornerRadius =
        kPromoViewContainerBorderRadius;

    self.promoView = [self createPromoView];
    [self.promoViewContainer addSubview:self.promoView];

    [self.contentStack addArrangedSubview:self.promoViewContainer];
    AddSameConstraints(self.promoViewContainer, self.promoView);
  } else {
    DCHECK(self.promoViewContainer);
    DCHECK(self.promoView);
    [self.contentStack willRemoveSubview:self.promoViewContainer];
    [self.promoViewContainer willRemoveSubview:self.promoView];
    [self.promoView removeFromSuperview];
    [self.promoViewContainer removeFromSuperview];
    self.promoViewContainer = nil;
    self.promoView = nil;
  }
  [self applyStackViewConstraints];
  [self.ntpDelegate updateFeedLayout];
}

- (void)updateSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)configurator {
  // TODO(crbug.com/1331010): Use feature params to specify which style of promo
  // to use.
  [configurator configureSigninPromoView:self.promoView
                               withStyle:SigninPromoViewStyleTitledCompact];
}

#pragma mark - Properties

- (void)setSigninPromoDelegate:(id<SigninPromoViewDelegate>)delegate {
  _signinPromoDelegate = delegate;
  self.promoView.delegate = _signinPromoDelegate;
}

#pragma mark - Private

// Returns insets to add a margin around the stackview if there are items
// to display in the stackview. Otherwise returns NSDirectionalEdgeInsetsZero.
- (NSDirectionalEdgeInsets)stackViewInsets {
  if (self.shouldShowSigninPromo) {
    return NSDirectionalEdgeInsetsMake(
        kContentStackVerticalPadding, kContentStackHorizontalPadding,
        kContentStackVerticalPadding, kContentStackHorizontalPadding);
  } else {
    return NSDirectionalEdgeInsetsZero;
  }
}

// Applies constraints to the stack view.
- (void)applyStackViewConstraints {
  if (self.contentStackConstraints) {
    [NSLayoutConstraint deactivateConstraints:self.contentStackConstraints];
  }

  self.contentStackConstraints = SameConstraintsWithInsets(
      self.contentStack, self.view, [self stackViewInsets]);
  [NSLayoutConstraint activateConstraints:self.contentStackConstraints];
}

// Configures a creates a signin promo view.
- (SigninPromoView*)createPromoView {
  DCHECK(self.signinPromoDelegate);
  SigninPromoView* promoView =
      [[SigninPromoView alloc] initWithFrame:CGRectZero];
  promoView.translatesAutoresizingMaskIntoConstraints = NO;
  promoView.delegate = self.signinPromoDelegate;

  SigninPromoViewConfigurator* configurator =
      self.delegate.signinPromoConfigurator;
  SigninPromoViewStyle promoViewStyle = IsDiscoverFeedTopSyncPromoCompact()
                                            ? SigninPromoViewStyleTitledCompact
                                            : SigninPromoViewStyleTitled;
  [configurator configureSigninPromoView:promoView withStyle:promoViewStyle];
  promoView.titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_NTP_FEED_SIGNIN_PROMO_TITLE);
  promoView.textLabel.text =
      IsDiscoverFeedTopSyncPromoCompact()
          ? l10n_util::GetNSString(IDS_IOS_NTP_FEED_SIGNIN_COMPACT_PROMO_BODY)
          : l10n_util::GetNSString(IDS_IOS_NTP_FEED_SIGNIN_FULL_PROMO_BODY);
  // TODO(crbug.com/1331010): Update the Promo icon.
  [promoView setNonProfileImage:[UIImage imageNamed:kPromoViewImageName]];
  return promoView;
}

@end
