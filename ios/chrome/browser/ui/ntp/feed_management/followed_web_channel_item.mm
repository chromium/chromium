// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowedWebChannelItem

- (NSString*)text {
  return self.channel.title;
}

- (NSString*)detailText {
  return self.channel.hostname;
}

@end
