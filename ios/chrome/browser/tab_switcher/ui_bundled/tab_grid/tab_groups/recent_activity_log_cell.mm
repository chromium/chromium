// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The size for the default avatar symbol.
constexpr CGFloat kDefaultAvatarSize = 20;
// Alpha of the no-avatar background.
constexpr CGFloat kDefaultAvatarAlpha = 0.2;
}  // namespace

@implementation RecentActivityLogCell {
  // Container view that displays the `faviconView`.
  FaviconContainerView* _faviconContainerView;
  // The container for the avatar.
  UIView* _avatarContainer;
  // The avatar.
  UIView* _avatar;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    _avatarContainer = [[UIView alloc] init];
    _uniqueIdentifier = [[NSUUID UUID] UUIDString];

    _faviconContainerView = [[FaviconContainerView alloc] init];
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Set font size using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.numberOfLines = 2;
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _descriptionLabel = [[UILabel alloc] init];
    _descriptionLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _descriptionLabel.adjustsFontForContentSizeCategory = YES;
    _descriptionLabel.numberOfLines = 1;
    _descriptionLabel.lineBreakMode = NSLineBreakByTruncatingTail;

    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _descriptionLabel ]];
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = 0;
    verticalStack.distribution = UIStackViewDistributionFill;
    verticalStack.alignment = UIStackViewAlignmentLeading;
    [self.contentView addSubview:verticalStack];

    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _avatarContainer, verticalStack, _faviconContainerView
        ]];
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    [self.contentView addSubview:horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;
    [NSLayoutConstraint activateConstraints:@[
      [_avatarContainer.heightAnchor
          constraintEqualToConstant:kRecentActivityLogAvatarSize],
      [_avatarContainer.widthAnchor
          constraintEqualToAnchor:_avatarContainer.heightAnchor],
      // Horizontal Stack constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kTableViewVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kTableViewVerticalSpacing],
      heightConstraint,
    ]];
  }
  return self;
}

#pragma mark - Accessors

- (FaviconView*)faviconView {
  return _faviconContainerView.faviconView;
}

- (void)setAvatar:(UIView*)avatar {
  [_avatar removeFromSuperview];
  if (!avatar) {
    UIImageView* defaultAvatarImage = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kPersonFillSymbol,
                                                 kDefaultAvatarSize)];
    defaultAvatarImage.translatesAutoresizingMaskIntoConstraints = NO;
    defaultAvatarImage.tintColor = [UIColor colorNamed:kSolidWhiteColor];

    avatar = [[UIView alloc] init];
    avatar.translatesAutoresizingMaskIntoConstraints = NO;
    avatar.backgroundColor = [[UIColor colorNamed:kTextPrimaryColor]
        colorWithAlphaComponent:kDefaultAvatarAlpha];
    avatar.layer.cornerRadius = kRecentActivityLogAvatarSize / 2;
    [NSLayoutConstraint activateConstraints:@[
      [avatar.heightAnchor
          constraintEqualToConstant:kRecentActivityLogAvatarSize],
      [avatar.widthAnchor constraintEqualToAnchor:avatar.heightAnchor],
    ]];

    [avatar addSubview:defaultAvatarImage];
    AddSameCenterConstraints(avatar, defaultAvatarImage);
  }
  _avatar = avatar;

  [_avatarContainer addSubview:avatar];
  AddSameCenterConstraints(_avatar, _avatarContainer);
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _uniqueIdentifier = [[NSUUID UUID] UUIDString];
  _titleLabel.text = nil;
  _descriptionLabel.text = nil;
  _faviconContainerView = nil;
  [self setAvatar:nil];
}

@end
