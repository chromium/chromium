// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/ui_bundled/followed_web_channel.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/model/crurl.h"

@implementation FollowedWebChannel

#pragma mark - NSObject

- (BOOL)isEqualToFollowedWebChannel:(FollowedWebChannel*)channel {
  return channel && [self.title isEqualToString:channel.title] &&
         self.webPageURL.gurl == channel.webPageURL.gurl &&
         self.rssURL.gurl == channel.rssURL.gurl &&
         self.faviconURL.gurl == channel.faviconURL.gurl;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;

  if (![object isMemberOfClass:[FollowedWebChannel class]])
    return NO;

  return [self isEqualToFollowedWebChannel:object];
}

- (NSUInteger)hash {
  return [self.title hash] ^
         [base::SysUTF8ToNSString(self.webPageURL.gurl.spec()) hash] ^
         [base::SysUTF8ToNSString(self.rssURL.gurl.spec()) hash] ^
         [base::SysUTF8ToNSString(self.faviconURL.gurl.spec()) hash];
}

@end
