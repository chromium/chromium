// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_personalization_view_controller.h"

#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_option_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTitleTextMargin = 16;
const CGFloat kOptionsVerticalMargin = 16;
}  // namespace

@interface ConsentBumpPersonalizationViewController ()

// Main view.
@property(nonatomic, strong) UIScrollView* scrollView;
// Vertical constraint on imageBackgroundView to have it over non-safe area.
@property(nonatomic, strong)
    NSLayoutConstraint* imageBackgroundViewHeightConstraint;

// Array containing all the options presented by this ViewController.
@property(nonatomic, copy) NSArray<ConsentBumpOptionButton*>* options;
// Redefined as readwrite.
@property(nonatomic, assign, readwrite) ConsentBumpOptionType selectedOption;

@end

@implementation ConsentBumpPersonalizationViewController

@synthesize scrollView = _scrollView;
@synthesize imageBackgroundViewHeightConstraint =
    _imageBackgroundViewHeightConstraint;
@synthesize options = _options;
@synthesize selectedOption = _selectedOption;

- (void)viewDidLoad {
  [super viewDidLoad];

  // Main scroll view.
  self.scrollView = [[UIScrollView alloc] init];
  self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  if (@available(iOS 11, *)) {
    // The observed behavior was buggy. When the view appears on the screen,
    // the scrollview was not scrolled all the way to the top. Adjusting the
    // safe area manually fixes the issue.
    self.scrollView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  [self.view addSubview:self.scrollView];

  // Scroll view container.
  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollView addSubview:container];

  // View used to draw the background color of the header image, in the non-safe
  // areas (like the status bar).
  UIView* imageBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
  imageBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  imageBackgroundView.backgroundColor =
      UIColorFromRGB(kAuthenticationHeaderBackgroundColor);
  [container addSubview:imageBackgroundView];

  // Header image.
  UIImageView* headerImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  headerImageView.image = [UIImage imageNamed:kAuthenticationHeaderImageName];
  [container addSubview:headerImageView];

  // Title.
  UILabel* title = [[UILabel alloc] initWithFrame:CGRectZero];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text =
      l10n_util::GetNSString(IDS_IOS_CONSENT_BUMP_PERSONALIZATION_TITLE);
  title.textColor =
      [UIColor colorWithWhite:0 alpha:kAuthenticationTitleColorAlpha];
  title.font = [UIFont preferredFontForTextStyle:kAuthenticationTitleFontStyle];
  title.numberOfLines = 0;
  [container addSubview:title];

  // Text.
  UILabel* text = [[UILabel alloc] initWithFrame:CGRectZero];
  text.translatesAutoresizingMaskIntoConstraints = NO;
  text.text =
      l10n_util::GetNSString(IDS_IOS_CONSENT_BUMP_PERSONALIZATION_MESSAGE);
  text.textColor =
      [UIColor colorWithWhite:0 alpha:kAuthenticationTextColorAlpha];
  text.font = [UIFont preferredFontForTextStyle:kAuthenticationTextFontStyle];
  text.numberOfLines = 0;
  [container addSubview:text];

  // Options.
  ConsentBumpOptionButton* noChangeOption = [ConsentBumpOptionButton
      consentBumpOptionButtonWithTitle:l10n_util::GetNSString(
                                           IDS_IOS_CONSENT_BUMP_NO_CHANGE_TITLE)
                                  text:
                                      l10n_util::GetNSString(
                                          IDS_IOS_CONSENT_BUMP_NO_CHANGE_TEXT)];
  noChangeOption.type = ConsentBumpOptionTypeMoreOptionsNoChange;
  noChangeOption.checked = YES;
  self.selectedOption = ConsentBumpOptionTypeMoreOptionsNoChange;

  ConsentBumpOptionButton* reviewOption = [ConsentBumpOptionButton
      consentBumpOptionButtonWithTitle:l10n_util::GetNSString(
                                           IDS_IOS_CONSENT_BUMP_REVIEW_TITLE)
                                  text:nil];
  reviewOption.type = ConsentBumpOptionTypeMoreOptionsReview;
  reviewOption.checked = NO;

  ConsentBumpOptionButton* turnOnOption = [ConsentBumpOptionButton
      consentBumpOptionButtonWithTitle:l10n_util::GetNSString(
                                           IDS_IOS_CONSENT_BUMP_TURN_ON_TITLE)
                                  text:l10n_util::GetNSString(
                                           IDS_IOS_CONSENT_BUMP_TURN_ON_TEXT)];
  turnOnOption.type = ConsentBumpOptionTypeMoreOptionsTurnOn;
  turnOnOption.checked = NO;

  self.options = @[ noChangeOption, reviewOption, turnOnOption ];

  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self.view);
  AddSameConstraints(self.view, self.scrollView);
  AddSameConstraints(container, self.scrollView);
  AddSameCenterXConstraint(container, headerImageView);
  AddSameConstraintsToSides(imageBackgroundView, self.view,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  self.imageBackgroundViewHeightConstraint = [imageBackgroundView.heightAnchor
      constraintEqualToAnchor:headerImageView.heightAnchor];
  [NSLayoutConstraint activateConstraints:@[
    self.imageBackgroundViewHeightConstraint,
    [imageBackgroundView.bottomAnchor
        constraintEqualToAnchor:headerImageView.bottomAnchor],
    [container.widthAnchor constraintEqualToAnchor:safeArea.widthAnchor],
  ]];

  NSDictionary* views = @{
    @"title" : title,
    @"text" : text,
    @"header" : headerImageView,
  };
  NSDictionary* metrics = @{
    @"HMargin" : @(kAuthenticationHorizontalMargin),
    @"HeaderTitleMargin" : @(kAuthenticationHeaderTitleMargin),
    @"TitleTextMargin" : @(kTitleTextMargin),
  };
  NSArray* constraints = @[
    @"H:|-(HMargin)-[title]-(HMargin)-|",
    @"H:|-(HMargin)-[text]-(HMargin)-|",
    @"V:|[header]-(HeaderTitleMargin)-[title]-(TitleTextMargin)-[text]",
  ];
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

  // Options positioning.
  UIView* viewAbove = nil;
  for (ConsentBumpOptionButton* option in self.options) {
    option.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:option];
    [option addTarget:self
                  action:@selector(optionTapped:)
        forControlEvents:UIControlEventTouchUpInside];

    if (viewAbove) {
      [option.topAnchor constraintEqualToAnchor:viewAbove.bottomAnchor].active =
          YES;
    }
    [NSLayoutConstraint activateConstraints:@[
      [option.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
      [option.trailingAnchor constraintEqualToAnchor:container.trailingAnchor]
    ]];
    viewAbove = option;
  }

  // Positioning the first and last options.
  [NSLayoutConstraint activateConstraints:@[
    [self.options[0].topAnchor constraintEqualToAnchor:text.bottomAnchor
                                              constant:kOptionsVerticalMargin],
    [container.bottomAnchor
        constraintEqualToAnchor:self.options[self.options.count - 1]
                                    .bottomAnchor
                       constant:kOptionsVerticalMargin],
  ]];

  [self updateScrollViewAndImageBackgroundView];
}

