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
    void (^completion)(
        TabGroupItem* item,
        NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons)) {
  const TabGroup* tab_group = group_item.tabGroup;
  if (!tab_group || !web_state_list) {
    completion(group_item, nil);
    return;
  }

  // Overwrite any previous request for this item.
  NSUUID* request_id = [NSUUID UUID];
  group_item_fetches_[TabGroupIdentifierKeyforTabGroupItem(group_item)] =
      request_id;

  NSMutableDictionary<NSNumber*, TabSnapshotAndFavicon*>* tab_group_infos =
      [[NSMutableDictionary alloc] init];
  NSUInteger first_index = tab_group->range().range_begin();
  NSUInteger number_of_requests = MIN(7, tab_group->range().count());

  for (NSUInteger request_index = 0; request_index < number_of_requests;
       request_index++) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(first_index + request_index);
    CHECK(web_state);
    FetchSnapshotAndFaviconFromWebState(group_item, web_state, tab_group_infos,
                                        request_index, number_of_requests,
                                        request_id, completion);
  }
}

void TabSnapshotAndFaviconConfigurator::
    FetchSingleSnapshotAndFaviconFromWebState(
        web::WebState* web_state,
        void (^completion)(TabSnapshotAndFavicon* tab_snapshot_and_favicon)) {
  auto inner_completion =
      ^(TabGroupItem* item,
        NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons) {
        CHECK(tab_snapshots_and_favicons.count == 1);
        completion([tab_snapshots_and_favicons firstObject]);
      };

  // For single tab fetches, we don't track them in `group_item_fetches_`
  // as they are not associated with a TabGroupItem that can be re-fetched.
  FetchSnapshotAndFaviconFromWebState(
      /*group_item=*/nil, web_state,
      /*tab_group_infos=*/[[NSMutableDictionary alloc] init],
      /*request_index=*/0, /*number_of_requests=*/1, /*request_id=*/nil,
      inner_completion);
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
    NSMutableDictionary<NSNumber*, TabSnapshotAndFavicon*>* tab_group_infos,
    NSUInteger request_index,
    NSUInteger number_of_requests,
    NSUUID* request_id,
    void (^completion)(
        TabGroupItem* item,
        NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons)) {
  TabSnapshotAndFavicon* tab_snapshot_and_favicon =
      [[TabSnapshotAndFavicon alloc] init];

  // Fetch the snapshot.
  SnapshotTabHelper::FromWebState(web_state)->RetrieveColorSnapshot(
      ^(UIImage* snapshot) {
        // If there is no available snapshot, configure the snapshot to be an
        // empty image in order to pass
        // `OnSnapshotAndFaviconFromWebStateFetched` checks.
        tab_snapshot_and_favicon.snapshot = snapshot ?: [[UIImage alloc] init];
        OnSnapshotAndFaviconFromWebStateFetched(
            group_item, tab_snapshot_and_favicon, tab_group_infos,
            request_index, number_of_requests, request_id, completion);
      });

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
        group_item, tab_snapshot_and_favicon, tab_group_infos, request_index,
        number_of_requests, request_id, completion);
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
          group_item, tab_snapshot_and_favicon, tab_group_infos, request_index,
          number_of_requests, request_id, completion);
      return;
    }
  }

  // Set the default favicon.
  UIImage* default_favicon =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  if (!favicon_loader_) {
    tab_snapshot_and_favicon.favicon = default_favicon;
    OnSnapshotAndFaviconFromWebStateFetched(
        group_item, tab_snapshot_and_favicon, tab_group_infos, request_index,
        number_of_requests, request_id, completion);
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
            group_item, tab_snapshot_and_favicon, tab_group_infos,
            request_index, number_of_requests, request_id, completion);
      });
}

void TabSnapshotAndFaviconConfigurator::OnSnapshotAndFaviconFromWebStateFetched(
    TabGroupItem* group_item,
    TabSnapshotAndFavicon* tab_snapshot_and_favicon,
    NSMutableDictionary<NSNumber*, TabSnapshotAndFavicon*>* tab_group_infos,
    NSUInteger request_index,
    NSUInteger number_of_requests,
    NSUUID* request_id,
    void (^completion)(
        TabGroupItem* item,
        NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons)) {
  // Check if all data have been fetched before proceeding.
  if (!tab_snapshot_and_favicon.snapshot || !tab_snapshot_and_favicon.favicon) {
    return;
  }

  // Check that the group still exists after asynchronous operations.
  if (group_item && !group_item.tabGroup) {
    return;
  }

  // Check if the fetch for this `group_item` is still the active one before
  // proceeding.
  if (group_item &&
      ![group_item_fetches_[TabGroupIdentifierKeyforTabGroupItem(group_item)]
          isEqual:request_id]) {
    return;
  }

  tab_group_infos[@(request_index)] = tab_snapshot_and_favicon;
  if (tab_group_infos.count != number_of_requests) {
    return;
  }

  auto comparator = ^NSComparisonResult(NSNumber* obj1, NSNumber* obj2) {
    return [obj1 compare:obj2];
  };
  NSMutableArray* infos = [NSMutableArray array];
  for (NSNumber* key in
       [[tab_group_infos allKeys] sortedArrayUsingComparator:comparator]) {
    [infos addObject:tab_group_infos[key]];
  }

  // Remove the current item from the dictionary as the fetch is complete.
  if (group_item) {
    [group_item_fetches_
        removeObjectForKey:TabGroupIdentifierKeyforTabGroupItem(group_item)];
  }
  completion(group_item, infos);
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
