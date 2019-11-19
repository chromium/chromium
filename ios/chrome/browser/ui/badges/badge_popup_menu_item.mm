// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_popup_menu_item.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the ContentView of the cell.
const CGFloat kCellHeight = 44;
// Horizontal spacing between subviews.
const CGFloat kMargin = 15;
// Maximum of the cell.
const CGFloat kMaxHeight = 100;
// Vertical spacing between subviews.
const CGFloat kVerticalMargin = 8;
// Corner radius of badge background.
const CGFloat kBadgeCornerRadius = 5.0;
}  // namespace

@interface BadgePopupMenuItem ()

// The title of the item.
@property(nonatomic, copy, readonly) NSString* title;

// The BadgeType of the item.
@property(nonatomic, assign, readonly) BadgeType badgeType;

@end

@implementation BadgePopupMenuItem
// Synthesized from BadgeItem.
@synthesize actionIdentifier = _actionIdentifier;

- (instancetype)initWithBadgeType:(BadgeType)badgeType {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    self.cellClass = [BadgePopupMenuCell class];
    _badgeType = badgeType;
    switch (badgeType) {
      case BadgeType::kBadgeTypePasswordSave:
        _actionIdentifier = PopupMenuActionShowSavePasswordOptions;
        _title = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_TITLE);
        break;
      case BadgeType::kBadgeTypePasswordUpdate:
        _actionIdentifier = PopupMenuActionShowUpdatePasswordOptions;
        _title = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD_TITLE);
        break;
      case BadgeType::kBadgeTypeSaveCard:
        _actionIdentifier = PopupMenuActionShowSaveCardOptions;
        _title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
        break;
      case BadgeType::kBadgeTypeTranslate:
        _actionIdentifier = PopupMenuActionShowTranslateOptions;
        // TODO(crbug.com/1014959): use l10n to translate string.
        _title = @"Translate Page";
        break;
      case BadgeType::kBadgeTypeIncognito:
        NOTREACHED() << "A BadgePopupMenuItem should not be an Incognito badge";
        break;
      case BadgeType::kBadgeTypeOverflow:
        NOTREACHED() << "A BadgePopupMenuItem should not be an overflow badge";
        break;
      case BadgeType::kBadgeTypeNone:
        NOTREACHED() << "A badge should not have kBadgeTypeNone";
        break;
    }
  }
  return self;
}

- (void)configureCell:(BadgePopupMenuCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.titleLabel.text = self.title;
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  UIImage* badgeImage;
  switch (self.badgeType) {
    case BadgeType::kBadgeTypePasswordSave:
    case BadgeType::kBadgeTypePasswordUpdate:
      badgeImage = [[UIImage imageNamed:@"infobar_passwords_icon"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case BadgeType::kBadgeTypeSaveCard:
      badgeImage = [[UIImage imageNamed:@"infobar_save_card_icon"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case BadgeType::kBadgeTypeTranslate:
      badgeImage = [[UIImage imageNamed:@"infobar_translate_icon"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case BadgeType::kBadgeTypeIncognito:
      NOTREACHED()
          << "A popup menu item should not be of type kBadgeTypeIncognito";
      break;
    case BadgeType::kBadgeTypeOverflow:
      NOTREACHED()
          << "A popup menu item should not be of type kBadgeTypeOverflow";
      break;
    case BadgeType::kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
  }
  [cell setBadgeImage:badgeImage];
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/828357): This should be done at the table view level.
  static BadgePopupMenuCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[BadgePopupMenuCell alloc] init];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

@interface BadgePopupMenuCell ()

// The font to be used for the title.
@property(nonatomic, strong, readonly) UIFont* titleFont;

// Image view to display the badge.
@property(nonatomic, strong, readonly) UIImageView* badgeView;

// Image view displayed in the trailing corner of the cell.
@property(nonatomic, strong, readonly) UIImageView* trailingImageView;

@end

@implementation BadgePopupMenuCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _titleFont = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.numberOfLines = 0;
    _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _titleLabel.font = _titleFont;
    _titleLabel.textColor = [UIColor colorNamed:kBlueColor];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _badgeView = [[UIImageView alloc] init];
    _badgeView.translatesAutoresizingMaskIntoConstraints = NO;
    _badgeView.tintColor = [UIColor colorNamed:kBlueColor];
    _badgeView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
    _badgeView.layer.cornerRadius = kBadgeCornerRadius;

    _trailingImageView = [[UIImageView alloc]
        initWithImage:
            [[UIImage imageNamed:@"infobar_settings_icon"]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]];
    _trailingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingImageView.tintColor = [UIColor colorNamed:kBlueColor];

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_badgeView];
    [self.contentView addSubview:_trailingImageView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[badge]-(margin)-[text]-(margin)-[gear]-(margin)-|",
          @"V:|-(verticalMargin)-[text]-(verticalMargin)-|",
          @"V:|-(verticalMargin)-[badge]-(verticalMargin)-|",
          @"V:|-(verticalMargin)-[gear]-(verticalMargin)-|",
        ],
        @{
          @"text" : _titleLabel,
          @"badge" : _badgeView,
          @"gear" : _trailingImageView,
        },
        @{
          @"margin" : @(kMargin),
          @"verticalMargin" : @(kVerticalMargin),
        });

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setBadgeImage:(UIImage*)badgeImage {
  [self.badgeView setImage:badgeImage];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = @"";
  [self.badgeView setImage:nil];
  [self.trailingImageView setImage:nil];
}

@end
