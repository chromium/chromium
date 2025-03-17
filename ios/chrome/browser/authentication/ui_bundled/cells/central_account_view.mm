// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The space between the enterprise icon and the "Your browser is managed ..."
// label.
const CGFloat kEnterpriseIconSpacing = 4.0;
// The vertical space between labels.
const CGFloat kLabelVerticalSpacing = 2.0;
// The button padding.
const CGFloat kButtonPadding = 8.0;

// Returns a tinted version of the enterprise building icon.
UIImage* GetEnterpriseIcon() {
  UIColor* color = [UIColor colorNamed:kTextSecondaryColor];
  return SymbolWithPalette(
      CustomSymbolWithConfiguration(
          kEnterpriseSymbol,
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
  // The account avatar.
  UIImageView* _imageView;
  // Whether to use large margin.
  BOOL _useLargeMargins;
  // The constraint for the top padding.
  NSLayoutConstraint* _topPaddingConstraint;
  // Whether to add manage your account button.
  BOOL _addManageYourAccountButton;
  // Manage your account button.
  UIButton* _button;
  // Manage your account button action.
  ProceduralBlock _manageYourAccountButtonAction;
  // Management label.
  UILabel* _managementLabel;
}

- (instancetype)initWithFrame:(CGRect)frame
                      avatarImage:(UIImage*)avatarImage
                             name:(NSString*)name
                            email:(NSString*)email
            managementDescription:(NSString*)managementDescription
                  useLargeMargins:(BOOL)useLargeMargins
       addManageYourAccountButton:(BOOL)addManageYourAccountButton
    manageYourAccountButtonAction:
        (ProceduralBlock)manageYourAccountButtonAction {
  self = [super initWithFrame:frame];
  if (self) {
    CHECK(avatarImage);
    CHECK(email);
    _avatarImage = avatarImage;
    _name = name ? name : email;
    _email = name ? email : nil;
    _useLargeMargins = useLargeMargins;
    _addManageYourAccountButton = addManageYourAccountButton;
    _manageYourAccountButtonAction = manageYourAccountButtonAction;
    CHECK_EQ(_addManageYourAccountButton,
             _manageYourAccountButtonAction != nil);
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitHeader;

    _imageView = [[UIImageView alloc] initWithImage:_avatarImage];
    // Creates the image rounded corners.
    _imageView.layer.cornerRadius =
        GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large).width / 2.0f;
    _imageView.clipsToBounds = YES;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_imageView];

    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.text = _name;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.numberOfLines = 1;
    titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:titleLabel];

    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = _email;
    subtitleLabel.textAlignment = NSTextAlignmentCenter;
    subtitleLabel.numberOfLines = 1;
    subtitleLabel.adjustsFontForContentSizeCategory = YES;
    subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:subtitleLabel];
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
      ]];

      if (_addManageYourAccountButton) {
        [self addButton];
        [_button.topAnchor constraintEqualToAnchor:horizontalStack.bottomAnchor
                                          constant:3 * kLabelVerticalSpacing]
            .active = YES;
        [self.bottomAnchor constraintEqualToAnchor:_button.bottomAnchor
                                          constant:bottomMargin]
            .active = YES;
      } else {
        [self.bottomAnchor constraintEqualToAnchor:horizontalStack.bottomAnchor
                                          constant:bottomMargin]
            .active = YES;
      }
    } else if (_addManageYourAccountButton) {
      [self addButton];
      [_button.topAnchor constraintEqualToAnchor:subtitleLabel.bottomAnchor
                                        constant:3 * kLabelVerticalSpacing]
          .active = YES;
      [self.bottomAnchor constraintEqualToAnchor:_button.bottomAnchor
                                        constant:bottomMargin]
          .active = YES;
    } else {
      [self.bottomAnchor constraintEqualToAnchor:subtitleLabel.bottomAnchor
                                        constant:bottomMargin]
          .active = YES;
    }
    _topPaddingConstraint = [_imageView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:(_useLargeMargins
                                     ? kTableViewLargeVerticalSpacing
                                     : kTopLargePadding)];
    [NSLayoutConstraint activateConstraints:@[
      [_imageView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      _topPaddingConstraint,
      [_imageView.widthAnchor
          constraintEqualToConstant:GetSizeForIdentityAvatarSize(
                                        IdentityAvatarSize::Large)
                                        .width],
      [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],

      [titleLabel.topAnchor constraintEqualToAnchor:_imageView.bottomAnchor
                                           constant:kTableViewVerticalSpacing],
      [titleLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [titleLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      [subtitleLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                              constant:kLabelVerticalSpacing],
      [subtitleLabel.leadingAnchor
          constraintEqualToAnchor:titleLabel.leadingAnchor],
      [subtitleLabel.trailingAnchor
          constraintEqualToAnchor:titleLabel.trailingAnchor],
    ]];
    [self updateFrame];
  }
  return self;
}

- (NSString*)accessibilityLabel {
  NSMutableString* accessibilityLabel =
      [NSMutableString stringWithString:_name];
  if (_email) {
    [accessibilityLabel appendFormat:@", %@", _email];
  }
  if ([self managed]) {
    [accessibilityLabel appendFormat:@". %@", [self managementDescription]];
  }
  return accessibilityLabel;
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

- (NSString*)name {
  return _name;
}

- (NSString*)email {
  return _email;
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

#pragma mark Private

// Executes `_manageYourAccountButtonAction` action when Manage your Account
// button is tapped.
- (void)buttonTapped:(id)sender {
  CHECK(_addManageYourAccountButton);
  if (_manageYourAccountButtonAction) {
    _manageYourAccountButtonAction();
  }
}

// Adds Manage your Account button.
- (void)addButton {
  _button = [UIButton buttonWithType:UIButtonTypeSystem];

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonPadding, 2 * kButtonPadding, kButtonPadding, 2 * kButtonPadding);
  configuration.baseForegroundColor = [UIColor colorNamed:kBlueColor];

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
      initWithString:
          l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM)];
  [string addAttributes:attributes range:NSMakeRange(0, string.length)];
  configuration.attributedTitle = string;

  _button.configuration = configuration;
  _button.translatesAutoresizingMaskIntoConstraints = NO;

  [_button addTarget:self
                action:@selector(buttonTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  [self addSubview:_button];

  [NSLayoutConstraint activateConstraints:@[
    [_button.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_button.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:kTableViewHorizontalSpacing],
    [_button.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:kTableViewHorizontalSpacing],
  ]];
}

@end
