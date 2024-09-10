// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Height / width of the error icon.
const CGFloat kErrorIconLength = 20;
// Size of the symbols.
const CGFloat kSymbolSize = 15;

}  // namespace

@interface TableViewMultiLineTextEditItem () <UITextViewDelegate>
@end

@implementation TableViewMultiLineTextEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewMultiLineTextEditCell class];
    _validText = YES;
    _textFieldInteractionEnabled = YES;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewMultiLineTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.label;
  cell.textView.text = self.text;
  cell.textView.editable = self.editingEnabled;
  cell.textView.delegate = self;
  cell.textView.backgroundColor = styler.cellBackgroundColor
                                      ? styler.cellBackgroundColor
                                      : styler.tableViewBackgroundColor;

  if (self.label.length) {
    cell.textView.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.label];
  }

  if (self.validText) {
    cell.textView.textColor = [UIColor colorNamed:kTextPrimaryColor];
    cell.iconView.hidden = YES;
    [cell.iconView setImage:nil];
  } else {
    cell.textView.textColor = [UIColor colorNamed:kRedColor];
    cell.iconView.hidden = NO;
    [cell.iconView setImage:[self errorImage]];
    cell.iconView.tintColor = [UIColor colorNamed:kRedColor];
  }

  if (!self.textFieldInteractionEnabled) {
    cell.textView.userInteractionEnabled = NO;
  }
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  self.text = textView.text;
  [self.delegate textViewItemDidChange:self];
}

#pragma mark - Private

// Returns the error icon image.
- (UIImage*)errorImage {
  return DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kSymbolSize);
}

@end

@implementation TableViewMultiLineTextEditCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // The cell shouldn't be an a11y element to make the text view accessible to
    // VoiceOver.
    self.isAccessibilityElement = NO;

    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _textLabel.isAccessibilityElement = NO;
    [contentView addSubview:_textLabel];

    _textView = [[UITextView alloc] init];
    _textView.adjustsFontForContentSizeCategory = YES;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textView.scrollEnabled = NO;
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.textContainerInset = UIEdgeInsetsZero;
    _textView.isAccessibilityElement = YES;
    [contentView addSubview:_textView];

    _iconView = [[UIImageView alloc] initWithImage:nil];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.isAccessibilityElement = NO;
    [contentView addSubview:_iconView];

    [NSLayoutConstraint activateConstraints:@[
      // Label constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      // Text constraints.
      [_textView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_textView.topAnchor
          constraintEqualToAnchor:_textLabel.bottomAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      [_textView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],
      // Icon constraints.
      [_iconView.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_iconView.heightAnchor constraintEqualToConstant:kErrorIconLength],
      [_iconView.widthAnchor constraintEqualToAnchor:_iconView.heightAnchor],
      [_iconView.centerYAnchor
          constraintEqualToAnchor:_textLabel.centerYAnchor],

    ]];
  }
  return self;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textView.text = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", self.textLabel.text, self.textView.text];
}

@end
