// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_mediator.h"

#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FollowManagementMediator ()

// The current BrowserState.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// FaviconLoader retrieves favicons for a given page URL.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

@end

@implementation FollowManagementMediator

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
  }
  return self;
}

#pragma mark - FollowedWebChannelsDataSource

- (NSArray<FollowedWebChannel*>*)followedWebChannels {
  return ios::GetChromeBrowserProvider()
      .GetFollowProvider()
      ->GetFollowedWebChannels();
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(CrURL*)URL
           completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

@end
