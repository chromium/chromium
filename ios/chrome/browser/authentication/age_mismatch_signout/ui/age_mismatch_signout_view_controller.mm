// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/identity_view.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kIdentityViewCornerRadius = 8.0;

// Default margin between subtitle and specific content.
constexpr CGFloat kDefaultSubtitleBottomMargin = 22.0;

}  // namespace

@interface AgeMismatchSignoutViewController ()
// View displaying the identity that was signed out.
@property(nonatomic, strong) IdentityView* identityView;
// Text view displaying the subtitle with a link.
@property(nonatomic, strong) UITextView* subtitleTextView;
@end

@implementation AgeMismatchSignoutViewController {
  AgeMismatchPromptMode _mode;
  // Waiting view displayed when blocking the UI.
  HomeWaitingView* _waitingView;
  // YES if the "Stay signed out" button should be shown. Defaults to YES.
  BOOL _showStaySignedOutButton;
}

- (instancetype)initWithMode:(AgeMismatchPromptMode)mode {
  self = [super init];
  if (self) {
    _mode = mode;
    _showStaySignedOutButton = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kImage;
  self.headerBackgroundImage =
      [UIImage imageNamed:@"age_mismatch_prompt_image"];
  self.modalInPresentation = YES;

  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;

  switch (_mode) {
    case AgeMismatchPromptMode::kSigninFlow:
      self.titleText =
          l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_FOLLOW_UP_HEADER);
      break;
    case AgeMismatchPromptMode::kStandard:
      self.titleText =
          l10n_util::GetNSString(isIPad ? IDS_IOS_AGE_MISMATCH_HEADER_IPAD
                                        : IDS_IOS_AGE_MISMATCH_HEADER_IPHONE);
      break;
  }

  // Hide the default subtitle area.
  self.subtitleBottomMargin = 0;

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_PRIMARY_BUTTON);
  if (_showStaySignedOutButton) {
    self.configuration.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_SECONDARY_BUTTON);
  }

  [self.specificContentView addSubview:self.subtitleTextView];

  NSMutableArray<NSLayoutConstraint*>* constraints =
      [[NSMutableArray alloc] init];
  [constraints addObjectsFromArray:@[
    [self.subtitleTextView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [self.subtitleTextView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [self.subtitleTextView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
  ]];

  // Add the identity view only for the follow up prompt.
  switch (_mode) {
    case AgeMismatchPromptMode::kSigninFlow:
      [self.specificContentView addSubview:self.identityView];
      [constraints addObjectsFromArray:@[
        [self.identityView.topAnchor
            constraintEqualToAnchor:self.subtitleTextView.bottomAnchor
                           constant:kDefaultSubtitleBottomMargin],
        [self.identityView.centerXAnchor
            constraintEqualToAnchor:self.specificContentView.centerXAnchor],
        [self.identityView.widthAnchor
            constraintEqualToAnchor:self.specificContentView.widthAnchor],
        [self.identityView.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.specificContentView
                                                  .bottomAnchor],
      ]];
      break;
    case AgeMismatchPromptMode::kStandard:
      [constraints addObject:[self.subtitleTextView.bottomAnchor
                                 constraintLessThanOrEqualToAnchor:
                                     self.specificContentView.bottomAnchor]];
      break;
  }

  [NSLayoutConstraint activateConstraints:constraints];

  [super viewDidLoad];
}

#pragma mark - Properties

- (IdentityView*)identityView {
  if (!_identityView) {
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    _identityView.layer.cornerRadius = kIdentityViewCornerRadius;
    _identityView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

    // IdentityView defines its vertical bounds using inequality constraints
    // (greaterThanOrEqualTo) so it can be optionally stretched and centered.
    // Because it lacks an equality constraint or an intrinsic content size,
    // it will stretch uncontrollably unless countered. We use a low-priority
    // height constraint to pull it tightly around its contents.
    NSLayoutConstraint* heightConstraint =
        [_identityView.heightAnchor constraintEqualToConstant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow - 1;
    heightConstraint.active = YES;
  }
  return _identityView;
}

- (UITextView*)subtitleTextView {
  if (!_subtitleTextView) {
    _subtitleTextView = [[UITextView alloc] initWithFrame:CGRectZero];
    _subtitleTextView.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleTextView.editable = NO;
    _subtitleTextView.scrollEnabled = NO;
    _subtitleTextView.backgroundColor = [UIColor clearColor];
    _subtitleTextView.adjustsFontForContentSizeCategory = YES;
    _subtitleTextView.textContainerInset = UIEdgeInsetsZero;
    _subtitleTextView.textContainer.lineFragmentPadding = 0;
    _subtitleTextView.delegate = self;
    _subtitleTextView.accessibilityIdentifier =
        kPromoStyleSubtitleAccessibilityIdentifier;

    NSString* combinedText;
    switch (_mode) {
      case AgeMismatchPromptMode::kSigninFlow:
        combinedText =
            l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_FOLLOW_UP_SUBTITLE);
        break;
      case AgeMismatchPromptMode::kStandard: {
        NSString* part1 =
            l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_SUBTITLE_PART1);
        NSString* part2 =
            l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_SUBTITLE_PART2);
        combinedText = [NSString stringWithFormat:@"%@\n\n%@", part1, part2];
        break;
      }
    }

    NSMutableParagraphStyle* paragraphStyle =
        [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
    paragraphStyle.alignment = NSTextAlignmentCenter;

    NSDictionary* textAttributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
      NSForegroundColorAttributeName : [UIColor colorNamed:kGrey800Color],
      NSParagraphStyleAttributeName : paragraphStyle
    };
    NSDictionary* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSLinkAttributeName :
          [NSURL URLWithString:kAgeMismatchSignoutLearnMoreURL]
    };

    _subtitleTextView.attributedText = AttributedStringFromStringWithLink(
        combinedText, textAttributes, linkAttributes);
  }
  return _subtitleTextView;
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (textView == self.subtitleTextView) {
    NSURL* URL = textItem.link;
    if ([self.delegate respondsToSelector:@selector(didTapURLInDisclaimer:)]) {
      __weak __typeof(self) weakSelf = self;
      return [UIAction actionWithHandler:^(UIAction* action) {
        [weakSelf.delegate didTapURLInDisclaimer:URL];
      }];
    }
  }

  if ([super respondsToSelector:_cmd]) {
    return [super textView:textView
        primaryActionForTextItem:textItem
                   defaultAction:defaultAction];
  }

  return defaultAction;
}

#pragma mark - AgeMismatchSignoutConsumer

- (void)setPrimaryIdentityName:(NSString*)name
                         email:(NSString*)email
                        avatar:(UIImage*)avatar
                       managed:(BOOL)managed {
  [self.identityView setTitle:name ? name : email
                     subtitle:name ? email : nil
                      managed:managed];
  [self.identityView setAvatar:avatar];
}

- (void)blockUI {
  CHECK(!_waitingView);
  _waitingView = [[HomeWaitingView alloc] initWithFrame:self.view.bounds
                                        backgroundColor:UIColor.clearColor];
  _waitingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_waitingView];
  AddSameConstraints(_waitingView, self.view);
  [_waitingView startWaiting];
  self.view.userInteractionEnabled = NO;
}

- (void)setShowStaySignedOutButton:(BOOL)show {
  CHECK(!self.viewLoaded);
  _showStaySignedOutButton = show;
}

@end
