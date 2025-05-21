// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"

class FaviconLoader;
@class GroupTabInfo;
@class TabGroupItem;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Configures favicon and snapshot for tab items.
class TabSnapshotAndFaviconConfigurator {
 public:
  // The `favicon_loader` can be `nullptr` to disable fetching favicons via
  // Google servers.
  explicit TabSnapshotAndFaviconConfigurator(FaviconLoader* favicon_loader);
  TabSnapshotAndFaviconConfigurator(const TabSnapshotAndFaviconConfigurator&) =
      delete;
  TabSnapshotAndFaviconConfigurator& operator=(
      const TabSnapshotAndFaviconConfigurator&) = delete;
  ~TabSnapshotAndFaviconConfigurator();

  // Fetches snapshots and favicons for all tabs within a `tab_group_item`
  // and its associated `web_state_list` asynchronously.
  // The `completion` block is called once all information for the tabs
  // in the group has been fetched.
  void FetchGroupTabInfoForTabGroupItem(
      TabGroupItem* group_item,
      WebStateList* web_state_list,
      void (^completion)(TabGroupItem* item,
                         NSArray<GroupTabInfo*>* tab_infos));

  // Fetches the snapshot and favicon for a single `web_state` asynchronously.
  // The `completion` block is called once the information for the web state
  // has been fetched.
  void FetchSingleGroupTabInfoFromWebState(
      web::WebState* web_state,
      void (^completion)(GroupTabInfo* tab_info));

  // TODO(crbug.com/400966281): Add function to fetch snapshot and favicon for
  // TabSwitcherItem.

 private:
  // Initiates the asynchronous fetching of a snapshot and favicon for a
  // single `web_state` belonging to a `tab_group_item`.
  void FetchGroupTabInfoFromWebState(
      TabGroupItem* group_item,
      web::WebState* web_state,
      NSMutableDictionary<NSNumber*, GroupTabInfo*>* tab_group_infos,
      NSUInteger request_index,
      NSUInteger number_of_requests,
      NSUUID* request_id,
      void (^completion)(TabGroupItem* item,
                         NSArray<GroupTabInfo*>* tab_infos));

  // Called when the snapshot and/or favicon for a single web state has been
  // fetched. Checks if all information has been collected and calls
  // the `completion` block.
  void OnGroupTabInfoFromWebStateFetched(
      TabGroupItem* group_item,
      GroupTabInfo* tab_info,
      NSMutableDictionary<NSNumber*, GroupTabInfo*>* tab_group_infos,
      NSUInteger request_index,
      NSUInteger number_of_requests,
      NSUUID* request_id,
      void (^completion)(TabGroupItem* item,
                         NSArray<GroupTabInfo*>* tab_infos));

  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;

  // Stores the UUIDs of in-progress TabGroupItem fetch requests, keyed by the
  // item's tabGroupIdentifier. This is used to cancel previous fetches if a new
  // one starts for the same item.
  NSMutableDictionary<NSValue*, NSUUID*>* group_item_fetches_;
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
