// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_MANAGER_IMPL_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <set>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_shared_tabs_manager.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_id.h"

class PrefService;
class SnapshotBrowserAgent;
class WebStateList;

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

namespace web {
class WebState;
}  // namespace web

@class GeminiPageContext;
@class UIImage;

// Manages the active and inactive shared tabs attached to the Gemini session,
// including partial and asynchronous full page context retrieval.
class GeminiSharedTabsManagerImpl : public GeminiSharedTabsManager,
                                    public WebStateListObserver {
 public:
  // Creates a manager for the shared tabs in `web_state_list`, using
  // `pref_service` to observe page content sharing preference changes,
  // `persist_tab_context_browser_agent` for cached page context lookups,
  // `snapshot_browser_agent` for snapshot retrieval,
  // `is_gemini_invoked_callback` to query whether Gemini is currently invoked,
  // and `is_tab_grid_visible_callback` to query whether the tab grid is
  // currently visible.
  GeminiSharedTabsManagerImpl(
      WebStateList* web_state_list,
      PrefService* pref_service,
      PersistTabContextBrowserAgent* persist_tab_context_browser_agent,
      SnapshotBrowserAgent* snapshot_browser_agent,
      base::RepeatingCallback<bool()> is_gemini_invoked_callback,
      base::RepeatingCallback<bool()> is_tab_grid_visible_callback);

  GeminiSharedTabsManagerImpl(const GeminiSharedTabsManagerImpl&) = delete;
  GeminiSharedTabsManagerImpl& operator=(const GeminiSharedTabsManagerImpl&) =
      delete;

  ~GeminiSharedTabsManagerImpl() override;

  // Invalidates pending callbacks, stops observations, clears shared tabs, and
  // resets references to dependencies.
  void Shutdown();

  // GeminiSharedTabsManager:
  NSArray<GeminiPageContext*>* GetInactiveSharedTabs() const override;
  void SaveActivePageContextToSharedTabs(
      GeminiPageContext* active_page_context) override;
  void UpdateSharedTabsForActiveWebState(
      web::WebState* active_web_state) override;
  void DetachTabWithID(NSString* tab_id) override;
  void UpdateLocalTabAttachmentState(
      NSString* tab_id,
      ios::provider::GeminiPageContextAttachmentState new_state) override;
  NSUInteger SharedTabsCount() const override;
  void OnTabPickerSelectionChanged(
      std::set<web::WebStateID> selected_tabs) override;
  std::set<web::WebStateID> GetSelectedTabsForTabPicker() const override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

 private:
  friend class GeminiBrowserAgentTest;

  using SharedTabsList =
      std::vector<std::pair<web::WebStateID, __strong GeminiPageContext*>>;

  // Removes `web_state` from the shared tabs when it is detached or replaced.
  void OnWebStateRemoved(web::WebState* web_state);

  // Updates or refetches `old_active`'s shared page context when the active
  // web state changes.
  void OnActiveWebStateChanged(web::WebState* old_active);

  // Clears all shared tabs when the page content sharing preference is
  // disabled.
  void OnPageContentPrefChanged();

  // Returns whether there is at least one inactive shared tab in
  // `shared_tabs_`.
  bool HasInactiveSharedTabs() const;

  // Returns the `WebStateID` of the active web state in `web_state_list_`, or
  // an invalid `WebStateID` if none exists.
  web::WebStateID GetActiveWebStateID() const;

  // Generates partial page contexts for `tabs_to_fetch` and triggers async
  // full page context retrieval for them. Page contexts are inserted directly
  // into `shared_tabs_`.
  void UpdateSharedTabContexts(
      const std::vector<web::WebStateID>& tabs_to_fetch);

  // Creates a partial `GeminiPageContext` synchronously for `web_state`.
  GeminiPageContext* CreatePartialPageContext(web::WebState* web_state);

  // Called when a cached page context lookup for `contexts_map` has completed.
  void OnPersistTabContextLookupComplete(
      PersistTabContextBrowserAgent::PageContextMap contexts_map);

  // Retrieves a color snapshot for `selected_tab` and attaches it to cached
  // `proto_context`, calling `OnFullPageContextAvailableForSharedTab` when
  // complete.
  void RetrieveCachedPageContextForTab(
      web::WebStateID selected_tab,
      std::unique_ptr<optimization_guide::proto::PageContext> proto_context);

  // Called when `snapshot` retrieval completes for `selected_tab` with
  // `proto_context`.
  void OnSnapshotRetrievedForTab(
      web::WebStateID selected_tab,
      std::unique_ptr<optimization_guide::proto::PageContext> proto_context,
      UIImage* snapshot);

  // Called when PNG base64 encoding (`encoded_snapshot`) completes for
  // `selected_tab` with `proto_context`.
  void OnSnapshotEncodedForTab(
      web::WebStateID selected_tab,
      std::unique_ptr<optimization_guide::proto::PageContext> proto_context,
      std::string encoded_snapshot);

  // Creates the full `GeminiPageContext` from `proto_context` for
  // `selected_tab` and forwards it to
  // `OnFullPageContextAvailableForSharedTab`.
  void CompleteCachedPageContextForTab(
      web::WebStateID selected_tab,
      std::unique_ptr<optimization_guide::proto::PageContext> proto_context);

  // Asynchronously generates full page context for `selected_tab` and calls
  // `OnFullPageContextAvailableForSharedTab` when successful.
  void GenerateFullPageContextForTab(web::WebStateID selected_tab);

  // Called when `full_page_context` for `web_state_id` becomes available.
  void OnFullPageContextAvailableForSharedTab(
      web::WebStateID web_state_id,
      GeminiPageContext* full_page_context);

  // Returns the shared `GeminiPageContext` for `tab_id`, or `nil` if not found.
  GeminiPageContext* GetSharedPageContext(web::WebStateID tab_id) const;

  // Adds or updates `page_context` for `tab_id` in `shared_tabs_`, preserving
  // the insertion order if `tab_id` already exists.
  void SetSharedPageContext(web::WebStateID tab_id,
                            GeminiPageContext* page_context);

  // Removes the entry for `tab_id` from `shared_tabs_`.
  void RemoveSharedPageContext(web::WebStateID tab_id);

  // The list of web states whose shared contexts are managed.
  raw_ptr<WebStateList> web_state_list_ = nullptr;

  // Registrar for page content sharing pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Scoped observation of `web_state_list_`.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // Browser agent used to look up cached page contexts for shared tabs.
  raw_ptr<PersistTabContextBrowserAgent> persist_tab_context_browser_agent_ =
      nullptr;

  // Browser agent used to retrieve color snapshots for shared tabs.
  raw_ptr<SnapshotBrowserAgent> snapshot_browser_agent_ = nullptr;

  // Callback returning whether Gemini (floaty or container) is currently
  // invoked.
  base::RepeatingCallback<bool()> is_gemini_invoked_callback_;

  // Callback returning whether the tab grid is currently visible.
  base::RepeatingCallback<bool()> is_tab_grid_visible_callback_;

  // The active and inactive shared tabs currently attached to the session,
  // represented by a list of `WebStateID` and `GeminiPageContext` pairs in
  // insertion order.
  SharedTabsList shared_tabs_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer factory for asynchronous context callbacks.
  base::WeakPtrFactory<GeminiSharedTabsManagerImpl> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_MANAGER_IMPL_H_
