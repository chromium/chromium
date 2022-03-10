// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FollowedWebChannelItem ()

// This property contains a value if the WebChannel is unavailable.
@property(nonatomic, copy) NSAttributedString* detailAttributedString;

@end

@implementation FollowedWebChannelItem

- (void)setFollowedWebChannel:(FollowedWebChannel*)followedWebChannel {
  _followedWebChannel = followedWebChannel;
  self.title = followedWebChannel.title;
  if (!_followedWebChannel.unavailable) {
    self.detailText = _followedWebChannel.hostname;
    return;
  }

  // This approach repurposes an existing cell by simply adding a newline with
  // additional text instead of creating a new cell with an additional label.
  NSString* unavailableText =
      l10n_util::GetNSString(IDS_IOS_FOLLOW_MANAGEMENT_CHANNEL_UNAVAILABLE);
  NSAttributedString* unavailableString = [[NSAttributedString alloc]
      initWithString:[NSString stringWithFormat:@"\n%@", unavailableText]
          attributes:@{NSForegroundColorAttributeName : UIColor.redColor}];
  NSMutableAttributedString* concatenatedString =
      [[NSMutableAttributedString alloc]
          initWithString:_followedWebChannel.hostname];
  [concatenatedString appendAttributedString:unavailableString];
  _detailAttributedString = concatenatedString;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  FollowedWebChannelCell* cell =
      base::mac::ObjCCastStrict<FollowedWebChannelCell>(tableCell);
  cell.followedWebChannel = self.followedWebChannel;

  // TODO(crbug.com/1296745): Modify TableViewURLCell to have spinner and third
  // row text.
}

@end

@implementation FollowedWebChannelCell
@end
