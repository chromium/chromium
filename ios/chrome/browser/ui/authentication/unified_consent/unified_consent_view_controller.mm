// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/ns_range.h"
#import "base/notreached.h"
#import "components/google/core/common/google_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller_delegate.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

@interface UnifiedConsentViewController () <UIScrollViewDelegate,
                                            UITextViewDelegate> {
  std::vector<int> _consentStringIds;
  // YES if the dialog is used for post restore sign-in promo.
  BOOL _postRestoreSigninPromo;
}

// Main view.
@property(nonatomic, strong) UIScrollView* scrollView;
// Identity picker to change the identity to sign-in.
@property(nonatomic, strong) IdentityButtonControl* identityButtonControl;
// Vertical constraint on imageBackgroundView to have it over non-safe area.
@property(nonatomic, strong)
    NSLayoutConstraint* imageBackgroundViewHeightConstraint;
// Constraint when identityButtonControl is hidden.
@property(nonatomic, strong) NSLayoutConstraint* noIdentityConstraint;
// Constraint when identityButtonControl is visible.
@property(nonatomic, strong) NSLayoutConstraint* withIdentityConstraint;
// Constraint for the maximum height of the header view (also used to hide the
// the header view if needed).
@property(nonatomic, strong) NSLayoutConstraint* headerViewMaxHeightConstraint;
// Text description that may show link to advanced Sync settings.
@property(nonatomic, strong) UITextView* syncSettingsTextView;
// Text description that show a link to open the management help page.
@property(nonatomic, strong) UITextView* managementNoticeTextView;

@end

@implementation UnifiedConsentViewController

- (instancetype)initWithPostRestoreSigninPromo:(BOOL)postRestoreSigninPromo {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _postRestoreSigninPromo = postRestoreSigninPromo;
  }
  return self;
}

- (const std::vector<int>&)consentStringIds {
  return _consentStringIds;
}

- (void)updateIdentityButtonControlWithUserFullName:(NSString*)fullName
                                              email:(NSString*)email {
  DCHECK(email);
  self.identityButtonControl.hidden = NO;
  self.noIdentityConstraint.active = NO;
  self.withIdentityConstraint.active = YES;
  [self.identityButtonControl setIdentityName:fullName email:email];
  [self setSettingsLinkURLShown:YES];
}

- (void)updateIdentityButtonControlWithAvatar:(UIImage*)avatar {
  DCHECK(!self.identityButtonControl.hidden);
  [self.identityButtonControl setIdentityAvatar:avatar];
}

