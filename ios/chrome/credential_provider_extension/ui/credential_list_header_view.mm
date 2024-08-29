// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_header_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation CredentialListHeaderView

+ (NSString*)reuseID {
  return @"CredentialListHeaderView";
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithReuseIdentifier:reuseIdentifier])) {
    _headerTextLabel = [[UILabel alloc] init];
    _headerTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _headerTextLabel.numberOfLines = 0;
    _headerTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _headerTextLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

    [self.contentView addSubview:_headerTextLabel];

    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.leadingAnchor
          constraintEqualToAnchor:_headerTextLabel.leadingAnchor],
      [self.contentView.trailingAnchor
          constraintEqualToAnchor:_headerTextLabel.trailingAnchor],
      [_headerTextLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:_headerTextLabel.bottomAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.headerTextLabel.text = @"";
}

@end
