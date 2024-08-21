// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

#import "base/apple/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/web_state.h"

namespace {
const CGFloat kSymbolSize = 16;
}

@implementation WebStateTabSwitcherItem {
  // The web state represented by this item.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super initWithIdentifier:webState->GetUniqueIdentifier()];
  if (self) {
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (GURL)URL {
  if (!_webState) {
    return GURL();
  }
  return _webState->GetVisibleURL();
}

- (NSString*)title {
  if (!_webState) {
    return nil;
  }
  return tab_util::GetTabTitle(_webState.get());
}

- (BOOL)hidesTitle {
  if (!_webState) {
    return NO;
  }
  return IsUrlNtp(_webState->GetVisibleURL());
}

- (BOOL)showsActivity {
  if (!_webState) {
    return NO;
  }
  return _webState->IsLoading();
}

#pragma mark - Image Fetching

- (void)fetchFavicon:(TabSwitcherImageFetchingCompletionBlock)completion {
  web::WebState* webState = _webState.get();
  if (!webState) {
    completion(self, nil);
    return;
  }

  // NTP tabs have special treatment.
  if (IsUrlNtp(webState->GetVisibleURL())) {
    completion(self, [self NTPFavicon]);
    return;
  }

  // Use the page favicon.
  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  // The favicon driver may be null during testing.
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty()) {
      completion(self, favicon.ToUIImage());
      return;
    }
  }

  // Otherwise, set a default favicon.
  completion(self, [self defaultFavicon]);
}

- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion {
  web::WebState* webState = _webState.get();
  if (!webState) {
    completion(self, nil);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* snapshot) {
        if (weakSelf) {
          completion(weakSelf, snapshot);
        }
      });
}

#pragma mark - Favicons

- (UIImage*)defaultFavicon {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  return DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
}

- (UIImage*)NTPFavicon {
  // By default NTP tabs gets no favicon.
  return nil;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[WebStateTabSwitcherItem class]]) {
    return NO;
  }
  WebStateTabSwitcherItem* otherTabStrip =
      base::apple::ObjCCastStrict<WebStateTabSwitcherItem>(object);
  return self.identifier == otherTabStrip.identifier;
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.identifier.identifier());
}

@end
