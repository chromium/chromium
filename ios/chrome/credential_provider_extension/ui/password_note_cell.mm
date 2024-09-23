// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/password_note_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Height / width of the error icon.
const CGFloat kErrorIconLength = 20;

}  // namespace

@interface PasswordNoteCell () <UITextViewDelegate>
@end

@implementation PasswordNoteCell

@synthesize textLabel = _textLabel;

+ (NSString*)reuseID {
  return @"PasswordNoteCell";
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithStyle:style reuseIdentifier:reuseIdentifier])) {
    _textLabel = [[UILabel alloc] init];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    [self.contentView addSubview:_textLabel];

    _iconView = [[UIImageView alloc] initWithImage:nil];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_iconView];

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
      // Icon constraints.
      [_iconView.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_iconView.heightAnchor constraintEqualToConstant:kErrorIconLength],
      [_iconView.widthAnchor constraintEqualToAnchor:_iconView.heightAnchor],
      [_iconView.centerYAnchor
          constraintEqualToAnchor:_textLabel.centerYAnchor],
    ]];
  }
  return self;
}

- (void)configureCell {
  self.textLabel.text =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NOTE", @"Note");
  self.textView.delegate = self;
}

- (void)setValid:(BOOL)valid {
  if (valid) {
    self.textView.textColor = [UIColor colorNamed:kTextPrimaryColor];
    self.iconView.hidden = YES;
    [self.iconView setImage:nil];
  } else {
    self.textView.textColor = [UIColor colorNamed:kRedColor];
    self.iconView.hidden = NO;
    [self.iconView setImage:[self errorImage]];
    self.iconView.tintColor = [UIColor colorNamed:kRedColor];
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = @"";
  self.textView.text = @"";
  self.textView.textColor = nil;
  self.iconView = nil;
  self.delegate = nil;
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
  [self.delegate textViewDidChangeInCell:self];
}

#pragma mark - Private

// Returns the error icon image.
- (UIImage*)errorImage {
  return [[UIImage imageNamed:@"error_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

@end