- (void)viewSafeAreaInsetsDidChange {
  // Updates the scroll view content inset, used by iOS 11 or later.
  [super viewSafeAreaInsetsDidChange];
  [self updateScrollViewAndImageBackgroundView];
}

// Updates the scroll view content inset, used by pre iOS 11.
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self updateScrollViewAndImageBackgroundView];
      }
                      completion:nil];
}

#pragma mark - Private

- (void)optionTapped:(ConsentBumpOptionButton*)tappedOption {
  for (ConsentBumpOptionButton* option in self.options) {
    option.checked = NO;
  }
  tappedOption.checked = YES;
  self.selectedOption = tappedOption.type;
}

// Updates constraints and content insets for the |scrollView| and
// |imageBackgroundView| related to non-safe area.
- (void)updateScrollViewAndImageBackgroundView {
  if (@available(iOS 11, *)) {
    self.scrollView.contentInset = self.view.safeAreaInsets;
    self.imageBackgroundViewHeightConstraint.constant =
        self.view.safeAreaInsets.top;
  } else {
    CGFloat statusBarHeight =
        [UIApplication sharedApplication].isStatusBarHidden ? 0.
                                                            : StatusBarHeight();
    self.scrollView.contentInset = UIEdgeInsetsMake(statusBarHeight, 0, 0, 0);
    self.imageBackgroundViewHeightConstraint.constant = statusBarHeight;
  }
}

@end
