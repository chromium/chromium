// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message_view.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the icons.
constexpr CGFloat kSymbolsPointSize = 24.;

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      DefaultSymbolWithPointSize(@"gearshape.2", kSymbolsPointSize));
#endif
}

// Returns a tinted version of the enterprise building icon.
UIImage* GetEnterpriseIcon() {
  UIColor* color = [UIColor colorNamed:kInvertedTextSecondaryColor];
  // Actual size does not matter, the image is resized.
  return SymbolWithPalette(
      CustomSymbolWithPointSize(kEnterpriseSymbol, kSymbolsPointSize),
      @[ color ]);
}

// Returns the text for `_emailView`.
NSString* GetEmailLabelText(NSString* email, bool managed) {
  if (!managed) {
    // Not managed, just the email.
    return email;
  }
  if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone) {
    // iPhone, show the label on a separate line.
    return email;
  }
  // TODO(crbug.com/349071774): In Phase 2, display the domain name or
  // admin-provided company name/icon (when available).
  // iPad, show the label on the same line.
  return l10n_util::GetNSStringF(
      IDS_IOS_ENTERPRISE_SWITCH_TO_MANAGED_WIDE_SCREEN,
      base::SysNSStringToUTF16(email));
}

// Returns the text for `_managementView`, or nil if the view isn't needed
// at all.
NSString* GetManagementLabelText(bool managed) {
  if (!managed) {
    return nil;
  }
  if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone) {
    return l10n_util::GetNSString(
        IDS_IOS_ENTERPRISE_MANAGED_BY_YOUR_ORGANIZATION);
  }
  return nil;
}

UILabel* CreateSingleLineLabel(NSString* text,
                               UIFontTextStyle text_style,
                               NSString* color_name) {
  UILabel* label = [[UILabel alloc] init];
  label.adjustsFontForContentSizeCategory = YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:text_style];
  label.adjustsFontSizeToFitWidth = NO;
  label.textColor = [UIColor colorNamed:color_name];
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.text = text;
  label.numberOfLines = 1;
  return label;
}

const CGFloat kAvatarSize = 32.;
const CGFloat kGoogleSize = 32.;
const CGFloat kVerticalPadding = 12.;
const CGFloat kHorizontalPadding = 16.;
const CGFloat kHorizontalGap = 16.;
// The offset between texts.
const CGFloat kTextOffset = 2.;

}  // namespace

@interface MDCSnackbarMessageView (internal)

- (void)dismissWithAction:(MDCSnackbarMessageAction*)action
            userInitiated:(BOOL)userInitiated;
@end

@interface IdentitySnackbarMessageView () {
  // The view containing the avatar.
  UIImageView* _avatarView;
  // The view containing the "Signed in as name" text.
  UILabel* _signedInAsView;
  // The view that contains at least the email address. It may also contain ".
  // Managed by your organisation" on iPad when the profile is managed.
  UILabel* _emailView;
  // The view containing the "managed by your organization" message on iPhone,
  // on a separate line. Can be nil.
  UILabel* _managementView;
  // The texts view above.
  UIStackView* _textViews;
  // The view containing the google symbol.
  UIImageView* _accountBadgeView;
}

@end

@implementation IdentitySnackbarMessageView

