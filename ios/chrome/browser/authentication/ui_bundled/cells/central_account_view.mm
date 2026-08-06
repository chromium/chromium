// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The space between the enterprise icon and the "Your browser is managed ..."
// label.
const CGFloat kEnterpriseIconSpacing = 4.0;
// The vertical space between labels.
const CGFloat kLabelVerticalSpacing = 2.0;

// Returns a tinted version of the enterprise building icon.
UIImage* GetEnterpriseIcon() {
  UIColor* color = [UIColor colorNamed:kTextSecondaryColor];
  return SymbolWithPalette(
      SymbolWithConfiguration(
          SymbolEnterprise,
          [UIImageSymbolConfiguration
              configurationWithFont:
                  [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]]),
      @[ color ]);
}

}  // namespace

@implementation CentralAccountView {
  // Rounded avatarImage used for the account user picture. Note: the image
  // doesn't need to be rounded as the cell configs create the image rounded
  // corners.
  UIImage* _avatarImage;
  // Name displayed in main label.
  NSString* _name;
  // Email subtitle displayed in secondary label.
  NSString* _email;
  // The avatar view.
  AITierAvatarView* _avatarView;
  // Whether to use large margin.
  BOOL _useLargeMargins;
  // The constraint for the top padding.
  NSLayoutConstraint* _topPaddingConstraint;
  // Management label.
  UILabel* _managementLabel;
  // The full name of the AI tier.
  NSString* _aiTierFullName;
}