- (void)hideIdentityButtonControl {
  self.identityButtonControl.hidden = YES;
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
                       fontStyle:UIFontTextStyleTitle1
                       textColor:[UIColor colorNamed:kTextPrimaryColor]
                      parentView:container];

  // Identity picker view.
  self.identityButtonControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  self.identityButtonControl.translatesAutoresizingMaskIntoConstraints = NO;
  [self.identityButtonControl addTarget:self
                                 action:@selector(identityButtonControlAction:
                                                                     forEvent:)
                       forControlEvents:UIControlEventTouchUpInside];
  [container addSubview:self.identityButtonControl];

  // Sync title and subtitle.
  int stringId =
      self.delegate.unifiedConsentCoordinatorHasManagedSyncDataType
          ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_MANAGED_TITLE
          : IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE_WITHOUT_PASSWORDS;
  UILabel* syncTitleLabel =
      [self addLabelWithStringId:stringId
                       fontStyle:UIFontTextStyleSubheadline
                       textColor:[UIColor colorNamed:kTextPrimaryColor]
                      parentView:container];

  UILabel* syncSubtitleLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE
                       fontStyle:UIFontTextStyleSubheadline
                       textColor:[UIColor colorNamed:kTextSecondaryColor]
                      parentView:container];

  // Separator.
  UIView* separator = [[UIView alloc] initWithFrame:CGRectZero];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  [container addSubview:separator];

  // Sync settings description.
  self.syncSettingsTextView = CreateUITextViewWithTextKit1();
  self.syncSettingsTextView.scrollEnabled = NO;
  self.syncSettingsTextView.editable = NO;
  self.syncSettingsTextView.delegate = self;
  self.syncSettingsTextView.backgroundColor = UIColor.clearColor;
  self.syncSettingsTextView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  self.syncSettingsTextView.adjustsFontForContentSizeCategory = YES;
  self.syncSettingsTextView.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:self.syncSettingsTextView];

  // Layouts
  NSDictionary* views = @{
    @"header" : headerImageView,
    @"title" : title,
    @"picker" : self.identityButtonControl,
    @"container" : container,
    @"scrollview" : self.scrollView,
    @"separator" : separator,
    @"synctitle" : syncTitleLabel,
    @"syncsubtitle" : syncSubtitleLabel,
    @"customizesync" : self.syncSettingsTextView,
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
    @"V:[separator]-(VSeparatorText)-[customizesync]-(>=VTextMargin)-|",
    // Size constraints.
    @"V:[separator(SeparatorHeight)]",
  ];
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

  if (self.delegate.unifiedConsentCoordinatorHasAccountRestrictions ||
      self.delegate.unifiedConsentCoordinatorHasManagedSyncDataType) {
    // Manage settings description.
    [container addSubview:self.managementNoticeTextView];
    [NSLayoutConstraint activateConstraints:@[
      [self.managementNoticeTextView.leadingAnchor
          constraintEqualToAnchor:self.syncSettingsTextView.leadingAnchor],
      [self.managementNoticeTextView.trailingAnchor
          constraintEqualToAnchor:self.syncSettingsTextView.trailingAnchor],
      [self.managementNoticeTextView.topAnchor
          constraintEqualToAnchor:self.syncSettingsTextView.bottomAnchor],
      [self.managementNoticeTextView.bottomAnchor
          constraintLessThanOrEqualToAnchor:container.bottomAnchor
                                   constant:-kVerticalTextMargin],
    ]];
  }

  // Adding constraints for header image.
  AddSameCenterXConstraint(self.view, headerImageView);
  // `headerView` fills 20% of `view`, capped at
  // `kAuthenticationHeaderImageHeight`.
  [headerImageView.heightAnchor
      constraintLessThanOrEqualToAnchor:self.view.heightAnchor
                             multiplier:0.2]
      .active = YES;
  self.headerViewMaxHeightConstraint = [headerImageView.heightAnchor
      constraintLessThanOrEqualToConstant:kAuthenticationHeaderImageHeight];
  self.headerViewMaxHeightConstraint.active = YES;
  [self updateHeaderViewConstraints];

  // Adding constraints with or without identity.
  self.noIdentityConstraint =
      [syncTitleLabel.topAnchor constraintEqualToAnchor:title.bottomAnchor
                                               constant:kVerticalTextMargin];
  self.withIdentityConstraint = [syncTitleLabel.topAnchor
      constraintEqualToAnchor:self.identityButtonControl.bottomAnchor
                     constant:kVerticalTextMargin];

  // Adding constraints for the container.
  id<LayoutGuideProvider> safeArea = self.view.safeAreaLayoutGuide;
  [container.widthAnchor constraintEqualToAnchor:safeArea.widthAnchor].active =
      YES;

  // Adding constraints for `imageBackgroundView`.
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
  [self hideIdentityButtonControl];
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

- (void)identityButtonControlAction:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate
      unifiedConsentViewControllerDidTapIdentityButtonControl:self
                                                      atPoint:
                                                          [touch locationInView:
                                                                     nil]];
}

#pragma mark - Private

