// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - BookmarkParentFolderItem

@implementation BookmarkParentFolderItem

@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.accessibilityIdentifier = @"Change Folder";
    self.cellClass = [BookmarkParentFolderCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  BookmarkParentFolderCell* cell =
      base::mac::ObjCCastStrict<BookmarkParentFolderCell>(tableCell);
  cell.parentFolderNameLabel.text = self.title;
}

@end

#pragma mark - BookmarkParentFolderCell

@interface BookmarkParentFolderCell ()
// Stack view to display label / value which we'll switch from horizontal to
// vertical based on preferredContentSizeCategory.
@property(nonatomic, strong) UIStackView* stackView;
@end

@interface BookmarkParentFolderCell ()
@property(nonatomic, readwrite, strong) UILabel* parentFolderNameLabel;
@end

@implementation BookmarkParentFolderCell
@synthesize parentFolderNameLabel = _parentFolderNameLabel;
@synthesize stackView = _stackView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (!self)
    return nil;

  self.isAccessibilityElement = YES;
  self.accessibilityTraits |= UIAccessibilityTraitButton;

  // "Folder" decoration label.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_BOOKMARK_GROUP_BUTTON);
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  [titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                forAxis:UILayoutConstraintAxisHorizontal];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Parent Folder name label.
  self.parentFolderNameLabel = [[UILabel alloc] init];
  self.parentFolderNameLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.parentFolderNameLabel.adjustsFontForContentSizeCategory = YES;
  self.parentFolderNameLabel.textColor = UIColor.cr_secondaryLabelColor;
  self.parentFolderNameLabel.textAlignment = NSTextAlignmentRight;
  [self.parentFolderNameLabel
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];

  // Container StackView.
  self.stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, self.parentFolderNameLabel ]];
  self.stackView.axis = UILayoutConstraintAxisHorizontal;
  self.stackView.spacing = kBookmarkCellViewSpacing;
  self.stackView.distribution = UIStackViewDistributionFill;
  self.stackView.alignment = UIStackViewAlignmentCenter;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.stackView];

  // Set up constraints.
  AddSameConstraintsToSidesWithInsets(
      self.stackView, self.contentView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom |
          LayoutSides::kTop,
      ChromeDirectionalEdgeInsetsMake(
          kBookmarkCellVerticalInset, kBookmarkCellHorizontalLeadingInset,
          kBookmarkCellVerticalInset,
          kBookmarkCellHorizontalAccessoryViewSpacing));

  // Chevron accessory view.
  UIImageView* navigationChevronImage = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
  self.accessoryView = navigationChevronImage;
  // TODO(crbug.com/870841): Use default accessory type.
  if (base::i18n::IsRTL())
    self.accessoryView.transform = CGAffineTransformMakeRotation(M_PI);

  [self applyContentSizeCategoryStyles];

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.parentFolderNameLabel.text = nil;
}

- (NSString*)accessibilityLabel {
  return self.parentFolderNameLabel.text;
}

- (NSString*)accessibilityHint {
  return l10n_util::GetNSString(
      IDS_IOS_BOOKMARK_EDIT_PARENT_FOLDER_BUTTON_HINT);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self applyContentSizeCategoryStyles];
  }
}

- (void)applyContentSizeCategoryStyles {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIScreen.mainScreen.traitCollection.preferredContentSizeCategory)) {
    self.stackView.axis = UILayoutConstraintAxisVertical;
    self.stackView.alignment = UIStackViewAlignmentLeading;
    self.parentFolderNameLabel.textAlignment = NSTextAlignmentLeft;
  } else {
    self.stackView.axis = UILayoutConstraintAxisHorizontal;
    self.stackView.alignment = UIStackViewAlignmentCenter;
    self.parentFolderNameLabel.textAlignment = NSTextAlignmentRight;
  }
}

@end
