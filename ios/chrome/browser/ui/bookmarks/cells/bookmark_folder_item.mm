// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_folder_item.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_edit_delegate.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The amount in points by which to offset horizontally the text label.
const CGFloat kTitleLabelLeadingOffset = 18.0;
// The amount in points by which to offset horizontally the image view.
const CGFloat kImageViewLeadingOffset = 1.0;
// Width by which to indent folder cell's content. This is multiplied by the
// |indentationLevel| of the cell.
const CGFloat kFolderCellIndentationWidth = 32.0;
// The amount in points by which to inset horizontally the cell contents.
const CGFloat kFolderCellHorizonalInset = 17.0;
}  // namespace

#pragma mark - BookmarkFolderItem

@interface BookmarkFolderItem ()
@property(nonatomic, assign) BookmarkFolderStyle style;
@end

@implementation BookmarkFolderItem
@synthesize currentFolder = _currentFolder;
@synthesize indentationLevel = _indentationLevel;
@synthesize style = _style;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type style:(BookmarkFolderStyle)style {
  if ((self = [super initWithType:type])) {
    self.cellClass = [TableViewBookmarkFolderCell class];
    self.style = style;
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewBookmarkFolderCell* folderCell =
      base::mac::ObjCCastStrict<TableViewBookmarkFolderCell>(cell);
  switch (self.style) {
    case BookmarkFolderStyleNewFolder: {
      folderCell.folderTitleTextField.text =
          l10n_util::GetNSString(IDS_IOS_BOOKMARK_CREATE_GROUP);
      folderCell.folderImageView.image =
          [UIImage imageNamed:@"bookmark_blue_new_folder"];
      folderCell.accessibilityIdentifier =
          kBookmarkCreateNewFolderCellIdentifier;
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      break;
    }
    case BookmarkFolderStyleFolderEntry: {
      folderCell.folderTitleTextField.text = self.title;
      folderCell.accessibilityIdentifier = self.title;
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      if (self.isCurrentFolder)
        folderCell.bookmarkAccessoryType =
            TableViewBookmarkFolderAccessoryTypeCheckmark;
      // In order to indent the cell's content we need to modify its
      // indentation constraint.
      folderCell.indentationConstraint.constant =
          folderCell.indentationConstraint.constant +
          kFolderCellIndentationWidth * self.indentationLevel;
      folderCell.folderImageView.image =
          [UIImage imageNamed:@"bookmark_blue_folder"];
      break;
    }
  }
}

@end

#pragma mark - TableViewBookmarkFolderCell

@interface TableViewBookmarkFolderCell ()<UITextFieldDelegate>
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* indentationConstraint;
// True when title text has ended editing and committed.
@property(nonatomic, assign) BOOL isTextCommitted;
@end

@implementation TableViewBookmarkFolderCell
@synthesize bookmarkAccessoryType = _bookmarkAccessoryType;
@synthesize folderImageView = _folderImageView;
@synthesize folderTitleTextField = _folderTitleTextFieldl;
@synthesize indentationConstraint = _indentationConstraint;
@synthesize isTextCommitted = _isTextCommitted;
@synthesize textDelegate = _textDelegate;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.isAccessibilityElement = YES;

    self.folderImageView = [[UIImageView alloc] init];
    self.folderImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.folderImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [self.folderImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    self.folderTitleTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    self.folderTitleTextField.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.folderTitleTextField.userInteractionEnabled = NO;
    self.folderTitleTextField.adjustsFontForContentSizeCategory = YES;
    [self.folderTitleTextField
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Container StackView.
    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          self.folderImageView, self.folderTitleTextField
        ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kBookmarkCellViewSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:horizontalStack];

    // Set up constraints.
    self.indentationConstraint = [horizontalStack.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kFolderCellHorizonalInset];
    [NSLayoutConstraint activateConstraints:@[
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kBookmarkCellVerticalInset],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kBookmarkCellVerticalInset],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kFolderCellHorizonalInset],
      self.indentationConstraint,
    ]];
  }
  return self;
}

