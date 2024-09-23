// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_text_item.h"

#import <stdlib.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/browser/ui/reading_list/number_badge_view.h"
#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kCellHeight = 32;
const CGFloat kMargin = 15;
const CGFloat kTopMargin = 8;
const CGFloat kMaxHeight = 100;

NSMutableAttributedString* GetAttributedString(NSString* imageName,
                                               NSString* message) {
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  // Add a space to have a distance with the leading icon.
  NSAttributedString* attributedString = AttributedStringFromStringWithLink(
      [@" " stringByAppendingString:message], textAttributes, linkAttributes);

  // Create the leading enterprise icon.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [UIImage imageNamed:imageName];

  // Making sure the image is well centered vertically relative to the text,
  // and also that the image scales with the text size.
  CGFloat height = attributedString.size.height;
  CGFloat capHeight =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote].capHeight;
  CGFloat verticalOffset = roundf(capHeight - height) / 2.f;
  attachment.bounds = CGRectMake(0, verticalOffset, height, height);

  NSMutableAttributedString* outputString = [[NSAttributedString
      attributedStringWithAttachment:attachment] mutableCopy];
  [outputString appendAttributedString:attributedString];

  return outputString;
}

}  // namespace

@implementation PopupMenuTextItem

@synthesize actionIdentifier = _actionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuTextCell class];
  }
  return self;
}

- (void)configureCell:(PopupMenuTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.userInteractionEnabled = YES;
  NSMutableAttributedString* StringOfCell =
      GetAttributedString(self.imageName, self.message);
  cell.messageAttributedString = StringOfCell;
  cell.messageLabel.attributedText = StringOfCell;
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/41380449): This should be done at the table view level.
  static PopupMenuTextCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[PopupMenuTextCell alloc] init];
    [cell registerForContentSizeUpdates];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

#pragma mark - PopupMenuTextCell

@implementation PopupMenuTextCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* selectedBackgroundView = [[UIView alloc] init];
    selectedBackgroundView.backgroundColor =
        [UIColor colorNamed:kTableViewRowHighlightColor];
    self.selectedBackgroundView = selectedBackgroundView;

    _messageLabel = [[UILabel alloc] init];
    _messageLabel.numberOfLines = 0;
    _messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _messageLabel.adjustsFontForContentSizeCategory = YES;

    [self.contentView addSubview:_messageLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_messageLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kCellHeight],
    ]];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[label]-(>=margin)-|",
          @"V:|-(>=topMargin)-[label]-(>=topMargin)-|"
        ],
        @{
          @"label" : self.messageLabel,
        },
        @{
          @"margin" : @(kMargin),
          @"topMargin" : @(kTopMargin),
        });

    // The height constraint is used to have something as small as possible when
    // calculating the size of the prototype cell.
    NSLayoutConstraint* heightConstraint =
        [self.contentView.heightAnchor constraintEqualToConstant:kCellHeight];
    heightConstraint.priority = UILayoutPriorityDefaultLow;
    heightConstraint.active = YES;
  }
  return self;
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

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  self.messageLabel.preferredMaxLayoutWidth = parentWidth - kMargin * 2;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.userInteractionEnabled = NO;
  self.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  return self.messageAttributedString.string;
}

#pragma mark - Private

// Callback when the preferred Content Size change.
- (void)preferredContentSizeDidChange:(NSNotification*)notification {
  self.messageLabel.attributedText = self.messageAttributedString;
}

@end
