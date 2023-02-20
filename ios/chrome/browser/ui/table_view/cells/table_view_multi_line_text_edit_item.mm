// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_line_text_edit_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TableViewMultiLineTextEditItem () <UITextViewDelegate>
@end

@implementation TableViewMultiLineTextEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewMultiLineTextEditCell class];
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

  if (self.label.length) {
    cell.textView.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.label];
  }
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  self.text = textView.text;
  [self.delegate textViewItemDidChange:self];
}

@end

@implementation TableViewMultiLineTextEditCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;

    _textView = [[UITextView alloc] init];
    _textView.scrollEnabled = NO;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textView.adjustsFontForContentSizeCategory = YES;

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _textView ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.spacing = kTableViewOneLabelCellVerticalSpacing;
    [contentView addSubview:stackView];

    [NSLayoutConstraint activateConstraints:@[
      [stackView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [stackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [stackView.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                          constant:kTableViewVerticalSpacing],
      [stackView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewVerticalSpacing],
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
