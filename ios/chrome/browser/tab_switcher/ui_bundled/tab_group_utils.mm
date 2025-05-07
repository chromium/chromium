// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_utils.h"

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_tab_info.h"
#import "ios/web/public/web_state.h"

namespace {
const CGFloat kFaviconSize = 16;
}

@implementation TabGroupUtils

+ (void)fetchTabGroupInfoFromWebState:(web::WebState*)webState
                        faviconLoader:(FaviconLoader*)faviconLoader
                           completion:(void (^)(GroupTabInfo*))completion {
  CHECK(webState);
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
      ^(UIImage* snapshot) {
        GroupTabInfo* groupTabInfo = [[GroupTabInfo alloc] init];
        groupTabInfo.snapshot = snapshot;
        [TabGroupUtils fetchFaviconFromWebState:weakWebState
                                   groupTabInfo:groupTabInfo
                                  faviconLoader:faviconLoader
                                     completion:completion];
      });
}

#pragma mark - Private helpers

// Fetches the favicon for the given `webState` and executes the `completion`
// block.
+ (void)fetchFaviconFromWebState:(base::WeakPtr<web::WebState>)webState
                    groupTabInfo:(GroupTabInfo*)groupTabInfo
                   faviconLoader:(FaviconLoader*)faviconLoader
                      completion:(void (^)(GroupTabInfo*))completion {
  if (!webState) {
    completion(groupTabInfo);
    return;
  }

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];

  if (IsUrlNtp(webState->GetVisibleURL())) {
    groupTabInfo.favicon =
        CustomSymbolWithConfiguration(kChromeProductSymbol, configuration);
    completion(groupTabInfo);
    return;
  }

  // Use the page favicon.
  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState.get());
  // The favicon driver may be null during testing.
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty()) {
      groupTabInfo.favicon = favicon.ToUIImage();
      completion(groupTabInfo);
      return;
    }
  }

  // Use the default favicon.
  groupTabInfo.favicon =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  if (!faviconLoader) {
    completion(groupTabInfo);
    return;
  }
  // TODO(crbug.com/400966281): Fetch favicon on Google server.
  completion(groupTabInfo);
}

@end
