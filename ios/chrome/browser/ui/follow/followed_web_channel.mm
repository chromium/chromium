// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/followed_web_channel.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowedWebChannel

- (instancetype)initWithTitle:(NSString*)title
                        crURL:(CrURL*)channelURL
                  unavailable:(BOOL)unavailable
         unfollowRequestBlock:(FollowRequestBlock)unfollowRequestBlock
         refollowRequestBlock:(FollowRequestBlock)refollowRequestBlock {
  self = [super init];
  if (self) {
    _title = title;
    _channelURL = channelURL;
    _unavailable = unavailable;
    _unfollowRequestBlock = unfollowRequestBlock;
    _refollowRequestBlock = refollowRequestBlock;
  }
  return self;
}

@end
