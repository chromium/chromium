// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Content stack padding.
const CGFloat kContentStackVerticalPadding = 9;

// Content stack padding for the notifications promo view.
const CGFloat kNotificationsContentStackTopPadding = 17;
const CGFloat kNotificationsContentStackBottomPadding = 1;
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

// The promo view UIView object. Could be `SigninPromoView` or
// `NotificationsPromoView`.
@property(nonatomic, strong) UIView* promoView;

// The signin promo view.
@property(nonatomic, strong) SigninPromoView* signinPromoView;

// The notifications promo view.
@property(nonatomic, strong) NotificationsPromoView* notificationsPromoView;

// View to contain the signin promo.
@property(nonatomic, strong) UIView* promoViewContainer;

// Stores the current UI constraints for the stack view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* contentStackConstraints;

@end

@implementation FeedTopSectionViewController

@synthesize visiblePromoViewType;

- (instancetype)init {
  self = [super init];
  if (self) {
    // Create `contentStack` early so we can set up the promo before
    // `viewDidLoad`.
    _contentStack = [[UIStackView alloc] init];
    _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStack.axis = UILayoutConstraintAxisVertical;
    _contentStack.distribution = UIStackViewDistributionFill;
    // TODO(crbug.com/40843602): Update background color for the view.
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
- (void)createPromoViewContainerForPromoType:(PromoViewType)type {
  DCHECK(!self.promoViewContainer);
  DCHECK(!self.promoView);
  self.promoViewContainer = [[UIView alloc] init];
  self.promoViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.promoViewContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];

  self.promoViewContainer.layer.cornerRadius = kPromoViewContainerBorderRadius;
  self.visiblePromoViewType = type;
  switch (type) {
    case PromoViewTypeSignin:
      self.signinPromoView = [self createSigninPromoView];
      self.promoView = self.signinPromoView;
      break;
    case PromoViewTypeNotifications:
      self.notificationsPromoView = [self createNotificationsPromoView];
      self.promoView = self.notificationsPromoView;
      break;
  }
  // Add the subview to the promoViewContainer.
  [self.promoViewContainer addSubview:self.promoView];
  [self.contentStack addArrangedSubview:self.promoViewContainer];
  AddSameConstraints(self.promoViewContainer, self.promoView);
}

- (void)updateSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)configurator {
  [configurator configureSigninPromoView:self.signinPromoView
                               withStyle:SigninPromoViewStyleCompact];
}

#pragma mark - Properties

- (void)setFeedTopSectionMutator:(id<FeedTopSectionMutator>)mutator {
  _feedTopSectionMutator = mutator;
  self.notificationsPromoView.mutator = _feedTopSectionMutator;
}

- (void)setSigninPromoDelegate:(id<SigninPromoViewDelegate>)delegate {
  _signinPromoDelegate = delegate;
  self.signinPromoView.delegate = _signinPromoDelegate;
}

#pragma mark - Private

// Returns insets to add a margin around the stackview if there are items
// to display in the stackview. Otherwise returns NSDirectionalEdgeInsetsZero.
// `visible` indicates whether or not the Feed Top Section is visible.
- (NSDirectionalEdgeInsets)stackViewInsetsForTopSectionVisible:(BOOL)visible {
  if (!visible) {
    return NSDirectionalEdgeInsetsZero;
  }
  if (self.notificationsPromoView) {
    return NSDirectionalEdgeInsetsMake(
        kNotificationsContentStackTopPadding, kContentStackVerticalPadding,
        kNotificationsContentStackBottomPadding, kContentStackVerticalPadding);
  } else {
    return NSDirectionalEdgeInsetsMake(
        kContentStackVerticalPadding, kContentStackVerticalPadding,
        kContentStackVerticalPadding, kContentStackVerticalPadding);
  }
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

- (void)showPromo {
  // Hide any visible promo when `showPromo` is called to display a new one.
  if (self.promoViewContainer) {
    [self hidePromo];
  }
  // Check if the promoViewContainer does not exist. Might not exist if the
  // promo has been "hidden", which involves removing the container.
  if (!self.promoViewContainer) {
    [self createPromoViewContainerForPromoType:self.visiblePromoViewType];
  }
  [self applyStackViewConstraintsForTopSectionVisible:YES];
  [self.NTPDelegate updateFeedLayout];
}

- (void)hidePromo {
  [self.contentStack willRemoveSubview:self.promoViewContainer];
  [self.promoViewContainer willRemoveSubview:self.promoView];
  [self.promoView removeFromSuperview];
  [self.promoViewContainer removeFromSuperview];
  self.promoViewContainer = nil;
  self.promoView = nil;
  self.signinPromoView = nil;
  self.notificationsPromoView = nil;
  [self applyStackViewConstraintsForTopSectionVisible:NO];
}

// TODO(b/312248486): Assign configurator and delegate here.
- (NotificationsPromoView*)createNotificationsPromoView {
  NotificationsPromoView* promoView =
      [[NotificationsPromoView alloc] initWithFrame:CGRectZero];
  promoView.translatesAutoresizingMaskIntoConstraints = NO;
  promoView.mutator = self.feedTopSectionMutator;
  return promoView;
}

- (SigninPromoView*)createSigninPromoView {
  SigninPromoView* promoView =
      [[SigninPromoView alloc] initWithFrame:CGRectZero];
  promoView.translatesAutoresizingMaskIntoConstraints = NO;
  promoView.delegate = self.signinPromoDelegate;

  SigninPromoViewConfigurator* configurator =
      self.delegate.signinPromoConfigurator;
  [configurator configureSigninPromoView:promoView
                               withStyle:SigninPromoViewStyleCompact];

  promoView.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_SHEET_LABEL_FOR_FEED_PROMO);
  return promoView;
}

@end