- (instancetype)initWithMessage:(MDCSnackbarMessage*)message
                 dismissHandler:(MDCSnackbarMessageDismissHandler)handler
                snackbarManager:(MDCSnackbarManager*)manager {
  self = [super initWithMessage:message
                 dismissHandler:handler
                snackbarManager:manager];
  if (self) {
    IdentitySnackbarMessage* snackbarMessage =
        (IdentitySnackbarMessage*)message;

    // Avatar view.
    _avatarView = [[UIImageView alloc] init];
    _avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    _avatarView.image = snackbarMessage.avatar;
    _avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    _avatarView.clipsToBounds = YES;
    _avatarView.isAccessibilityElement = NO;
    _avatarView.layer.cornerRadius = kAvatarSize / 2.;
    [self addSubview:_avatarView];

    // Text views.
    NSString* name = snackbarMessage.name;
    NSString* signedInText =
        (name)
            ? l10n_util::GetNSStringF(
                  IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                  base::SysNSStringToUTF16(name))
            : l10n_util::GetNSString(IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE);
    _signedInAsView = CreateSingleLineLabel(
        signedInText, UIFontTextStyleSubheadline, kInvertedTextPrimaryColor);

    _emailView = CreateSingleLineLabel(
        GetEmailLabelText(snackbarMessage.email, snackbarMessage.managed),
        UIFontTextStyleFootnote, kInvertedTextSecondaryColor);

    NSString* managementText = GetManagementLabelText(snackbarMessage.managed);
    if (snackbarMessage.managed && !managementText) {
      // Show the management message on the same line as the email. This could
      // be fairly long, so allow the text to wrap once.
      _emailView.numberOfLines = 2;
    }

    if (managementText) {
      // Show the management message on a separate line.
      _managementView = CreateSingleLineLabel(
          managementText, UIFontTextStyleFootnote, kInvertedTextSecondaryColor);
    }

    _textViews = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _signedInAsView, _emailView ]];
    if (_managementView) {
      [_textViews addArrangedSubview:_managementView];
    }
    _textViews.axis = UILayoutConstraintAxisVertical;
    _textViews.distribution = UIStackViewDistributionEqualSpacing;
    _textViews.alignment = UIStackViewAlignmentLeading;
    _textViews.translatesAutoresizingMaskIntoConstraints = NO;

    AddSameConstraintsToSides(_emailView, _signedInAsView,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    AddSameConstraintsToSides(_emailView, _textViews,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    if (_managementView) {
      AddSameConstraintsToSides(_emailView, _managementView,
                                LayoutSides::kLeading | LayoutSides::kTrailing);
    }

    [self addSubview:_textViews];

    UIImage* accountBadge = snackbarMessage.managed
                                ? GetEnterpriseIcon()
                                : GetBrandedGoogleServicesSymbol();
    _accountBadgeView = [[UIImageView alloc] initWithImage:accountBadge];
    _accountBadgeView.contentMode = UIViewContentModeCenter;
    _accountBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    _accountBadgeView.clipsToBounds = YES;
    _accountBadgeView.isAccessibilityElement = NO;
    [self addSubview:_accountBadgeView];

    UILabel* lastLabel = _managementView ? _managementView : _emailView;
    [NSLayoutConstraint activateConstraints:@[
      // Size constraints

      [_avatarView.heightAnchor constraintEqualToConstant:kAvatarSize],
      [_avatarView.widthAnchor constraintEqualToConstant:kAvatarSize],

      [_accountBadgeView.heightAnchor constraintEqualToConstant:kGoogleSize],
      [_accountBadgeView.widthAnchor constraintEqualToConstant:kGoogleSize],
      [_emailView.heightAnchor
          constraintEqualToConstant:[_emailView intrinsicContentSize].height],
      [_signedInAsView.heightAnchor
          constraintEqualToConstant:[_signedInAsView intrinsicContentSize]
                                        .height],

      // Vertical counstraints from top to bottom.
      [_avatarView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kVerticalPadding],
      [_accountBadgeView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kVerticalPadding],
      [_textViews.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kVerticalPadding],
      [_signedInAsView.topAnchor constraintEqualToAnchor:_textViews.topAnchor],
      [_emailView.topAnchor constraintEqualToAnchor:_signedInAsView.bottomAnchor
                                           constant:kTextOffset],
      [_textViews.bottomAnchor constraintEqualToAnchor:lastLabel.bottomAnchor],
      [self.bottomAnchor constraintEqualToAnchor:_textViews.bottomAnchor
                                        constant:kVerticalPadding],
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_avatarView.bottomAnchor
                                      constant:kVerticalPadding],
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_accountBadgeView.bottomAnchor
                                      constant:kVerticalPadding],

      // Horizontal constraints from left to right.

      [_avatarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                                constant:kHorizontalPadding],
      [_textViews.leadingAnchor
          constraintEqualToAnchor:_avatarView.trailingAnchor
                         constant:kHorizontalGap],

      [_accountBadgeView.leadingAnchor
          constraintEqualToAnchor:_textViews.trailingAnchor
                         constant:kHorizontalGap],

      [self.trailingAnchor
          constraintEqualToAnchor:_accountBadgeView.trailingAnchor
                         constant:kHorizontalPadding],
    ]];
    if (_managementView) {
      [_managementView.topAnchor constraintEqualToAnchor:_emailView.bottomAnchor
                                                constant:kTextOffset]
          .active = YES;
    }

    AddSameCenterYConstraint(self, _avatarView);
    AddSameCenterYConstraint(self, _accountBadgeView);
  }
  return self;
}

#pragma mark - UIView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // While `pointInside:withEvent` is also defined in MDCSnackbarMessageView, it
  // first tries to scroll its `contentView`, and only if it can’t scroll, cloze
  // the view. We are adding views on top of `contentView` which breaks the way
  // MDCSnackbarMessageView dealt with closing on tap. So we must reimplement it
  // here.
  BOOL r = [super pointInside:point withEvent:event];
  if (r) {
    [self dismissWithAction:nil userInitiated:YES];
  }
  return r;
}

@end
