// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/cells/table_view_bookmarks_folder_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_table_cell_title_edit_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Width by which to indent folder cell's content. This is multiplied by the
// `indentationLevel` of the cell.
const CGFloat kFolderCellIndentationWidth = 44.0;
// The amount in points by which to inset horizontally the cell contents.
const CGFloat kFolderCellHorizonalInset = 17.0;
}  // namespace

#pragma mark - TableViewBookmarksFolderItem

@interface TableViewBookmarksFolderItem ()
@property(nonatomic, assign) TableViewBookmarksFolderStyle style;
@end

@implementation TableViewBookmarksFolderItem
@synthesize currentFolder = _currentFolder;
@synthesize indentationLevel = _indentationLevel;
@synthesize style = _style;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type
                       style:(TableViewBookmarksFolderStyle)style {
  if ((self = [super initWithType:type])) {
    self.cellClass = [TableViewBookmarksFolderCell class];
    self.style = style;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewBookmarksFolderCell* folderCell =
      base::apple::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
  switch (self.style) {
    case BookmarksFolderStyleNewFolder: {
      folderCell.folderTitleTextField.text =
          l10n_util::GetNSString(IDS_IOS_BOOKMARK_CREATE_GROUP);
      folderCell.folderImageView.image =
          [UIImage imageNamed:@"bookmark_blue_new_folder"];
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      break;
    }
    case BookmarksFolderStyleFolderEntry: {
      folderCell.folderTitleTextField.text = self.title;
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      if (self.isCurrentFolder) {
        folderCell.bookmarksAccessoryType =
            BookmarksFolderAccessoryTypeCheckmark;
      }
      // In order to indent the cell's content we need to modify its
      // indentation constraint.
      folderCell.indentationConstraint.constant =
          folderCell.indentationConstraint.constant +
          kFolderCellIndentationWidth * self.indentationLevel;
      folderCell.folderImageView.image =
          [UIImage imageNamed:@"bookmark_blue_folder"];
      CGFloat separatorInset =
          kFolderCellHorizonalInset +
          (kFolderCellIndentationWidth * (self.indentationLevel + 1));
      folderCell.separatorInset = UIEdgeInsetsMake(0, separatorInset, 0, 0);
      break;
    }
  }
  folderCell.cloudSlashedView.hidden = !self.shouldDisplayCloudSlashIcon;
}

@end

#pragma mark - TableViewBookmarksFolderCell

@interface TableViewBookmarksFolderCell () <UITextFieldDelegate>
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* indentationConstraint;
// True when title text has ended editing and committed.
@property(nonatomic, assign) BOOL isTextCommitted;
@end

@implementation TableViewBookmarksFolderCell
@synthesize bookmarksAccessoryType = _bookmarksAccessoryType;
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

    // Slashed cloud view.
    UIImage* cloudSlashedImage = CustomSymbolWithPointSize(
        kCloudSlashSymbol, kCloudSlashSymbolPointSize);
    self.cloudSlashedView =
        [[UIImageView alloc] initWithImage:cloudSlashedImage];
    self.cloudSlashedView.tintColor = CloudSlashTintColor();
    self.cloudSlashedView.hidden = YES;
    [self.cloudSlashedView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Container StackView.
    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          self.folderImageView, self.folderTitleTextField, self.cloudSlashedView
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

- (void)setBookmarksAccessoryType:
    (TableViewBookmarksFolderAccessoryType)bookmarksAccessoryType {
  _bookmarksAccessoryType = bookmarksAccessoryType;
  switch (_bookmarksAccessoryType) {
    case BookmarksFolderAccessoryTypeCheckmark:
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]];
      break;
    case BookmarksFolderAccessoryTypeDisclosureIndicator: {
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
      // TODO(crbug.com/41405943): Use default accessory type.
      if (base::i18n::IsRTL()) {
        self.accessoryView.transform = CGAffineTransformMakeRotation(M_PI);
      }
      break;
    }
    case BookmarksFolderAccessoryTypeNone:
      self.accessoryView = nil;
      break;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.bookmarksAccessoryType = BookmarksFolderAccessoryTypeNone;
  self.indentationWidth = 0;
  self.imageView.image = nil;
  self.indentationConstraint.constant = kFolderCellHorizonalInset;
  self.folderTitleTextField.userInteractionEnabled = NO;
  self.textDelegate = nil;
  self.folderTitleTextField.accessibilityIdentifier = nil;
  self.accessoryType = UITableViewCellAccessoryNone;
  self.isAccessibilityElement = YES;
  self.cloudSlashedView.hidden = YES;
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
  if (!self.cloudSlashedView.hidden) {
    return l10n_util::GetNSStringF(
        IDS_IOS_BOOKMARKS_FOLDER_NAME_WITH_CLOUD_SLASH_ICON_LABEL,
        base::SysNSStringToUTF16(self.folderTitleTextField.text));
  }
  return self.folderTitleTextField.text;
}

@end