- (instancetype)initWithFrame:(CGRect)frame
                  avatarImage:(UIImage*)avatarImage
              showsAITierRing:(BOOL)showsAITierRing
               aiTierFullName:(NSString*)aiTierFullName
                         name:(NSString*)name
                        email:(NSString*)email
        managementDescription:(NSString*)managementDescription
              useLargeMargins:(BOOL)useLargeMargins {
  self = [super initWithFrame:frame];
  if (self) {
    CHECK(avatarImage);
    CHECK(email);
    _avatarImage = avatarImage;
    _aiTierFullName = [aiTierFullName copy];
    _name = name ? name : email;
    _email = name ? email : nil;
    _useLargeMargins = useLargeMargins;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitHeader;
    self.accessibilityIdentifier =
        CentralAccountViewAccessibilityIdentifier(email);

    CGFloat outerSize =
        GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large).width;
    _avatarView =
        [[AITierAvatarView alloc] initWithAvatarImage:_avatarImage
                                            outerSize:outerSize
                                      showsAITierRing:showsAITierRing];
    [self addSubview:_avatarView];

    UILabel* nameLabel = [[UILabel alloc] init];
    nameLabel.text = _name;
    nameLabel.textAlignment = NSTextAlignmentCenter;
    nameLabel.numberOfLines = 1;
    nameLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    nameLabel.adjustsFontForContentSizeCategory = YES;
    nameLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    nameLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:nameLabel];

    UILabel* emailLabel = [[UILabel alloc] init];
    emailLabel.text = _email;
    emailLabel.textAlignment = NSTextAlignmentCenter;
    emailLabel.numberOfLines = 1;
    emailLabel.adjustsFontForContentSizeCategory = YES;
    emailLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    emailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    emailLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    emailLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:emailLabel];
    CGFloat bottomMargin =
        _useLargeMargins
            ? (2 * kTableViewLargeVerticalSpacing)
            : (kTableViewLargeVerticalSpacing + kTableViewVerticalSpacing);

    if (managementDescription) {
      CHECK_GT(managementDescription.length, 0u, base::NotFatalUntil::M140);
      UIImage* managementIcon = GetEnterpriseIcon();
      UIImageView* managementIconView =
          [[UIImageView alloc] initWithImage:managementIcon];
      managementIconView.translatesAutoresizingMaskIntoConstraints = NO;
      [managementIconView
          setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];
      [managementIconView
          setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisVertical];
      [managementIconView
          setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      [managementIconView
          setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisVertical];
      [self addSubview:managementIconView];

      _managementLabel = [[UILabel alloc] init];
      // TODO(crbug.com/349071774): In Phase 2, display the admin-provided
      // company icon (when available).
      _managementLabel.text = managementDescription;
      _managementLabel.textAlignment = NSTextAlignmentNatural;
      _managementLabel.numberOfLines = 1;
      _managementLabel.adjustsFontForContentSizeCategory = YES;
      _managementLabel.lineBreakMode = NSLineBreakByTruncatingTail;
      _managementLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
      _managementLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
      _managementLabel.translatesAutoresizingMaskIntoConstraints = NO;

      UIStackView* horizontalStack = [[UIStackView alloc]
          initWithArrangedSubviews:@[ managementIconView, _managementLabel ]];
      horizontalStack.axis = UILayoutConstraintAxisHorizontal;
      horizontalStack.distribution = UIStackViewDistributionEqualSpacing;
      horizontalStack.alignment = UIStackViewAlignmentCenter;
      horizontalStack.spacing = kEnterpriseIconSpacing;
      horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
      [self addSubview:horizontalStack];

      [NSLayoutConstraint activateConstraints:@[
        [horizontalStack.topAnchor
            constraintEqualToAnchor:emailLabel.bottomAnchor
                           constant:kLabelVerticalSpacing],
        [horizontalStack.centerXAnchor
            constraintEqualToAnchor:self.centerXAnchor],
        [horizontalStack.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                        constant:kTableViewHorizontalSpacing],
        [horizontalStack.trailingAnchor
            constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                     constant:-kTableViewHorizontalSpacing],
        [self.bottomAnchor constraintEqualToAnchor:horizontalStack.bottomAnchor
                                          constant:bottomMargin],
      ]];

    } else {
      [self.bottomAnchor constraintEqualToAnchor:emailLabel.bottomAnchor
                                        constant:bottomMargin]
          .active = YES;
    }
    _topPaddingConstraint = [_avatarView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:(_useLargeMargins
                                     ? kTableViewLargeVerticalSpacing
                                     : kTopLargePadding)];
    [NSLayoutConstraint activateConstraints:@[
      [_avatarView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      _topPaddingConstraint,
      [_avatarView.widthAnchor
          constraintEqualToConstant:GetSizeForIdentityAvatarSize(
                                        IdentityAvatarSize::Large)
                                        .width],
      [_avatarView.heightAnchor
          constraintEqualToAnchor:_avatarView.widthAnchor],

      [nameLabel.topAnchor constraintEqualToAnchor:_avatarView.bottomAnchor
                                          constant:kTableViewVerticalSpacing],
      [nameLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [nameLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      [emailLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor
                                           constant:kLabelVerticalSpacing],
      [emailLabel.leadingAnchor
          constraintEqualToAnchor:nameLabel.leadingAnchor],
      [emailLabel.trailingAnchor
          constraintEqualToAnchor:nameLabel.trailingAnchor],
    ]];
    [self updateFrame];
  }
  return self;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  NSString* aiTierString = nil;
  if (_aiTierFullName.length > 0) {
    aiTierString = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
        base::SysNSStringToUTF16(_aiTierFullName));
  }

  if (_email != nil) {
    // Both name and email are present.
    if ([self managed]) {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_MANAGED_STATUS_AI_TIER,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16([self managementDescription]),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_MANAGED_STATUS,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16([self managementDescription]));
      }
    } else {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_AI_TIER,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email));
      }
    }
  } else {
    // Only email is present (stored in _name).
    if ([self managed]) {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MANAGED_STATUS_AI_TIER,
            base::SysNSStringToUTF16(_name),
            base::SysNSStringToUTF16([self managementDescription]),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MANAGED_STATUS,
            base::SysNSStringToUTF16(_name),
            base::SysNSStringToUTF16([self managementDescription]));
      }
    } else {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_AI_TIER,
            base::SysNSStringToUTF16(_name),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return _name;
      }
    }
  }
}

// Updates the frame size.
- (void)updateFrame {
  CGSize size =
      [self systemLayoutSizeFittingSize:self.frame.size
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  CGRect newFrame = CGRectZero;
  newFrame.size = size;
  self.frame = newFrame;
}

- (UIImage*)avatarImage {
  return _avatarImage;
}

- (UIView*)avatarView {
  return _avatarView;
}

- (NSString*)name {
  return _name;
}

- (NSString*)email {
  return _email;
}

- (NSString*)aiTierFullName {
  return _aiTierFullName;
}

- (BOOL)managed {
  return _managementLabel != nil;
}

- (NSString*)managementDescription {
  return _managementLabel.text;
}

- (void)updateTopPadding:(CGFloat)existingPadding {
  CGFloat topPadding =
      (_useLargeMargins ? kTableViewLargeVerticalSpacing : kTopLargePadding);
  _topPaddingConstraint.constant = topPadding - existingPadding;
  [self updateFrame];
}

@end
