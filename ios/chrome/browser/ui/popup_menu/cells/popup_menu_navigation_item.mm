// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_navigation_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 16;
const CGFloat kImageCornerRadius = 2;
const CGFloat kFaviconBackgroundSize = 28;
const CGFloat kFaviconBackgroundCornerRadius = 7;
const CGFloat kCellHeight = 44;
const CGFloat kIconTextMargin = 11;
const CGFloat kMargin = 15;
const CGFloat kVerticalMargin = 8;
const CGFloat kMaxHeight = 100;
}  // namespace

@implementation PopupMenuNavigationItem

@synthesize actionIdentifier = _actionIdentifier;
@synthesize favicon = _favicon;
@synthesize title = _title;
@synthesize navigationItem = _navigationItem;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuNavigationCell class];
  }
  return self;
}

- (void)configureCell:(PopupMenuNavigationCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  [cell setFavicon:self.favicon];
  [cell setTitle:self.title];
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/828357): This should be done at the table view level.
  static PopupMenuNavigationCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[PopupMenuNavigationCell alloc] init];
    [cell registerForContentSizeUpdates];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

#pragma mark - PopupMenuNavigationCell

@interface PopupMenuNavigationCell ()
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UIImageView* faviconImageView;
@end

@implementation PopupMenuNavigationCell

@synthesize faviconImageView = _faviconImageView;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* selectedBackgroundView = [[UIView alloc] init];
    selectedBackgroundView.backgroundColor =
        [UIColor colorNamed:kTableViewRowHighlightColor];
    self.selectedBackgroundView = selectedBackgroundView;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [self titleFont];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    UIView* faviconBackground = [[UIView alloc] init];
    faviconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    faviconBackground.backgroundColor =
        [UIColor colorNamed:kFaviconBackgroundColor];
    faviconBackground.layer.cornerRadius = kFaviconBackgroundCornerRadius;

    _faviconImageView = [[UIImageView alloc] init];
    _faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconImageView.layer.cornerRadius = kImageCornerRadius;
    _faviconImageView.layer.masksToBounds = YES;

    [faviconBackground addSubview:_faviconImageView];

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:faviconBackground];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[favicon(faviconSize)]-(iconText)-[text]-(margin)-|",
          @"H:[image(imageSize)]", @"V:[favicon(faviconSize)]",
          @"V:[image(imageSize)]",
          @"V:|-(verticalMargin)-[text]-(verticalMargin)-|"
        ],
        @{
          @"image" : _faviconImageView,
          @"text" : _titleLabel,
          @"favicon" : faviconBackground
        },
        @{
          @"margin" : @(kMargin),
          @"imageSize" : @(kImageSize),
          @"faviconSize" : @(kFaviconBackgroundSize),
          @"iconText" : @(kIconTextMargin),
          @"verticalMargin" : @(kVerticalMargin),
        });

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;
    AddSameCenterConstraints(_faviconImageView, faviconBackground);
    AddSameCenterYConstraint(self.contentView, faviconBackground);

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
}

- (void)setFavicon:(UIImage*)favicon {
  if (favicon) {
    self.faviconImageView.image = favicon;
  } else {
    self.faviconImageView.image = [UIImage imageNamed:@"default_favicon"];
  }
}

- (void)registerForContentSizeUpdates {
  // This is needed because if the cell is static (used for height),
  // adjustsFontForContentSizeCategory isn't working.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(preferredContentSizeDidChange:)
             name:UIContentSizeCategoryDidChangeNotification
           object:nil];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self setFavicon:nil];
}

#pragma mark - Private

// Callback when the preferred Content Size change.
- (void)preferredContentSizeDidChange:(NSNotification*)notification {
  self.titleLabel.font = [self titleFont];
}

// Font to be used for the title.
- (UIFont*)titleFont {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

@end
