// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_mediator.h"

#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

@interface HistoryMediator ()
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@end

@implementation HistoryMediator
@synthesize profile = _profile;
@synthesize faviconLoader = _faviconLoader;

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    CHECK(profile);
    _profile = profile;
    _faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(_profile);
    CHECK(_faviconLoader);
  }
  return self;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

@end
