// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
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

void TabSnapshotAndFaviconConfigurator::FetchGroupTabInfoForTabGroupItem(
    TabGroupItem* group_item,
    WebStateList* web_state_list,
    void (^completion)(TabGroupItem* item, NSArray<GroupTabInfo*>* tab_infos)) {
  const TabGroup* tab_group = group_item.tabGroup;
  if (!tab_group || !web_state_list) {
    completion(group_item, nil);
    return;
  }

  // Overwrite any previous request for this item.
  NSUUID* request_id = [NSUUID UUID];
  group_item_fetches_[TabGroupIdentifierKeyforTabGroupItem(group_item)] =
      request_id;

  NSMutableDictionary<NSNumber*, GroupTabInfo*>* tab_group_infos =
      [[NSMutableDictionary alloc] init];
  NSUInteger first_index = tab_group->range().range_begin();
  NSUInteger number_of_requests = MIN(7, tab_group->range().count());

  for (NSUInteger request_index = 0; request_index < number_of_requests;
       request_index++) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(first_index + request_index);
    CHECK(web_state);
    FetchGroupTabInfoFromWebState(group_item, web_state, tab_group_infos,
                                  request_index, number_of_requests, request_id,
                                  completion);
  }
}

void TabSnapshotAndFaviconConfigurator::FetchSingleGroupTabInfoFromWebState(
    web::WebState* web_state,
    void (^completion)(GroupTabInfo* tab_info)) {
  auto inner_completion =
      ^(TabGroupItem* item, NSArray<GroupTabInfo*>* tab_infos) {
        CHECK(tab_infos.count == 1);
        completion([tab_infos firstObject]);
      };

  // For single tab fetches, we don't track them in `group_item_fetches_`
  // as they are not associated with a TabGroupItem that can be re-fetched.
  FetchGroupTabInfoFromWebState(
      /*group_item=*/nil, web_state,
      /*tab_group_infos=*/[[NSMutableDictionary alloc] init],
      /*request_index=*/0, /*number_of_requests=*/1, /*request_id=*/nil,
      inner_completion);
}

#pragma mark - Private

void TabSnapshotAndFaviconConfigurator::FetchGroupTabInfoFromWebState(
    TabGroupItem* group_item,
    web::WebState* web_state,
    NSMutableDictionary<NSNumber*, GroupTabInfo*>* tab_group_infos,
    NSUInteger request_index,
    NSUInteger number_of_requests,
    NSUUID* request_id,
    void (^completion)(TabGroupItem* item, NSArray<GroupTabInfo*>* tab_infos)) {
  GroupTabInfo* tab_info = [[GroupTabInfo alloc] init];

  // Fetch the snapshot.
  SnapshotTabHelper::FromWebState(web_state)->RetrieveColorSnapshot(
      ^(UIImage* snapshot) {
        // If there is no available snapshot, configure the snapshot to be an
        // empty image in order to pass `OnGroupTabInfoFromWebStateFetched`
        // checks.
        tab_info.snapshot = snapshot ?: [[UIImage alloc] init];
        OnGroupTabInfoFromWebStateFetched(group_item, tab_info, tab_group_infos,
                                          request_index, number_of_requests,
                                          request_id, completion);
      });

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  const GURL& url = web_state->GetVisibleURL();

  // Set the NTP favicon.
  if (IsUrlNtp(url)) {
    tab_info.favicon =
        CustomSymbolWithConfiguration(kChromeProductSymbol, configuration);
    OnGroupTabInfoFromWebStateFetched(group_item, tab_info, tab_group_infos,
                                      request_index, number_of_requests,
                                      request_id, completion);
    return;
  }

  // Set the default favicon.
  UIImage* default_favicon =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  if (!favicon_loader_) {
    tab_info.favicon = default_favicon;
    OnGroupTabInfoFromWebStateFetched(group_item, tab_info, tab_group_infos,
                                      request_index, number_of_requests,
                                      request_id, completion);
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
          tab_info.favicon = attributes.faviconImage;
        } else {
          tab_info.favicon = default_favicon;
        }
        OnGroupTabInfoFromWebStateFetched(group_item, tab_info, tab_group_infos,
                                          request_index, number_of_requests,
                                          request_id, completion);
      });
}

void TabSnapshotAndFaviconConfigurator::OnGroupTabInfoFromWebStateFetched(
    TabGroupItem* group_item,
    GroupTabInfo* tab_info,
    NSMutableDictionary<NSNumber*, GroupTabInfo*>* tab_group_infos,
    NSUInteger request_index,
    NSUInteger number_of_requests,
    NSUUID* request_id,
    void (^completion)(TabGroupItem* item, NSArray<GroupTabInfo*>* tab_infos)) {
  // Check if all data have been fetched before proceeding.
  if (!tab_info.snapshot || !tab_info.favicon) {
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

  tab_group_infos[@(request_index)] = tab_info;
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
