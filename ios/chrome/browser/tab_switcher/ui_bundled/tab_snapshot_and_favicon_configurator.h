// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"

class FaviconLoader;
class SnapshotBrowserAgent;
@class TabGroupItem;
@class TabGroupItemFetchInfo;
@class TabSnapshotAndFavicon;
class WebStateList;
@class WebStateTabSwitcherItem;

namespace web {
class WebState;
}  // namespace web

// Configures favicon and snapshot for tab items.
class TabSnapshotAndFaviconConfigurator {
 public:
  // Callbacks invoked when fetching the data is complete.
  using Completion = void (^)(TabSnapshotAndFavicon* result);
  using CompletionWithTabGroupItem = void (^)(TabGroupItem* tab_group,
                                              NSInteger tab_index,
                                              TabSnapshotAndFavicon* result);
  using CompletionWithTabSwitcherItem = void (^)(WebStateTabSwitcherItem* item,
                                                 TabSnapshotAndFavicon* result);

  // The `favicon_loader` can be `nullptr` to disable fetching favicons via
  // Google servers.
  explicit TabSnapshotAndFaviconConfigurator(
      FaviconLoader* favicon_loader,
      SnapshotBrowserAgent* snapshot_browser_agent);
  TabSnapshotAndFaviconConfigurator(const TabSnapshotAndFaviconConfigurator&) =
      delete;
  TabSnapshotAndFaviconConfigurator& operator=(
      const TabSnapshotAndFaviconConfigurator&) = delete;
  ~TabSnapshotAndFaviconConfigurator();

  // Fetches snapshots and favicons for all tabs within a `tab_group_item`
  // and its associated `web_state_list` asynchronously.
  // The `completion` block is invoked twice per tabIndex: once when the
  // snapshot has been fetched, and again when the favicon has been fetched.
  void FetchSnapshotAndFaviconForTabGroupItem(
      TabGroupItem* group_item,
      WebStateList* web_state_list,
      CompletionWithTabGroupItem completion);

  // Fetches the snapshot and favicon for a single `web_state` asynchronously.
  // The `completion` block is invoked twice: once when the snapshot has been
  // fetched, and again when the favicon has been fetched.
  void FetchSingleSnapshotAndFaviconFromWebState(web::WebState* web_state,
                                                 Completion completion);

  // Fetches the snapshot and favicon for `tab_item`.
  // The `completion` block is invoked twice: once when the snapshot has been
  // fetched, and again when the favicon has been fetched.
  void FetchSnapshotAndFaviconForTabSwitcherItem(
      WebStateTabSwitcherItem* tab_item,
      CompletionWithTabSwitcherItem completion);

  // Fetches the favicon for `tab_item`.
  // The `completion` block is invoked when the favicon has been fetched.
  // The snapshot is not fetched and always nil.
  void FetchFaviconForTabSwitcherItem(WebStateTabSwitcherItem* tab_item,
                                      CompletionWithTabSwitcherItem completion);

 private:
  // Store information about a specific fetch request (used to implement
  // FetchSnapshotAndFaviconInternal and abstract the difference between
  // the two methods using it).
  class RequestInfo;
  class TabGroupItemRequestInfo;
  class TabSwitcherItemRequestInfo;

  // Initiates the asynchronous fetching of a snapshot and favicon for a
  // single `web_state` belonging to a `tab_group_item`.
  void FetchSnapshotAndFaviconFromWebState(
      TabGroupItem* group_item,
      web::WebState* web_state,
      NSInteger request_index,
      NSUUID* request_id,
      CompletionWithTabGroupItem completion);

  // Fetches the snapshot and favicon for `tab_item`.
  // If `fetch_snapshot` is false, only the favicon will be fetched.
  // The `completion` block is invoked twice: once when the snapshot
  // has been fetched (if requested), and again when the favicon has been
  // fetched.
  void FetchSnapshotAndFaviconForTabSwitcherItem(
      WebStateTabSwitcherItem* tab_item,
      bool fetch_snapshot,
      CompletionWithTabSwitcherItem completion);

  // Fetches the snapshot and favicon according to `request_info`.
  // Used by the methods FetchSnapshotAndFaviconFromWebState() and
  // FetchSnapshotAndFaviconForTabSwitcherItem().
  void FetchSnapshotAndFaviconInternal(const RequestInfo& request_info,
                                       Completion completion);

  // Called when a snapshot or favicon for a web state has been fetched.
  // This updates the fetch status and executes the `completion` block.
  void OnSnapshotAndFaviconFromWebStateFetched(
      TabGroupItem* group_item,
      NSInteger request_index,
      NSUUID* request_id,
      CompletionWithTabGroupItem completion,
      TabSnapshotAndFavicon* tab_snapshot_and_favicon);

  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
  raw_ptr<SnapshotBrowserAgent, DanglingUntriaged> snapshot_browser_agent_ =
      nullptr;

  // Stores the TabGroupItemFetchInfo of in-progress TabGroupItem fetch
  // requests, keyed by the item's tabGroupIdentifier. This is used to cancel
  // previous fetches if a new one starts for the same item.
  NSMutableDictionary<NSValue*, TabGroupItemFetchInfo*>* group_item_fetches_;

  base::WeakPtrFactory<TabSnapshotAndFaviconConfigurator> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_CONFIGURATOR_H_
