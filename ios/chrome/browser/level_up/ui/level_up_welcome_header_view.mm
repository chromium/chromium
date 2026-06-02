// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_welcome_header_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing for layout margins and stacks.
const CGFloat kLayoutSpacing = 16.0;
// The size of the user avatar.
const CGFloat kAvatarSize = 56.0;
// The corner radius of the user avatar image view.
const CGFloat kAvatarCornerRadius = 28.0;
// Spacing within the welcome text container.
const CGFloat kWelcomeTextSpacing = 4.0;
// The line height multiple for the welcome title.
const CGFloat kWelcomeTextLineHeightMultiple = 1.2;
// The line height multiple for the user name.
const CGFloat kUserNameLineHeightMultiple = 1.08;

}  // namespace

@implementation LevelUpWelcomeHeaderView {
  // Image view containing user's sign-in avatar.
  UIImageView* _userAvatarImageView;
  // Label showing user's full name.
  UILabel* _userNameLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _userAvatarImageView = [[UIImageView alloc] init];
    _userAvatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _userAvatarImageView.contentMode = UIViewContentModeScaleAspectFill;
    _userAvatarImageView.layer.cornerRadius = kAvatarCornerRadius;
    _userAvatarImageView.layer.masksToBounds = YES;
    _userAvatarImageView.backgroundColor =
        [UIColor colorNamed:kTextQuaternaryColor];
    [NSLayoutConstraint activateConstraints:@[
      [_userAvatarImageView.widthAnchor constraintEqualToConstant:kAvatarSize],
      [_userAvatarImageView.heightAnchor constraintEqualToConstant:kAvatarSize],
    ]];

    UILabel* welcomeLabel = [[UILabel alloc] init];
    welcomeLabel.translatesAutoresizingMaskIntoConstraints = NO;

    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineHeightMultiple = kWelcomeTextLineHeightMultiple;

    NSDictionary<NSAttributedStringKey, id>* welcomeTextAttributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
      NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
      NSParagraphStyleAttributeName : paragraphStyle
    };

    welcomeLabel.attributedText = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_WELCOME)
            attributes:welcomeTextAttributes];

    _userNameLabel = [[UILabel alloc] init];
    _userNameLabel.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* textStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ welcomeLabel, _userNameLabel ]];
    textStack.translatesAutoresizingMaskIntoConstraints = NO;
    textStack.axis = UILayoutConstraintAxisVertical;
    textStack.spacing = kWelcomeTextSpacing;

    UIStackView* headerStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _userAvatarImageView, textStack ]];
    headerStack.translatesAutoresizingMaskIntoConstraints = NO;
    headerStack.axis = UILayoutConstraintAxisHorizontal;
    headerStack.spacing = kLayoutSpacing;
    headerStack.alignment = UIStackViewAlignmentCenter;

    [self.contentView addSubview:headerStack];
    AddSameConstraints(headerStack, self.contentView);

    [headerStack.heightAnchor constraintEqualToConstant:kAvatarSize].active =
        YES;
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _userAvatarImageView.image = nil;
  _userNameLabel.attributedText = nil;
}

- (void)setUserAvatar:(UIImage*)userAvatar {
  _userAvatar = userAvatar;
  _userAvatarImageView.image = userAvatar;
}

- (void)setUserFullName:(NSString*)userFullName {
  _userFullName = userFullName;
  _userNameLabel.attributedText =
      [self attributedUserNameStringWithName:userFullName];
}

#pragma mark - Private
// Returns the attributed user name string.
- (NSAttributedString*)attributedUserNameStringWithName:(NSString*)name {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = kUserNameLineHeightMultiple;

  UIFontDescriptor* bodyDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIFontDescriptor* boldDescriptor = [bodyDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  UIFont* userNameFont = [UIFont fontWithDescriptor:boldDescriptor size:0.0];

  NSDictionary<NSAttributedStringKey, id>* userNameTextAttributes = @{
    NSFontAttributeName : userNameFont,
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  return [[NSAttributedString alloc] initWithString:name ?: @""
                                         attributes:userNameTextAttributes];
}

@end
