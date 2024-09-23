// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_footer_view.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Spacing above the label.
const CGFloat kLabelSpacing = 8;

}  // namespace

@interface NewPasswordFooterView ()

// Label to hold the actual text.
@property(nonatomic, strong) UILabel* footerTextLabel;

@end

@implementation NewPasswordFooterView

+ (NSString*)reuseID {
  return @"NewPasswordFooterView";
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithReuseIdentifier:reuseIdentifier])) {
    _footerTextLabel = [[UILabel alloc] init];
    _footerTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _footerTextLabel.numberOfLines = 0;
    _footerTextLabel.text = [self footerText];
    _footerTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _footerTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    [self.contentView addSubview:_footerTextLabel];

    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.leadingAnchor
          constraintEqualToAnchor:_footerTextLabel.leadingAnchor],
      [self.contentView.trailingAnchor
          constraintEqualToAnchor:_footerTextLabel.trailingAnchor],
      [_footerTextLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kLabelSpacing],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:_footerTextLabel.bottomAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  // Update the text here, just in case it has changed in between uses.
  self.footerTextLabel.text = [self footerText];
}

#pragma mark - Private

- (NSString*)footerText {
  NSString* userEmail = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];

  if (userEmail) {
    NSString* baseLocalizedString = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_FOOTER_BRANDED_SYNC",
        @"Disclaimer telling users what will happen to their passwords");
    return [baseLocalizedString stringByReplacingOccurrencesOfString:@"$1"
                                                          withString:userEmail];
  } else {
    return NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_FOOTER_BRANDED_NO_SYNC",
        @"Disclaimer telling non-logged in users what "
        @"will happen to their passwords");
  }
}

@end
