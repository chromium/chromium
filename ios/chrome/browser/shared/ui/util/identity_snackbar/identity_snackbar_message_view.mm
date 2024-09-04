// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message_view.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, 24.));
#else
  return MakeSymbolMulticolor(DefaultSymbolWithPointSize(@"gearshape.2", 24.));
#endif
}

const CGFloat kAvatarSize = 32.;
const CGFloat kGoogleSize = 32.;
const CGFloat kVerticalPadding = 12.;
const CGFloat kHorizontalPadding = 16.;
const CGFloat kHorizontalGap = 16.;
// The offset between both texts.
const CGFloat kTextOffset = 6.;

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
  // The view containing the email address.
  UILabel* _emailView;
  // The texts view above.
  UIStackView* _textViews;
  // The view containing the google symbol
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

    _signedInAsView = [[UILabel alloc] init];
    _signedInAsView.adjustsFontForContentSizeCategory = YES;
    _signedInAsView.translatesAutoresizingMaskIntoConstraints = NO;
    _signedInAsView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _signedInAsView.adjustsFontSizeToFitWidth = NO;
    _signedInAsView.textColor = [UIColor colorNamed:kInvertedTextPrimaryColor];
    _signedInAsView.lineBreakMode = NSLineBreakByTruncatingTail;
    _signedInAsView.text =
        l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                                base::SysNSStringToUTF16(snackbarMessage.name));
    _signedInAsView.numberOfLines = 1;

    _emailView = [[UILabel alloc] init];
    _emailView.adjustsFontForContentSizeCategory = YES;
    _emailView.translatesAutoresizingMaskIntoConstraints = NO;
    _emailView.numberOfLines = 1;
    _emailView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _emailView.adjustsFontSizeToFitWidth = NO;
    _emailView.lineBreakMode = NSLineBreakByTruncatingTail;
    _emailView.textColor = [UIColor colorNamed:kInvertedTextSecondaryColor];
    _emailView.text = snackbarMessage.email;

    _textViews = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _signedInAsView, _emailView ]];
    _textViews.axis = UILayoutConstraintAxisVertical;
    _textViews.distribution = UIStackViewDistributionEqualSpacing;
    _textViews.alignment = UIStackViewAlignmentLeading;
    _textViews.translatesAutoresizingMaskIntoConstraints = NO;

    AddSameConstraintsToSides(_emailView, _signedInAsView,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    AddSameConstraintsToSides(_emailView, _textViews,
                              LayoutSides::kLeading | LayoutSides::kTrailing);

    [self addSubview:_textViews];

    _accountBadgeView =
        [[UIImageView alloc] initWithImage:GetBrandedGoogleServicesSymbol()];
    _accountBadgeView.contentMode = UIViewContentModeCenter;
    _accountBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    _accountBadgeView.clipsToBounds = YES;
    _accountBadgeView.isAccessibilityElement = NO;
    [self addSubview:_accountBadgeView];

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
      [_textViews.bottomAnchor constraintEqualToAnchor:_emailView.bottomAnchor],
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
