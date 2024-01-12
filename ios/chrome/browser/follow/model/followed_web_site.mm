// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/followed_web_site.h"

#import "base/check.h"

@implementation FollowedWebSite

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;

  if (![object isMemberOfClass:[FollowedWebSite class]])
    return NO;

  return [self isEqualToFollowedWebSite:object];
}

- (NSUInteger)hash {
  return [self.title hash] ^ [self.webPageURL hash] ^ [self.faviconURL hash] ^
         [self.RSSURL hash];
}

#pragma mark - Private

- (BOOL)isEqualToFollowedWebSite:(FollowedWebSite*)channel {
  DCHECK(channel);
  return [self.title isEqualToString:channel.title] &&
         [self.webPageURL isEqual:channel.webPageURL] &&
         [self.faviconURL isEqual:channel.faviconURL] &&
         [self.RSSURL isEqual:channel.RSSURL];
}

@end
