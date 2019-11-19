// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_mediator.h"

#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Desired width and height of favicon.
const CGFloat kFaviconWidthHeight = 24;
// Minimum favicon size to retrieve.
const CGFloat kFaviconMinWidthHeight = 16;
}  // namespace

@interface HistoryMediator ()
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@end

@implementation HistoryMediator
@synthesize browserState = _browserState;
@synthesize faviconLoader = _faviconLoader;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
  }
  return self;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(const GURL&)URL
           completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL, kFaviconWidthHeight, kFaviconMinWidthHeight,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

@end
