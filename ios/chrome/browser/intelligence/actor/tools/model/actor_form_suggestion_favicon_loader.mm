// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_loader.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

@implementation ActorFormSuggestionFaviconLoader {
  // Favicon loader used to fetch credential favicons;
  // cleared via `-disconnect` on teardown.
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)disconnect {
  _faviconLoader = nullptr;
}

#pragma mark - ActorFormSuggestionFaviconDataSource

- (BOOL)actorFormSuggestion:(ActorFormSuggestion*)suggestion
    loadFaviconWithCompletion:(ActorFormSuggestionFaviconCompletion)completion {
  CHECK(completion);
  if (!_faviconLoader || !suggestion.credential.has_value()) {
    return NO;
  }
  GURL siteURL(suggestion.credential->source_site_or_app);
  if (!siteURL.is_valid() || !siteURL.SchemeIsHTTPOrHTTPS()) {
    siteURL = suggestion.credential->request_origin.GetURL();
  }
  if (!siteURL.is_valid()) {
    return NO;
  }
  _faviconLoader->FaviconForPageUrlOrHost(
      siteURL, kDesiredMediumFaviconSizePt,
      ^(FaviconAttributes* attributes, bool cached) {
        completion(attributes.faviconImage);
      });
  return YES;
}

@end
