// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/cells_swift.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The space between the enterprise icon and the "Your browser is managed ..."
// label.
const CGFloat kEnterpriseIconSpacing = 4.0;
// The vertical space between labels.
const CGFloat kLabelVerticalSpacing = 2.0;
// The vertical space between the avatar and the subscription chip.
const CGFloat kSubscriptionChipTopSpacing = 4.0;

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
              displayedAiTier:(NSInteger)displayedAiTier
                         name:(NSString*)name
                        email:(NSString*)email
        managementDescription:(NSString*)managementDescription
              useLargeMargins:(BOOL)useLargeMargins {
  self = [super initWithFrame:frame];
  if (self) {
    CHECK(avatarImage);
    CHECK(email);
    _avatarImage = avatarImage;
    BOOL showAITierViews = displayedAiTier > 0;
    _aiTierFullName =
        showAITierViews
            ? [ios::provider::GetAITierFullName(displayedAiTier) copy]
            : nil;
    _name = name;
    _email = email;
    _useLargeMargins = useLargeMargins;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitHeader;
    self.accessibilityIdentifier =
        CentralAccountViewAccessibilityIdentifier(email);

    CGFloat avatarDiameter =
        GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large).width;
    _avatarView =
        [[AITierAvatarView alloc] initWithAvatarImage:_avatarImage
                                       avatarDiameter:avatarDiameter
                                      showsAITierRing:showAITierViews];
    [self addSubview:_avatarView];

    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.text = self.title;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.numberOfLines = 1;
    titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.accessibilityIdentifier =
        kCentralAccountViewTitleAccessibilityIdentifier;
    [self addSubview:titleLabel];

    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = self.subtitle;
    subtitleLabel.textAlignment = NSTextAlignmentCenter;
    subtitleLabel.numberOfLines = 1;
    subtitleLabel.adjustsFontForContentSizeCategory = YES;
    subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    subtitleLabel.accessibilityIdentifier =
        kCentralAccountViewSubtitleAccessibilityIdentifier;
    [self addSubview:subtitleLabel];
    CGFloat bottomMargin =
        _useLargeMargins
            ? (2 * kTableViewLargeVerticalSpacing)
            : (kTableViewLargeVerticalSpacing + kTableViewVerticalSpacing);

    if (managementDescription) {
      CHECK_GT(managementDescription.length, 0u);
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
      _managementLabel.accessibilityIdentifier =
          kCentralAccountViewManagementDescriptionAccessibilityIdentifier;

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
            constraintEqualToAnchor:subtitleLabel.bottomAnchor
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
      [self.bottomAnchor constraintEqualToAnchor:subtitleLabel.bottomAnchor
                                        constant:bottomMargin]
          .active = YES;
    }
    _topPaddingConstraint = [_avatarView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:(_useLargeMargins
                                     ? kTableViewLargeVerticalSpacing
                                     : kTopLargePadding)];
    AddSameConstraintsToSidesWithInsets(
        titleLabel, self, LayoutSides::kHorizontal,
        NSDirectionalEdgeInsets{0, kTableViewHorizontalSpacing, 0,
                                kTableViewHorizontalSpacing});
    AddSameConstraintsToSides(subtitleLabel, titleLabel,
                              LayoutSides::kHorizontal);
    [NSLayoutConstraint activateConstraints:@[
      [_avatarView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      _topPaddingConstraint,
      [subtitleLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                              constant:kLabelVerticalSpacing],
    ]];

    NSString* aiTierName =
        showAITierViews ? ios::provider::GetAITierName(displayedAiTier) : nil;
    if (aiTierName.length > 0) {
      UIView* subscriptionChipView =
          [[AISubscriptionChipWrapperView alloc] initWithText:aiTierName];
      // We track whether user interacts with this chip as if it were a button.
      // So we disable any accessibility features it may have on its own.
      UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(subscriptionChipTapped:)];
      [subscriptionChipView addGestureRecognizer:tapRecognizer];
      subscriptionChipView.userInteractionEnabled = YES;
      subscriptionChipView.isAccessibilityElement = NO;
      subscriptionChipView.accessibilityElementsHidden = YES;

      [self addSubview:subscriptionChipView];
      subscriptionChipView.translatesAutoresizingMaskIntoConstraints = NO;

      [NSLayoutConstraint activateConstraints:@[
        [subscriptionChipView.topAnchor
            constraintEqualToAnchor:_avatarView.bottomAnchor
                           constant:kSubscriptionChipTopSpacing],
        [subscriptionChipView.centerXAnchor
            constraintEqualToAnchor:self.centerXAnchor],
        [titleLabel.topAnchor
            constraintEqualToAnchor:subscriptionChipView.bottomAnchor
                           constant:kTableViewVerticalSpacing],
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [titleLabel.topAnchor
            constraintEqualToAnchor:_avatarView.bottomAnchor
                           constant:kTableViewVerticalSpacing],
      ]];
    }

    [self updateFrame];
  }
  return self;
}

- (void)updateTopPadding:(CGFloat)existingPadding {
  CGFloat topPadding =
      (_useLargeMargins ? kTableViewLargeVerticalSpacing : kTopLargePadding);
  _topPaddingConstraint.constant = topPadding - existingPadding;
  [self updateFrame];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  NSString* aiTierString = nil;
  if (_aiTierFullName.length > 0) {
    aiTierString = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
        base::SysNSStringToUTF16(_aiTierFullName));
  }

  if (_name != nil) {
    // Both name and email are present.
    if (_managementLabel) {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_MANAGED_STATUS_AI_TIER,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(_managementLabel.text),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_MANAGED_STATUS,
            base::SysNSStringToUTF16(_name), base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(_managementLabel.text));
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
    // Only email is present.
    if (_managementLabel) {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MANAGED_STATUS_AI_TIER,
            base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(_managementLabel.text),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MANAGED_STATUS,
            base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(_managementLabel.text));
      }
    } else {
      if (aiTierString) {
        return l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_AI_TIER,
            base::SysNSStringToUTF16(_email),
            base::SysNSStringToUTF16(aiTierString));
      } else {
        return _email;
      }
    }
  }
}

#pragma mark - Private

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

- (NSString*)title {
  if (_name) {
    return _name;
  }
  return _email;
}

- (NSString*)subtitle {
  if (_name) {
    return _email;
  }
  return nil;
}

- (void)subscriptionChipTapped:(UITapGestureRecognizer*)sender {
  [self.delegate centralAccountViewDidTapAISubscriptionChip:self];
}

@end
