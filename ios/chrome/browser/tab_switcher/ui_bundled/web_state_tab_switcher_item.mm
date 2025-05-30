// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"

#import "base/apple/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/web_state.h"

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

- (web::WebState*)webState {
  return _webState.get();
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

#pragma mark - Favicons

- (UIImage*)NTPFavicon {
  return [[UIImage alloc] init];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[WebStateTabSwitcherItem class]]) {
    return NO;
  }
  WebStateTabSwitcherItem* otherItem =
      base::apple::ObjCCastStrict<WebStateTabSwitcherItem>(object);
  return self.identifier == otherItem.identifier;
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.identifier.identifier());
}

@end
