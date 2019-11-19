// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

#include "base/logging.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical margin the main title and the identity picker.
const CGFloat kTitlePickerMargin = 16.;
// Vertical margin above the first text and after the last text.
const CGFloat kVerticalTextMargin = 22.;
// Vertical margin between texts.
const CGFloat kVerticalBetweenTextMargin = 25.;
// Vertical margin between separator and text.
const CGFloat kVerticalSeparatorTextMargin = 16.;

// URL for the Settings link.
const char* const kSettingsSyncURL = "internal://settings-sync";
}  // namespace

@interface UnifiedConsentViewController ()<UIScrollViewDelegate> {
  std::vector<int> _consentStringIds;
}

// Read/write internal.
@property(nonatomic, readwrite) int openSettingsStringId;
// Main view.
@property(nonatomic, strong) UIScrollView* scrollView;
// Identity picker to change the identity to sign-in.
@property(nonatomic, strong) IdentityPickerView* identityPickerView;
// Vertical constraint on imageBackgroundView to have it over non-safe area.
@property(nonatomic, strong)
    NSLayoutConstraint* imageBackgroundViewHeightConstraint;
// Constraint when identityPickerView is hidden.
@property(nonatomic, strong) NSLayoutConstraint* noIdentityConstraint;
// Constraint when identityPickerView is visible.
@property(nonatomic, strong) NSLayoutConstraint* withIdentityConstraint;
// Constraint for the maximum height of the header view (also used to hide the
// the header view if needed).
@property(nonatomic, strong) NSLayoutConstraint* headerViewMaxHeightConstraint;
// Constraint for the proportiortional size of the header view.
@property(nonatomic, strong)
    NSLayoutConstraint* headerViewProportionalHeightConstraint;
// Settings link controller.
@property(nonatomic, strong) LabelLinkController* settingsLinkController;
// Label related to customize sync text.
@property(nonatomic, strong) UILabel* customizeSyncLabel;

@end

@implementation UnifiedConsentViewController

@synthesize delegate = _delegate;
@synthesize identityPickerView = _identityPickerView;
@synthesize imageBackgroundViewHeightConstraint =
    _imageBackgroundViewHeightConstraint;
@synthesize noIdentityConstraint = _noIdentityConstraint;
@synthesize openSettingsStringId = _openSettingsStringId;
@synthesize scrollView = _scrollView;
@synthesize settingsLinkController = _settingsLinkController;
@synthesize withIdentityConstraint = _withIdentityConstraint;
@synthesize customizeSyncLabel = _customizeSyncLabel;

- (const std::vector<int>&)consentStringIds {
  return _consentStringIds;
}

- (void)updateIdentityPickerViewWithUserFullName:(NSString*)fullName
                                           email:(NSString*)email {
  DCHECK(email);
  self.identityPickerView.hidden = NO;
  self.noIdentityConstraint.active = NO;
  self.withIdentityConstraint.active = YES;
  [self.identityPickerView setIdentityName:fullName email:email];
  [self setSettingsLinkURLShown:YES];
}

- (void)updateIdentityPickerViewWithAvatar:(UIImage*)avatar {
  DCHECK(!self.identityPickerView.hidden);
  [self.identityPickerView setIdentityAvatar:avatar];
}

- (void)hideIdentityPickerView {
  self.identityPickerView.hidden = YES;
  self.withIdentityConstraint.active = NO;
  self.noIdentityConstraint.active = YES;
  [self setSettingsLinkURLShown:NO];
}

- (void)scrollToBottom {
  // Add one point to make sure that it is actually scrolled to the bottom (as
  // there are some issues when the fonts are increased).
  CGPoint bottomOffset =
      CGPointMake(0, self.scrollView.contentSize.height -
                         self.scrollView.bounds.size.height +
                         self.scrollView.contentInset.bottom + 1);
  [self.scrollView setContentOffset:bottomOffset animated:YES];
}

- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      self.scrollView.contentOffset.y + self.scrollView.frame.size.height;
  CGFloat scrollLimit =
      self.scrollView.contentSize.height + self.scrollView.contentInset.bottom;
  return scrollPosition >= scrollLimit;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Main scroll view.
  self.scrollView = [[UIScrollView alloc] initWithFrame:self.view.bounds];
  self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView.accessibilityIdentifier = kUnifiedConsentScrollViewIdentifier;
  // The observed behavior was buggy. When the view appears on the screen,
  // the scrollview was not scrolled all the way to the top. Adjusting the
  // safe area manually fixes the issue.
  self.scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  [self.view addSubview:self.scrollView];

  // Scroll view container.
  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollView addSubview:container];

  // View used to draw the background color of the header image, in the non-safe
  // areas (like the status bar).
  UIView* imageBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
  imageBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:imageBackgroundView];

  // Header image.
  UIImageView* headerImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  headerImageView.image = [UIImage imageNamed:kAuthenticationHeaderImageName];
  headerImageView.contentMode = UIViewContentModeScaleAspectFit;
  headerImageView.clipsToBounds = YES;
  [container addSubview:headerImageView];

  // Title.
  UILabel* title =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE
                       fontStyle:kAuthenticationTitleFontStyle
                       textColor:UIColor.cr_labelColor
                      parentView:container];

  // Identity picker view.
  self.identityPickerView =
      [[IdentityPickerView alloc] initWithFrame:CGRectZero];
  self.identityPickerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.identityPickerView addTarget:self
                              action:@selector(identityPickerAction:forEvent:)
                    forControlEvents:UIControlEventTouchUpInside];
  [container addSubview:self.identityPickerView];

  // Sync title and subtitle.
  UILabel* syncTitleLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE
                       fontStyle:kAuthenticationTextFontStyle
                       textColor:UIColor.cr_labelColor
                      parentView:container];

  UILabel* syncSubtitleLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE
                       fontStyle:kAuthenticationTextFontStyle
                       textColor:UIColor.cr_secondaryLabelColor
                      parentView:container];

  // Separator.
  UIView* separator = [[UIView alloc] initWithFrame:CGRectZero];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = UIColor.cr_secondarySystemBackgroundColor;
  [container addSubview:separator];

  // Customize label.
  self.openSettingsStringId = IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS;
  self.customizeSyncLabel =
      [self addLabelWithStringId:self.openSettingsStringId
                       fontStyle:kAuthenticationTextFontStyle
                       textColor:UIColor.cr_secondaryLabelColor
                      parentView:container];

  // Layouts
  NSDictionary* views = @{
    @"header" : headerImageView,
    @"title" : title,
    @"picker" : self.identityPickerView,
    @"container" : container,
    @"scrollview" : self.scrollView,
    @"separator" : separator,
    @"synctitle" : syncTitleLabel,
    @"syncsubtitle" : syncSubtitleLabel,
    @"customizesync" : self.customizeSyncLabel,
  };
  NSDictionary* metrics = @{
    @"TitlePickerMargin" : @(kTitlePickerMargin),
    @"HMargin" : @(kAuthenticationHorizontalMargin),
    @"VBetweenText" : @(kVerticalBetweenTextMargin),
    @"VSeparatorText" : @(kVerticalSeparatorTextMargin),
    @"VTextMargin" : @(kVerticalTextMargin),
    @"SeparatorHeight" : @(kAuthenticationSeparatorHeight),
    @"HeaderTitleMargin" : @(kAuthenticationHeaderTitleMargin),
  };
  NSArray* constraints = @[
    // Horitizontal constraints.
    @"H:|[scrollview]|",
    @"H:|[container]|",
    @"H:|-(HMargin)-[title]-(HMargin)-|",
    @"H:|-(HMargin)-[picker]-(HMargin)-|",
    @"H:|-(HMargin)-[separator]-(HMargin)-|",
    @"H:|-(HMargin)-[synctitle]-(HMargin)-|",
    @"H:|-(HMargin)-[syncsubtitle]-(HMargin)-|",
    @"H:|-(HMargin)-[customizesync]-(HMargin)-|",
    // Vertical constraints.
    @"V:|[scrollview]|",
    @"V:|[container]|",
    @"V:|[header]-(HeaderTitleMargin)-[title]-(TitlePickerMargin)-[picker]",
    @"V:[synctitle]-[syncsubtitle]-(VBetweenText)-[separator]",
    @"V:[separator]-(VSeparatorText)-[customizesync]-(VTextMargin)-|",
    // Size constraints.
    @"V:[separator(SeparatorHeight)]",
  ];
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

  // Adding constraints for header image.
  AddSameCenterXConstraint(self.view, headerImageView);
  // |headerView| fills 20% of |view|, capped at
  // |kAuthenticationHeaderImageHeight|.
  self.headerViewProportionalHeightConstraint = [headerImageView.heightAnchor
      constraintEqualToAnchor:self.view.heightAnchor
                   multiplier:0.2];
  self.headerViewProportionalHeightConstraint.priority =
      UILayoutPriorityDefaultHigh;
  self.headerViewProportionalHeightConstraint.active = YES;
  self.headerViewMaxHeightConstraint = [headerImageView.heightAnchor
      constraintLessThanOrEqualToConstant:kAuthenticationHeaderImageHeight];
  self.headerViewMaxHeightConstraint.active = YES;
  [self updateHeaderViewConstraints];

  // Adding constraints with or without identity.
  self.noIdentityConstraint =
      [syncTitleLabel.topAnchor constraintEqualToAnchor:title.bottomAnchor
                                               constant:kVerticalTextMargin];
  self.withIdentityConstraint = [syncTitleLabel.topAnchor
      constraintEqualToAnchor:self.identityPickerView.bottomAnchor
                     constant:kVerticalTextMargin];

  // Adding constraints for the container.
  id<LayoutGuideProvider> safeArea = self.view.safeAreaLayoutGuide;
  [container.widthAnchor constraintEqualToAnchor:safeArea.widthAnchor].active =
      YES;

  // Adding constraints for |imageBackgroundView|.
  AddSameCenterXConstraint(self.view, imageBackgroundView);
  [imageBackgroundView.widthAnchor
      constraintEqualToAnchor:self.view.widthAnchor]
      .active = YES;
  self.imageBackgroundViewHeightConstraint = [imageBackgroundView.heightAnchor
      constraintEqualToAnchor:headerImageView.heightAnchor];
  self.imageBackgroundViewHeightConstraint.active = YES;
  [imageBackgroundView.bottomAnchor
      constraintEqualToAnchor:headerImageView.bottomAnchor]
      .active = YES;

  // Update UI.
  [self hideIdentityPickerView];
  [self updateScrollViewAndImageBackgroundView];
}

