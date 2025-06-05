// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"

#import "base/task/sequenced_task_runner.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_fetch_info.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ui/gfx/favicon_size.h"

namespace {

// Size for the default favicon.
const CGFloat kFaviconSize = 16.0;
// The minimum size of tab favicons.
constexpr CGFloat kFaviconMinimumSize = 8.0;

// Returns a key for the given `group_item`.
NSValue* TabGroupIdentifierKeyforTabGroupItem(TabGroupItem* group_item) {
  return [NSValue valueWithPointer:group_item.tabGroupIdentifier];
}

}  // namespace

TabSnapshotAndFaviconConfigurator::TabSnapshotAndFaviconConfigurator(
    FaviconLoader* favicon_loader)
    : favicon_loader_(favicon_loader) {
  group_item_fetches_ = [[NSMutableDictionary alloc] init];
}

TabSnapshotAndFaviconConfigurator::~TabSnapshotAndFaviconConfigurator() {}

void TabSnapshotAndFaviconConfigurator::FetchSnapshotAndFaviconForTabGroupItem(
    TabGroupItem* group_item,
    WebStateList* web_state_list,
    void (^completion)(TabGroupItem* item,
                       NSInteger tabIndex,
                       TabSnapshotAndFavicon* tabSnapshotAndFavicon)) {
  const TabGroup* tab_group = group_item.tabGroup;
  if (!tab_group || !web_state_list) {
    return;
  }
  NSInteger tab_count = tab_group->range().count();
  NSInteger number_of_requests = TabGroupItemDisplayedVisualCount(tab_count);

  // Overwrite any previous request for this item.
  NSUUID* request_id = [NSUUID UUID];
  TabGroupItemFetchInfo* fetch_info = [[TabGroupItemFetchInfo alloc]
      initWithRequestID:request_id
      initialFetchCount:TabGroupItemFetchRequestsCount(tab_count)];
  group_item_fetches_[TabGroupIdentifierKeyforTabGroupItem(group_item)] =
      fetch_info;

  NSInteger first_index = tab_group->range().range_begin();
  for (NSInteger request_index = 0; request_index < number_of_requests;
       request_index++) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(first_index + request_index);
    CHECK(web_state);
    FetchSnapshotAndFaviconFromWebState(group_item, web_state, request_index,
                                        request_id, completion);
  }
}

void TabSnapshotAndFaviconConfigurator::
    FetchSingleSnapshotAndFaviconFromWebState(
        web::WebState* web_state,
        void (^completion)(TabSnapshotAndFavicon* tab_snapshot_and_favicon)) {
  auto inner_completion = ^(TabGroupItem* item, NSInteger tabIndex,
                            TabSnapshotAndFavicon* tabSnapshotAndFavicon) {
    completion(tabSnapshotAndFavicon);
  };

  FetchSnapshotAndFaviconFromWebState(
      /*group_item=*/nil, web_state,
      /*request_index=*/0, /*request_id=*/nil, inner_completion);
}

void TabSnapshotAndFaviconConfigurator::
    FetchSnapshotAndFaviconForTabSwitcherItem(
        WebStateTabSwitcherItem* tab_item,
        void (^completion)(WebStateTabSwitcherItem* item,
                           TabSnapshotAndFavicon* tab_snapshot_and_favicon)) {
  FetchSnapshotAndFaviconForTabSwitcherItem(tab_item, /*fetch_snapshot=*/true,
                                            completion);
}

void TabSnapshotAndFaviconConfigurator::FetchFaviconForTabSwitcherItem(
    WebStateTabSwitcherItem* tab_item,
    void (^completion)(WebStateTabSwitcherItem* item,
                       TabSnapshotAndFavicon* tab_snapshot)) {
  FetchSnapshotAndFaviconForTabSwitcherItem(tab_item, /*fetch_snapshot=*/false,
                                            completion);
}

#pragma mark - Private

void TabSnapshotAndFaviconConfigurator::FetchSnapshotAndFaviconFromWebState(
    TabGroupItem* group_item,
    web::WebState* web_state,
    NSInteger request_index,
    NSUUID* request_id,
    void (^completion)(TabGroupItem* item,
                       NSInteger tabIndex,
                       TabSnapshotAndFavicon* tabSnapshotAndFavicon)) {
  TabSnapshotAndFavicon* tab_snapshot_and_favicon =
      [[TabSnapshotAndFavicon alloc] init];

  // Fetch the snapshot.
  bool shouldFetchSnapshot = true;
  const TabGroup* tab_group = group_item.tabGroup;
  if (tab_group) {
    shouldFetchSnapshot = ShouldFetchSnapshotForTabInGroup(
        request_index, tab_group->range().count());
  }
  if (shouldFetchSnapshot) {
    SnapshotTabHelper::FromWebState(web_state)->RetrieveColorSnapshot(
        ^(UIImage* snapshot) {
          tab_snapshot_and_favicon.snapshot = snapshot;
          OnSnapshotAndFaviconFromWebStateFetched(
              group_item, tab_snapshot_and_favicon, request_index, request_id,
              completion);
        });
  }

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  const GURL& url = web_state->GetVisibleURL();

  // Set the NTP favicon.
  if (IsUrlNtp(url)) {
    tab_snapshot_and_favicon.favicon =
        CustomSymbolWithConfiguration(kChromeProductSymbol, configuration);
    OnSnapshotAndFaviconFromWebStateFetched(
        group_item, tab_snapshot_and_favicon, request_index, request_id,
        completion);
    return;
  }

  // Use the favicon driver.
  favicon::FaviconDriver* favicon_driver =
      favicon::WebFaviconDriver::FromWebState(web_state);
  if (favicon_driver) {
    gfx::Image favicon = favicon_driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      tab_snapshot_and_favicon.favicon = favicon.ToUIImage();
      OnSnapshotAndFaviconFromWebStateFetched(
          group_item, tab_snapshot_and_favicon, request_index, request_id,
          completion);
      return;
    }
  }

  // Set the default favicon.
  UIImage* default_favicon =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  if (!favicon_loader_) {
    tab_snapshot_and_favicon.favicon = default_favicon;
    OnSnapshotAndFaviconFromWebStateFetched(
        group_item, tab_snapshot_and_favicon, request_index, request_id,
        completion);
    return;
  }

  // Fetch the favicon.
  favicon_loader_->FaviconForPageUrl(
      url, kFaviconSize, kFaviconMinimumSize,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        // Synchronously returned default favicon.
        if (attributes.usesDefaultImage) {
          return;
        }
        // Asynchronously returned favicon.
        if (attributes.faviconImage) {
          tab_snapshot_and_favicon.favicon = attributes.faviconImage;
        } else {
          tab_snapshot_and_favicon.favicon = default_favicon;
        }
        OnSnapshotAndFaviconFromWebStateFetched(
            group_item, tab_snapshot_and_favicon, request_index, request_id,
            completion);
      });
}