- (void)setBookmarkAccessoryType:
    (TableViewBookmarkFolderAccessoryType)bookmarkAccessoryType {
  _bookmarkAccessoryType = bookmarkAccessoryType;
  switch (_bookmarkAccessoryType) {
    case TableViewBookmarkFolderAccessoryTypeCheckmark:
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]];
      break;
    case TableViewBookmarkFolderAccessoryTypeDisclosureIndicator: {
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
      // TODO(crbug.com/870841): Use default accessory type.
      if (base::i18n::IsRTL())
        self.accessoryView.transform = CGAffineTransformMakeRotation(M_PI);
      break;
    }
    case TableViewBookmarkFolderAccessoryTypeNone:
      self.accessoryView = nil;
      break;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.bookmarkAccessoryType = TableViewBookmarkFolderAccessoryTypeNone;
  self.indentationWidth = 0;
  self.imageView.image = nil;
  self.indentationConstraint.constant = kFolderCellHorizonalInset;
  self.folderTitleTextField.userInteractionEnabled = NO;
  self.textDelegate = nil;
  self.folderTitleTextField.accessibilityIdentifier = nil;
  self.accessoryType = UITableViewCellAccessoryNone;
  self.isAccessibilityElement = YES;
}

#pragma mark BookmarkTableCellTitleEditing

- (void)startEdit {
  self.isAccessibilityElement = NO;
  self.isTextCommitted = NO;
  self.folderTitleTextField.userInteractionEnabled = YES;
  self.folderTitleTextField.enablesReturnKeyAutomatically = YES;
  self.folderTitleTextField.keyboardType = UIKeyboardTypeDefault;
  self.folderTitleTextField.returnKeyType = UIReturnKeyDone;
  self.folderTitleTextField.accessibilityIdentifier = @"bookmark_editing_text";
  [self.folderTitleTextField becomeFirstResponder];
  // selectAll doesn't work immediately after calling becomeFirstResponder.
  // Do selectAll on the next run loop.
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.folderTitleTextField isFirstResponder]) {
      [self.folderTitleTextField selectAll:nil];
    }
  });
  self.folderTitleTextField.delegate = self;
}

- (void)stopEdit {
  if (self.isTextCommitted) {
    return;
  }
  self.isTextCommitted = YES;
  self.isAccessibilityElement = YES;
  [self.textDelegate textDidChangeTo:self.folderTitleTextField.text];
  self.folderTitleTextField.userInteractionEnabled = NO;
  [self.folderTitleTextField endEditing:YES];
}

#pragma mark UITextFieldDelegate

// This method hides the keyboard when the return key is pressed.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self stopEdit];
  return YES;
}

// This method is called when titleText resigns its first responder status.
// (when return/dimiss key is pressed, or when navigating away.)
- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  [self stopEdit];
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return self.folderTitleTextField.text;
}

@end

#pragma mark - LegacyTableViewBookmarkFolderCell

@implementation LegacyTableViewBookmarkFolderCell
@synthesize checked = _checked;
@synthesize enabled = _enabled;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.textLabel.font = [MDCTypography subheadFont];
    self.textLabel.textColor = bookmark_utils_ios::darkTextColor();
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _enabled = YES;
  }
  return self;
}

- (void)setChecked:(BOOL)checked {
  if (checked != _checked) {
    _checked = checked;
    UIImageView* checkImageView =
        checked ? [[UIImageView alloc]
                      initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]]
                : nil;
    self.accessoryView = checkImageView;
  }
}

- (void)setEnabled:(BOOL)enabled {
  if (enabled != _enabled) {
    _enabled = enabled;
    self.userInteractionEnabled = enabled;
    self.textLabel.enabled = enabled;
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Move the text label as required by the design.
  UIEdgeInsets insets =
      UIEdgeInsetsMakeDirected(0, kTitleLabelLeadingOffset, 0, 0);
  self.textLabel.frame = UIEdgeInsetsInsetRect(self.textLabel.frame, insets);

  // Indent the image. An offset is required in the design.
  LayoutRect layout = LayoutRectForRectInBoundingRect(self.imageView.frame,
                                                      self.contentView.bounds);
  layout.position.leading +=
      self.indentationWidth * self.indentationLevel + kImageViewLeadingOffset;
  self.imageView.frame = LayoutRectGetRect(layout);
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.checked = NO;
  self.enabled = YES;
  self.indentationWidth = 0;
  self.imageView.image = nil;
}

@end
