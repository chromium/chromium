// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"

class FaviconLoader;
@class TabGroupItem;
@class TabSnapshotAndFavicon;
class WebStateList;
@class WebStateTabSwitcherItem;

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
  void FetchSnapshotAndFaviconForTabGroupItem(
      TabGroupItem* group_item,
      WebStateList* web_state_list,
      void (^completion)(
          TabGroupItem* item,
          NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons));

  // Fetches the snapshot and favicon for a single `web_state` asynchronously.
  // The `completion` block is called once the information for the web state
  // has been fetched.
  void FetchSingleSnapshotAndFaviconFromWebState(
      web::WebState* web_state,
      void (^completion)(TabSnapshotAndFavicon* tab_snapshot_and_favicon));

  // Fetches the snapshot and favicon for `tab_item`.
  // The `completion` block is invoked twice: once when the snapshot has been
  // fetched, and again when the favicon has been fetched.
  void FetchSnapshotAndFaviconForTabSwitcherItem(
      WebStateTabSwitcherItem* tab_item,
      void (^completion)(WebStateTabSwitcherItem* item,
                         TabSnapshotAndFavicon* tab_snapshot_and_favicon));

  // Fetches the favicon for `tab_item`.
  // The `completion` block is invoked when the favicon has been fetched.
  // The snapshot is not fetched and always nil.
  void FetchFaviconForTabSwitcherItem(
      WebStateTabSwitcherItem* tab_item,
      void (^completion)(WebStateTabSwitcherItem* item,
                         TabSnapshotAndFavicon* tab_snapshot));

 private:
  // Initiates the asynchronous fetching of a snapshot and favicon for a
  // single `web_state` belonging to a `tab_group_item`.
  void FetchSnapshotAndFaviconFromWebState(
      TabGroupItem* group_item,
      web::WebState* web_state,
      NSMutableDictionary<NSNumber*, TabSnapshotAndFavicon*>*
          tab_snapshots_and_favicons,
      NSUInteger request_index,
      NSUInteger number_of_requests,
      NSUUID* request_id,
      void (^completion)(
          TabGroupItem* item,
          NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons));

  // Called when the snapshot and/or favicon for a web state has been fetched.
  // Checks if all information has been collected and calls the `completion`
  // block once, with all the information.
  void OnSnapshotAndFaviconFromWebStateFetched(
      TabGroupItem* group_item,
      TabSnapshotAndFavicon* tab_snapshot_and_favicon,
      NSMutableDictionary<NSNumber*, TabSnapshotAndFavicon*>*
          tab_snapshots_and_favicons,
      NSUInteger request_index,
      NSUInteger number_of_requests,
      NSUUID* request_id,
      void (^completion)(
          TabGroupItem* item,
          NSArray<TabSnapshotAndFavicon*>* tab_snapshots_and_favicons));

  // Fetches the snapshot and favicon for `tab_item`.
  // If `fetch_snapshot` is false, only the favicon will be fetched.
  // The `completion` block is invoked twice: once when the snapshot
  // has been fetched (if requested), and again when the favicon has been
  // fetched.
  void FetchSnapshotAndFaviconForTabSwitcherItem(
      WebStateTabSwitcherItem* tab_item,
      bool fetch_snapshot,
      void (^completion)(WebStateTabSwitcherItem* item,
                         TabSnapshotAndFavicon* tab_snapshot_and_favicon));

  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;

  // Stores the UUIDs of in-progress TabGroupItem fetch requests, keyed by the
  // item's tabGroupIdentifier. This is used to cancel previous fetches if a new
  // one starts for the same item.
  NSMutableDictionary<NSValue*, NSUUID*>* group_item_fetches_;
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
