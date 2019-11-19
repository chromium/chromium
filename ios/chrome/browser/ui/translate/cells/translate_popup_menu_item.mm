// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/cells/translate_popup_menu_item.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCellHeight = 44;
const CGFloat kCheckmarkIconSize = 20;
const CGFloat kMargin = 15;
const CGFloat kMaxHeight = 100;
const CGFloat kVerticalMargin = 8;
}  // namespace

@implementation TranslatePopupMenuItem

@synthesize actionIdentifier = _actionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TranslatePopupMenuCell class];
  }
  return self;
}

- (void)configureCell:(TranslatePopupMenuCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  [cell setCheckmark:self.selected];
  [cell setTitle:self.title];
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/828357): This should be done at the table view level.
  static TranslatePopupMenuCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[TranslatePopupMenuCell alloc] init];
    [cell registerForContentSizeUpdates];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

#pragma mark - TranslatePopupMenuCell

@interface TranslatePopupMenuCell ()

@property(nonatomic, copy) UILabel* titleLabel;

// Use a custom checkmark icon instead of the UIKit provided accessory because
// it makes reserving space for the checkmark as well as multiline label width
// calculating more straightforward.
@property(nonatomic, copy) UIImageView* checkmarkView;

@end

@implementation TranslatePopupMenuCell

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
    _titleLabel.numberOfLines = 0;
    _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _titleLabel.font = [self titleFont];
    _titleLabel.textColor = [UIColor colorNamed:kBlueColor];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    [self.contentView addSubview:_titleLabel];

    UIImage* checkmarkIcon = [[UIImage imageNamed:@"checkmark"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _checkmarkView = [[UIImageView alloc] initWithImage:checkmarkIcon];
    _checkmarkView.translatesAutoresizingMaskIntoConstraints = NO;
    _checkmarkView.hidden = YES;  // The checkmark is hidden by default.

    [self.contentView addSubview:_checkmarkView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[text]-(margin)-[checkmark(checkmarkSize)]-(margin)-|",
          @"V:|-(verticalMargin)-[text]-(verticalMargin)-|",
          @"V:[checkmark(checkmarkSize)]",
        ],
        @{
          @"text" : _titleLabel,
          @"checkmark" : _checkmarkView,
        },
        @{
          @"margin" : @(kMargin),
          @"verticalMargin" : @(kVerticalMargin),
          @"checkmarkSize" : @(kCheckmarkIconSize),
        });

    AddSameCenterYConstraint(self.contentView, _checkmarkView);

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
}

- (void)setCheckmark:(BOOL)checkmark {
  if (checkmark) {
    self.checkmarkView.hidden = NO;
    self.accessibilityTraits |= UIAccessibilityTraitSelected;
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
  // Hide the checkmark and clear UIAccessibilityTraitSelected from the
  // accessibility traits, if applicable.
  self.checkmarkView.hidden = YES;
  self.accessibilityTraits &= ~UIAccessibilityTraitSelected;
  self.selected = NO;
  [self setTitle:nil];
}

#pragma mark - Private

// Callback when the preferred Content Size change.
- (void)preferredContentSizeDidChange:(NSNotification*)notification {
  self.titleLabel.font = [self titleFont];
}

// Returns the font to be used for the title.
- (UIFont*)titleFont {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

@end