- (UITextView*)managementNoticeTextView {
  if (_managementNoticeTextView)
    return _managementNoticeTextView;

  _managementNoticeTextView = CreateUITextViewWithTextKit1();
  _managementNoticeTextView.scrollEnabled = NO;
  _managementNoticeTextView.editable = NO;
  _managementNoticeTextView.delegate = self;
  _managementNoticeTextView.adjustsFontForContentSizeCategory = YES;
  _managementNoticeTextView.translatesAutoresizingMaskIntoConstraints = NO;
  _managementNoticeTextView.backgroundColor = UIColor.clearColor;

  NSString* fullText =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_SIGNIN_LEARN_MORE);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSLinkAttributeName : net::NSURLWithGURL(GURL(kChromeUIManagementURL)),
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };
  _managementNoticeTextView.attributedText = AttributedStringFromStringWithLink(
      fullText, textAttributes, linkAttributes);

  return _managementNoticeTextView;
}

// Adds label with title `stringId` into `parentView`.
- (UILabel*)addLabelWithStringId:(int)stringId
                       fontStyle:(UIFontTextStyle)fontStyle
                       textColor:(UIColor*)textColor
                      parentView:(UIView*)parentView {
  DCHECK(stringId);
  DCHECK(parentView);
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.adjustsFontForContentSizeCategory = YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:fontStyle];
  label.textColor = textColor;
  label.text = l10n_util::GetNSString(stringId);
  _consentStringIds.push_back(stringId);
  label.numberOfLines = 0;
  [parentView addSubview:label];
  return label;
}

// Displays the description used for advanced Sync Settings. The link to
// customize Settings is shown when there is at least one selected identity on
// the device.
- (void)setSettingsLinkURLShown:(BOOL)showLink {
  int openSettingsStringId;
  if (_postRestoreSigninPromo) {
    openSettingsStringId =
        self.delegate.unifiedConsentCoordinatorHasManagedSyncDataType
            ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_MANAGED_SETTINGS_POST_RESTORE_PROMO
            : IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS_POST_RESTORE_PROMO;
  } else {
    openSettingsStringId =
        self.delegate.unifiedConsentCoordinatorHasManagedSyncDataType
            ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_MANAGED_SETTINGS
            : IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS;
  }
  NSString* text = l10n_util::GetNSString(openSettingsStringId);
  _consentStringIds.push_back(openSettingsStringId);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSDictionary* linkAttributes = nil;
  if (showLink) {
    linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
      NSLinkAttributeName : net::NSURLWithGURL(GURL(kSettingsSyncURL)),
    };
  }
  self.syncSettingsTextView.attributedText =
      AttributedStringFromStringWithLink(text, textAttributes, linkAttributes);
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView == self.syncSettingsTextView) {
    DCHECK([URL isEqual:net::NSURLWithGURL(GURL(kSettingsSyncURL))]);
    [self.delegate unifiedConsentViewControllerDidTapSettingsLink:self];
  } else if (textView == self.managementNoticeTextView) {
    DCHECK([URL isEqual:net::NSURLWithGURL(GURL(kChromeUIManagementURL))]);
    [self.delegate unifiedConsentViewControllerDidTapLearnMoreLink:self];
  } else {
    NOTREACHED();
  }
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

// Updates constraints and content insets for the `scrollView` and
// `imageBackgroundView` related to non-safe area.
- (void)updateScrollViewAndImageBackgroundView {
  self.scrollView.contentInset = self.view.safeAreaInsets;
  self.imageBackgroundViewHeightConstraint.constant =
      self.view.safeAreaInsets.top;
  if (self.scrollView.delegate == self) {
    // Don't send the notification if the delegate is not configured yet.
    [self sendDidReachBottomIfReached];
  }
}

// Sends notification to the delegate if the scroll view is scrolled to the
// bottom.
- (void)sendDidReachBottomIfReached {
  if (self.isScrolledToBottom) {
    [self.delegate unifiedConsentViewControllerDidReachBottom:self];
  }
}

// Updates the header view constraints based on the height class traits of
// `view`.
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
