// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowManagementMediator

#pragma mark - FollowedWebChannelsDataSource

- (NSArray<FollowedWebChannel*>*)followedWebChannels {
  // TODO(crbug.com/1296745): Call provider API to get followed channels.
  return @[];
}

@end