- (void)viewSafeAreaInsetsDidChange {
  // Updates the scroll view content inset, used by iOS 11 or later.
  [super viewSafeAreaInsetsDidChange];
  [self updateScrollViewAndImageBackgroundView];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.delegate unifiedConsentViewControllerViewDidAppear:self];
}

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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateHeaderViewConstraints];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent)
    return;
  [parent.view layoutIfNeeded];
  // Needs to add the scroll view delegate only when all the view layouts are
  // fully done.
  dispatch_async(dispatch_get_main_queue(), ^{
    // Having a layout of the parent view makes the scroll view not being
    // presented at the top. Scrolling to the top is required.
    CGPoint topOffset = CGPointMake(self.scrollView.contentOffset.x,
                                    -self.scrollView.contentInset.top);
    [self.scrollView setContentOffset:topOffset animated:NO];
    self.scrollView.delegate = self;
    [self sendDidReachBottomIfReached];
  });
}

#pragma mark - UI actions

- (void)identityPickerAction:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate
      unifiedConsentViewControllerDidTapIdentityPickerView:self
                                                   atPoint:
                                                       [touch
                                                           locationInView:nil]];
}

#pragma mark - Private

// Adds label with title |stringId| into |parentView|.
- (UILabel*)addLabelWithStringId:(int)stringId
                       fontStyle:(UIFontTextStyle)fontStyle
                       textColor:(UIColor*)textColor
                      parentView:(UIView*)parentView {
  DCHECK(stringId);
  DCHECK(parentView);
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:fontStyle];
  label.textColor = textColor;
  label.text = l10n_util::GetNSString(stringId);
  _consentStringIds.push_back(stringId);
  label.numberOfLines = 0;
  [parentView addSubview:label];
  return label;
}

// Adds or removes the Settings link in |self.customizeSyncLabel|.
- (void)setSettingsLinkURLShown:(BOOL)showLink {
  self.customizeSyncLabel.text =
      l10n_util::GetNSString(self.openSettingsStringId);
  GURL URL = google_util::AppendGoogleLocaleParam(
      GURL(kSettingsSyncURL), GetApplicationContext()->GetApplicationLocale());
  NSRange range;
  NSString* text = self.customizeSyncLabel.text;
  self.customizeSyncLabel.text = ParseStringWithLink(text, &range);
  DCHECK(range.location != NSNotFound && range.length != 0);
  if (!showLink) {
    self.settingsLinkController = nil;
  } else {
    __weak UnifiedConsentViewController* weakSelf = self;
    self.settingsLinkController =
        [[LabelLinkController alloc] initWithLabel:self.customizeSyncLabel
                                            action:^(const GURL& URL) {
                                              [weakSelf openSettings];
                                            }];
    [self.settingsLinkController setLinkColor:[UIColor colorNamed:kBlueColor]];
    [self.settingsLinkController
        addLinkWithRange:range
                     url:URL
         accessibilityID:kAdvancedSigninSettingsLinkIdentifier];
  }
}

// Updates constraints and content insets for the |scrollView| and
// |imageBackgroundView| related to non-safe area.
- (void)updateScrollViewAndImageBackgroundView {
  self.scrollView.contentInset = self.view.safeAreaInsets;
  self.imageBackgroundViewHeightConstraint.constant =
      self.view.safeAreaInsets.top;
  if (self.scrollView.delegate == self) {
    // Don't send the notification if the delegate is not configured yet.
    [self sendDidReachBottomIfReached];
  }
}

// Notifies |delegate| that the user tapped on "Settings" link.
- (void)openSettings {
  [self.delegate unifiedConsentViewControllerDidTapSettingsLink:self];
}

// Sends notification to the delegate if the scroll view is scrolled to the
// bottom.
- (void)sendDidReachBottomIfReached {
  if (self.isScrolledToBottom) {
    [self.delegate unifiedConsentViewControllerDidReachBottom:self];
  }
}

// Updates the header view constraints based on the height class traits of
// |view|.
- (void)updateHeaderViewConstraints {
  if (IsCompactHeight(self)) {
    self.headerViewMaxHeightConstraint.constant = 0;
  } else {
    self.headerViewMaxHeightConstraint.constant =
        kAuthenticationHeaderImageHeight;
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollView, scrollView);
  [self sendDidReachBottomIfReached];
}

@end
