// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/password_note_cell.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordNoteCell () <UITextViewDelegate>
@end

@implementation PasswordNoteCell

@synthesize textLabel = _textLabel;

+ (NSString*)reuseID {
  return @"PasswordNoteCell";
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
    _textLabel = [[UILabel alloc] init];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    [self.contentView addSubview:_textLabel];

    _textView = [[UITextView alloc] init];
    _textView.adjustsFontForContentSizeCategory = YES;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textView.backgroundColor = UIColor.clearColor;
    _textView.scrollEnabled = NO;
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.textContainerInset = UIEdgeInsetsZero;
    [self.contentView addSubview:_textView];

    [NSLayoutConstraint activateConstraints:@[
      // Label constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      // Text constraints.
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_textView.topAnchor
          constraintEqualToAnchor:_textLabel.bottomAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],

    ]];
  }
  return self;
}

- (void)configureCell {
  self.textLabel.text =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NOTE", @"Note");
  self.textView.delegate = self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = @"";
  self.textView.text = @"";
  self.delegate = nil;
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  [self.delegate textViewDidChangeInCell:self];
}

@end
