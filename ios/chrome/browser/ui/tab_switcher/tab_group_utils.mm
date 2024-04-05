// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_utils.h"

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/web/public/web_state.h"

namespace {
const CGFloat kFaviconSize = 16;
}

@implementation TabGroupUtils

+ (void)fetchTabGroupInfoFromWebState:(web::WebState*)webState
                           completion:(void (^)(GroupTabInfo*))completion {
  CHECK(webState);
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* snapshot) {
        GroupTabInfo* info = [[GroupTabInfo alloc] init];
        info.snapshot = snapshot;
        info.favicon = [TabGroupUtils faviconFromWebState:weakWebState];
        completion(info);
      });
}

#pragma mark - Private helpers

// Returns the favicon for the given `webState` or nil otherwise.
+ (UIImage*)faviconFromWebState:(base::WeakPtr<web::WebState>)webState {
  if (!webState) {
    return nil;
  }

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];

  if (IsUrlNtp(webState->GetVisibleURL())) {
    return CustomSymbolWithConfiguration(kChromeProductSymbol, configuration);
  }

  // Use the page favicon.
  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState.get());
  // The favicon driver may be null during testing.
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty()) {
      return favicon.ToUIImage();
    }
  }

  // Return the default favicon.
  return DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
}

@end