void TabSnapshotAndFaviconConfigurator::OnSnapshotAndFaviconFromWebStateFetched(
    TabGroupItem* group_item,
    TabSnapshotAndFavicon* tab_snapshot_and_favicon,
    NSInteger request_index,
    NSUUID* request_id,
    void (^completion)(TabGroupItem* item,
                       NSInteger tabIndex,
                       TabSnapshotAndFavicon* tabSnapshotAndFavicon)) {
  if (!group_item) {
    completion(nil, request_index, tab_snapshot_and_favicon);
    return;
  }

  NSValue* group_key = TabGroupIdentifierKeyforTabGroupItem(group_item);

  // Check if the fetch is still the active one before proceeding.
  TabGroupItemFetchInfo* current_fetch_info = group_item_fetches_[group_key];
  if (![current_fetch_info.requestID isEqual:request_id]) {
    return;
  }
  [current_fetch_info decrementRemainingFetches];

  // If all fetches are completed, remove the `current_fetch_info` from
  // dictionary.
  if ([current_fetch_info currentRemainingFetches] == 0) {
    [group_item_fetches_ removeObjectForKey:group_key];
  }
  // Check that the group still exists.
  if (!group_item.tabGroup) {
    return;
  }

  completion(group_item, request_index, tab_snapshot_and_favicon);
}

void TabSnapshotAndFaviconConfigurator::
    FetchSnapshotAndFaviconForTabSwitcherItem(
        WebStateTabSwitcherItem* tab_item,
        bool fetch_snapshot,
        void (^completion)(WebStateTabSwitcherItem* item,
                           TabSnapshotAndFavicon* tab_snapshot_and_favicon)) {
  if (!tab_item.webState) {
    completion(tab_item, nil);
    return;
  }

  TabSnapshotAndFavicon* tab_snapshot_and_favicon =
      [[TabSnapshotAndFavicon alloc] init];

  // Fetch the snapshot.
  if (fetch_snapshot) {
    SnapshotTabHelper::FromWebState(tab_item.webState)
        ->RetrieveColorSnapshot(^(UIImage* snapshot) {
          // If there is no available snapshot, configure the snapshot to be an
          // empty image in order to pass
          // `OnSnapshotAndFaviconForTabSwitcherItemFetched` checks.
          tab_snapshot_and_favicon.snapshot = snapshot;
          completion(tab_item, tab_snapshot_and_favicon);
        });
  }

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  const GURL& url = tab_item.webState->GetVisibleURL();

  // Set the NTP favicon.
  if (IsUrlNtp(url)) {
    // Each WebStateTabSwitcherItem subclass provides its own NTPFavicon.
    tab_snapshot_and_favicon.favicon = tab_item.NTPFavicon;
    completion(tab_item, tab_snapshot_and_favicon);
    return;
  }

  // Use the favicon driver.
  favicon::FaviconDriver* favicon_driver =
      favicon::WebFaviconDriver::FromWebState(tab_item.webState);
  if (favicon_driver) {
    gfx::Image favicon = favicon_driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      tab_snapshot_and_favicon.favicon = favicon.ToUIImage();
      completion(tab_item, tab_snapshot_and_favicon);
      return;
    }
  }

  // Set the default favicon.
  UIImage* default_favicon =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  if (!favicon_loader_) {
    tab_snapshot_and_favicon.favicon = default_favicon;
    completion(tab_item, tab_snapshot_and_favicon);
    return;
  }

  // Fetch the favicon.
  favicon_loader_->FaviconForPageUrl(
      url, kFaviconSize, kFaviconMinimumSize,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        // Synchronously returned default favicon.
        if (attributes.usesDefaultImage) {
          return;
        }
        // Asynchronously returned favicon.
        if (attributes.faviconImage) {
          tab_snapshot_and_favicon.favicon = attributes.faviconImage;
        } else {
          tab_snapshot_and_favicon.favicon = default_favicon;
        }
        completion(tab_item, tab_snapshot_and_favicon);
      });
}
