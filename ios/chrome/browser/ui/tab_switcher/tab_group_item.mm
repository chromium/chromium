// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state.h"

namespace {
const CGFloat kFaviconSize = 16;
}

@implementation TabGroupItem {
  WebStateList* _webStateList;
  NSMutableArray<GroupTabInfo*>* _tabGroupInfos;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
                    webStateList:(WebStateList*)webStateList {
  CHECK(tabGroup);
  CHECK(webStateList);
  self = [super init];
  if (self) {
    _tabGroup = tabGroup;
    _webStateList = webStateList;
    _tabGroupInfos = [[NSMutableArray alloc] init];
  }
  return self;
}

- (NSString*)title {
  return base::SysUTF16ToNSString(_tabGroup->visual_data().title());
}

- (UIColor*)groupColor {
  return ColorForTabGroupColorId(_tabGroup->visual_data().color());
}

- (NSInteger)numberOfTabsInGroup {
  return (NSUInteger)_webStateList->GetGroupRange(_tabGroup).count();
}

- (void)fetchGroupTabInfos:(GroupTabInfosFetchingCompletionBlock)completion {
  NSUInteger numberOfRequestedImages = 0;
  for (int index : _webStateList->GetGroupRange(_tabGroup)) {
    if (numberOfRequestedImages >= 7) {
      break;
    }
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    CHECK(webState);
    __weak TabGroupItem* weakSelf = self;
    base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
    SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
        ^(UIImage* snapshot) {
          [weakSelf saveSnapshot:snapshot
                         favicon:[weakSelf faviconFromWebState:weakWebState]
                      completion:completion];
        });
    numberOfRequestedImages++;
  }
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

#pragma mark - Private helpers

// Returns the favicon for the given `webState` or nil otherwise.
- (UIImage*)faviconFromWebState:(base::WeakPtr<web::WebState>)webState {
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

// Saves the snapshot and favicon couple in the same GroupTabInfo. Call the
// completion if there is no new snapshot or favicon to save.
- (void)saveSnapshot:(UIImage*)snapshot
             favicon:(UIImage*)favicon
          completion:(GroupTabInfosFetchingCompletionBlock)completion {
  GroupTabInfo* info = [[GroupTabInfo alloc] init];
  info.snapshot = snapshot;
  info.favicon = favicon;
  [_tabGroupInfos addObject:info];
  if (static_cast<int>([_tabGroupInfos count]) ==
      MIN(_webStateList->GetGroupRange(_tabGroup).count(), 7)) {
    completion(self, _tabGroupInfos);
  }
}

@end
