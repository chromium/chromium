// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/password_note_footer_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@interface PasswordNoteFooterView ()
@end

@implementation PasswordNoteFooterView

@synthesize textLabel = _textLabel;

+ (NSString*)reuseID {
  return @"PasswordNoteFooterView";
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithReuseIdentifier:reuseIdentifier])) {
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [self.contentView addSubview:_textLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kTableViewVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _textLabel.text = nil;
}

@end
