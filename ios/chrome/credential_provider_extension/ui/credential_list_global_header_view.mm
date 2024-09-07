// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_global_header_view.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Spacing above the label.
const CGFloat kTopSpacing = 8;

// Spacing below the label.
const CGFloat kBottomSpacing = 20;

}  // namespace

@interface CredentialListGlobalHeaderView ()

// Label to hold the actual text.
@property(nonatomic, strong) UILabel* headerTextLabel;

@end

@implementation CredentialListGlobalHeaderView

+ (NSString*)reuseID {
  return @"CredentialListGlobalHeaderView";
}

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  if ((self = [super initWithReuseIdentifier:reuseIdentifier])) {
    _headerTextLabel = [[UILabel alloc] init];
    _headerTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _headerTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _headerTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _headerTextLabel.numberOfLines = 0;
    _headerTextLabel.text = [self headerText];

    [self.contentView addSubview:_headerTextLabel];

    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.leadingAnchor
          constraintEqualToAnchor:_headerTextLabel.leadingAnchor],
      [self.contentView.trailingAnchor
          constraintEqualToAnchor:_headerTextLabel.trailingAnchor],
      [self.contentView.topAnchor
          constraintEqualToAnchor:_headerTextLabel.topAnchor
                         constant:-kTopSpacing],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:_headerTextLabel.bottomAnchor
                         constant:kBottomSpacing],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  // Update the text here, just in case it has changed in between uses.
  self.headerTextLabel.text = [self headerText];
}

#pragma mark - Private

// Returns the header text depending of password sync (represented by the user's
// email not being available as used in the sync disclaimer).
- (NSString*)headerText {
  NSString* syncingUserEmail = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];

  BOOL passwordSyncOn = syncingUserEmail != nil;

  if (passwordSyncOn) {
    return NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_CREDENTIAL_LIST_BRANDED_HEADER_SYNC",
        @"The information provided in the header of password list.");
  } else {
    return NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_CREDENTIAL_LIST_BRANDED_HEADER_NO_SYNC",
        @"The information provided in the header of password list.");
  }
}

@end
