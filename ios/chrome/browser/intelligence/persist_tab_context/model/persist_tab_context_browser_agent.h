// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_

#import "base/files/file_path.h"
#import "base/scoped_multi_source_observation.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_state_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/web/public/web_state_observer.h"

class GURL;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

class PageContentCacheService;
@class PersistTabContextStateObserver;

// PersistTabContextBrowserAgent allows saving and retrieving saved page
// contexts. Page contexts are retrieved and saved to storage when a tab is
// backgrounded (either switched tab or backgrounded app). The page contexts
// that are stored contain information on tab content such as the APC and the
// inner_text, along with the page title and url. Persisted page contexts are
// invalidated and deleted from storage when a tab commits a navigation, when a
// tab is closed, or when their TTL expires, and are validated against the live
// WebState's last committed URL when read.
//
// *NOTE*: We do not store snapshots for the persisted tab contexts, since that
// would be duplicately stored. Those need to be fetched separately.
class PersistTabContextBrowserAgent
    : public BrowserUserData<PersistTabContextBrowserAgent>,
      public web::WebStateObserver,
      public TabsDependencyInstaller {
 public:
  PersistTabContextBrowserAgent(const PersistTabContextBrowserAgent&) = delete;
  PersistTabContextBrowserAgent& operator=(
      const PersistTabContextBrowserAgent&) = delete;

  ~PersistTabContextBrowserAgent() override;

  // Type alias for the result map of GetContextsAsync. Maps each webstate
  // unique id to an optional unique pointer holding the fetched
  // page context. A `std::nullopt` value indicates that no context was found
  // for the given ID.
  using PageContextMap = std::map<
      std::string,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>;

  // Asynchronously fetches a single page context associated with the given
  // `webstate_unique_id`.
  void GetSingleContextAsync(
      const std::string& webstate_unique_id,
      base::OnceCallback<void(std::optional<std::unique_ptr<
                                  optimization_guide::proto::PageContext>>)>
          callback);

  // Asynchronously fetches multiple page contexts for the provided vector of
  // `webstate_unique_ids`.
  void GetMultipleContextsAsync(
      const std::vector<std::string>& webstate_unique_ids,
      base::OnceCallback<void(PageContextMap)> callback);

  // TabsDependencyInstaller
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // WebStateObserver:
  void WasHidden(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  friend class PersistTabContextBrowserAgentTest;
  friend class BrowserUserData<PersistTabContextBrowserAgent>;

  explicit PersistTabContextBrowserAgent(Browser* browser);

  // Type alias for the intermediate result used in the barrier callback. Holds
  // a Tab ID and it's associated page context.
  using ContextPair = std::pair<
      std::string,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>>;

  // Aggregates results from the barrier callback and runs the final callback.
  void OnAllContextsRetrieved(
      base::OnceCallback<void(PageContextMap)> final_callback,
      std::vector<ContextPair> results);

  // Adapts the result of a single fetch to include its ID, then triggers the
  // barrier callback.
  void OnSingleContextRetrieved(
      std::string web_state_id,
      base::RepeatingCallback<void(ContextPair)> barrier_callback,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
          result);

  // Extracts and stores the page context.
  void ExtractAndStoreContext(web::WebState* web_state);

  // Called whenever scene activation level changed. This is explicitly used to
  // capture the event where a tab is hidden due to the app being backgrounded,
  // in which case the page context should be retrieved.
  void OnSceneActivationLevelChanged(SceneActivationLevel level);

  // Private callback for PageContextWrapper.
  void OnPageContextExtracted(base::WeakPtr<web::WebState> weak_web_state,
                              const GURL& expected_url,
                              PageContextWrapperCallbackResponse response);

  // Writes the page context to the PageContentCache.
  void WriteContextToContentCache(web::WebState* web_state,
                                  PageContextWrapperCallbackResponse response);

  // Deletes the persisted context for `web_state` from active storage.
  void DeleteContextForWebState(web::WebState* web_state);

  // Deletes the persisted context for `tab_id` from active storage.
  void DeleteContextById(int64_t tab_id);

  // Validates that `context` corresponds to a live WebState whose last
  // committed URL matches the cached URL. Fails closed by deleting the cached
  // entry and returning `std::nullopt` if the WebState no longer exists or its
  // URL does not match.
  std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
  ValidateReadContext(
      const std::string& webstate_unique_id,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
          context);

  // Validates a single read `context` and runs `callback`.
  void OnSingleContextRead(
      const std::string& webstate_unique_id,
      base::OnceCallback<void(std::optional<std::unique_ptr<
                                  optimization_guide::proto::PageContext>>)>
          callback,
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>
          context);

  // Validates all entries in `result_map` and runs `callback`.
  void OnMultipleContextsRead(base::OnceCallback<void(PageContextMap)> callback,
                              PageContextMap result_map);

  // Deletes a page context from the PageContentCache.
  void DeleteContextFromContentCache(int64_t tab_id);

  // Reads and parses a page context from the PageContentCache.
  void ReadAndParseContextFromContentCache(
      const std::string& webstate_unique_id,
      base::OnceCallback<void(std::optional<std::unique_ptr<
                                  optimization_guide::proto::PageContext>>)>
          callback);

  // The service's PageContext wrapper.
  __strong PageContextWrapper* page_context_wrapper_;

  // Manages this object as an observer of realized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // The service's PersistTabContextStateAgent. The state agent is used to
  // observe scene state changes such as the application being backgrounded, in
  // which case the last tab's page context should be saved to storage.
  __strong PersistTabContextStateAgent* persist_tab_context_state_agent_;

  // Schedules a background task to delete the legacy directory used for
  // filesystem storage.
  void RemoveFilesystemStorage();

  // Schedules a background task to delete the legacy directory used for SQLite
  // storage.
  void RemoveSqliteStorage();

  // Runs cleanup tasks on the PageContentCache.
  void RunCacheCleanup();

  // Profile-specific file path to store page contexts at in the app's cache.
  base::FilePath storage_directory_path_;

  // The sequenced task runner ensures that async tasks that are posted to it
  // execute in the same order as they were scheduled. This will be used to
  // ensure the creation of the user-specific directory is done before a write,
  // read or delete operation can be attempted. The task runner is created with
  // MayBlock, BEST_EFFORT task priority and SKIP_ON_SHUTDOWN task shutdown
  // behaviour.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The keyed service used to access the PageContentCache in.
  raw_ptr<PageContentCacheService> page_content_cache_service_ = nullptr;

  // Use PageContentCache for storage as opposed to the direct filesystem.
  const bool use_page_content_cache_;

  // Extract page context on page load of the visible tab (in addition to on
  // page hidden).
  const bool extract_context_on_page_load_;

  // Store page innerText only; don't store APC.
  const bool store_inner_text_only_;

  base::WeakPtrFactory<PersistTabContextBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PERSIST_TAB_CONTEXT_BROWSER_AGENT_H_
