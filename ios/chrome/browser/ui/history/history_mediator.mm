// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_mediator.h"

#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryMediator ()
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@end

@implementation HistoryMediator
@synthesize browserState = _browserState;
@synthesize faviconLoader = _faviconLoader;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
  }
  return self;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(CrURL*)URL
           completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

@end
