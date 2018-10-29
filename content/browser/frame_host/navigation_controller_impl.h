// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_controller_delegate.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/reload_type.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {
enum class WasActivatedOption;
class FrameTreeNode;
class RenderFrameHostImpl;
class NavigationEntryScreenshotManager;
class SiteInstance;
struct LoadCommittedDetails;

class CONTENT_EXPORT NavigationControllerImpl : public NavigationController {
 public:
  NavigationControllerImpl(
      NavigationControllerDelegate* delegate,
      BrowserContext* browser_context);
  ~NavigationControllerImpl() override;

  // NavigationController implementation:
  WebContents* GetWebContents() const override;
  BrowserContext* GetBrowserContext() const override;
  void Restore(int selected_navigation,
               RestoreType type,
               std::vector<std::unique_ptr<NavigationEntry>>* entries) override;
  NavigationEntryImpl* GetActiveEntry() const override;
  NavigationEntryImpl* GetVisibleEntry() const override;
  int GetCurrentEntryIndex() const override;
  NavigationEntryImpl* GetLastCommittedEntry() const override;
  int GetLastCommittedEntryIndex() const override;
  bool CanViewSource() const override;
  int GetEntryCount() const override;
  NavigationEntryImpl* GetEntryAtIndex(int index) const override;
  NavigationEntryImpl* GetEntryAtOffset(int offset) const override;
  void DiscardNonCommittedEntries() override;
  NavigationEntryImpl* GetPendingEntry() const override;
  int GetPendingEntryIndex() const override;
  NavigationEntryImpl* GetTransientEntry() const override;
  void SetTransientEntry(std::unique_ptr<NavigationEntry> entry) override;
  void LoadURL(const GURL& url,
               const Referrer& referrer,
               ui::PageTransition type,
               const std::string& extra_headers) override;
  void LoadURLWithParams(const LoadURLParams& params) override;
  void LoadIfNecessary() override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  void GoToIndex(int index) override;
  void GoToOffset(int offset) override;
  bool RemoveEntryAtIndex(int index) override;
  const SessionStorageNamespaceMap& GetSessionStorageNamespaceMap()
      const override;
  SessionStorageNamespace* GetDefaultSessionStorageNamespace() override;
  bool NeedsReload() const override;
  void SetNeedsReload() override;
  void CancelPendingReload() override;
  void ContinuePendingReload() override;
  bool IsInitialNavigation() const override;
  bool IsInitialBlankNavigation() const override;
  void Reload(ReloadType reload_type, bool check_for_repost) override;
  void NotifyEntryChanged(const NavigationEntry* entry) override;
  void CopyStateFrom(const NavigationController& source,
                     bool needs_reload) override;
  void CopyStateFromAndPrune(NavigationController* source,
                             bool replace_entry) override;
  bool CanPruneAllButLastCommitted() override;
  void PruneAllButLastCommitted() override;
  void DeleteNavigationEntries(
      const DeletionPredicate& deletionPredicate) override;

  // Starts a navigation in a newly created subframe as part of a history
  // navigation. Returns true if the history navigation could start, false
  // otherwise.  If this returns false, the caller should do a regular
  // navigation to |default_url| should be done instead.
  bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      const GURL& default_url);

  // Called when a document requests a navigation through a
  // RenderFrameProxyHost.
  void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      bool is_renderer_initiated,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  void ClearAllScreenshots() override;

  // Whether this is the initial navigation in an unmodified new tab.  In this
  // case, we know there is no content displayed in the page.
  bool IsUnmodifiedBlankTab() const;

  // The session storage namespace that all child RenderViews belonging to
  // |instance| should use.
  SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance);

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const NavigationEntryImpl* entry) const;

  // Return the index of the entry with the given unique id, or -1 if not found.
  int GetEntryIndexWithUniqueID(int nav_entry_id) const;

  // Return the entry with the given unique id, or null if not found.
  NavigationEntryImpl* GetEntryWithUniqueID(int nav_entry_id) const;

  NavigationControllerDelegate* delegate() const {
    return delegate_;
  }

  // For use by WebContentsImpl ------------------------------------------------

  // Allow renderer-initiated navigations to create a pending entry when the
  // provisional load starts.
  void SetPendingEntry(std::unique_ptr<NavigationEntryImpl> entry);

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the WebContentsImpl.
  //
  // If a new entry is created, it will return true and will have filled the
  // given details structure and broadcast the NOTIFY_NAV_ENTRY_COMMITTED
  // notification. The caller can then use the details without worrying about
  // listening for the notification.
  //
  // In the case that nothing has changed, the details structure is undefined
  // and it will return false.
  bool RendererDidNavigate(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      LoadCommittedDetails* details,
      bool is_same_document_navigation,
      NavigationHandleImpl* navigation_handle);

  // Notifies us that we just became active. This is used by the WebContentsImpl
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

  // Returns true if the given URL would be a same-document navigation (e.g., if
  // the reference fragment is different, or after a pushState) from the last
  // committed URL in the specified frame. If there is no last committed entry,
  // then nothing will be same-document.
  //
  // Special note: if the URLs are the same, it does NOT automatically count as
  // a same-document navigation. Neither does an input URL that has no ref, even
  // if the rest is the same. This may seem weird, but when we're considering
  // whether a navigation happened without loading anything, the same URL could
  // be a reload, while only a different ref would be in-page (pages can't clear
  // refs without reload, only change to "#" which we don't count as empty).
  //
  // The situation is made murkier by history.replaceState(), which could
  // provide the same URL as part of a same-document navigation, not a reload.
  // So we need to let the (untrustworthy) renderer resolve the ambiguity, but
  // only when the URLs are on the same origin. We rely on |origin|, which
  // matters in cases like about:blank that otherwise look cross-origin.
  bool IsURLSameDocumentNavigation(const GURL& url,
                                   const url::Origin& origin,
                                   bool renderer_says_same_document,
                                   RenderFrameHost* rfh) const;

  // Sets the SessionStorageNamespace for the given |partition_id|. This is
  // used during initialization of a new NavigationController to allow
  // pre-population of the SessionStorageNamespace objects. Session restore,
  // prerendering, and the implementaion of window.open() are the primary users
  // of this API.
  //
  // Calling this function when a SessionStorageNamespace has already been
  // associated with a |partition_id| will CHECK() fail.
  void SetSessionStorageNamespace(
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace);

  // Random data ---------------------------------------------------------------

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count_for_testing(size_t max_entry_count) {
    max_entry_count_for_testing_ = max_entry_count;
  }
  static size_t max_entry_count();

  void SetGetTimestampCallbackForTest(
      const base::Callback<base::Time()>& get_timestamp_callback);

  // Takes a screenshot of the page at the current state.
  void TakeScreenshot();

  // Sets the screenshot manager for this NavigationControllerImpl. Setting a
  // NULL manager recreates the default screenshot manager and uses that.
  void SetScreenshotManager(
      std::unique_ptr<NavigationEntryScreenshotManager> manager);

  // Discards only the pending entry. |was_failure| should be set if the pending
  // entry is being discarded because it failed to load.
  void DiscardPendingEntry(bool was_failure);

  // Sets a flag on the pending NavigationEntryImpl instance if any that the
  // navigation failed due to an SSL error.
  void SetPendingNavigationSSLError(bool error);

// Returns true if the string corresponds to a valid data URL, false
// otherwise.
#if defined(OS_ANDROID)
  static bool ValidateDataURLAsString(
      const scoped_refptr<const base::RefCountedString>& data_url_as_string);
#endif

 private:
  friend class RestoreHelper;

  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest,
                           PurgeScreenshot);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, Basic);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, SingleDuplicate);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ManyDuplicates);
  FRIEND_TEST_ALL_PREFIXES(TimeSmoother, ClockBackwardsJump);

  // Helper class to smooth out runs of duplicate timestamps while still
  // allowing time to jump backwards.
  class CONTENT_EXPORT TimeSmoother {
   public:
    // Returns |t| with possibly some time added on.
    base::Time GetSmoothedTime(base::Time t);

   private:
    // |low_water_mark_| is the first time in a sequence of adjusted
    // times and |high_water_mark_| is the last.
    base::Time low_water_mark_;
    base::Time high_water_mark_;
  };

  // Starts a navigation to an already existing pending NavigationEntry.
  void NavigateToExistingPendingEntry(ReloadType reload_type);

  // Recursively identifies which frames need to be navigated for a navigation
  // to |pending_entry_|, starting at |frame| and exploring its children.
  // |same_document_loads| and |different_document_loads| will be filled with
  // the NavigationRequests needed to navigate to |pending_entry_|.
  void FindFramesToNavigate(
      FrameTreeNode* frame,
      ReloadType reload_type,
      std::vector<std::unique_ptr<NavigationRequest>>* same_document_loads,
      std::vector<std::unique_ptr<NavigationRequest>>*
          different_document_loads);

  // Starts a new navigation based on |load_params|, that doesn't correspond to
  // an exisiting NavigationEntry.
  void NavigateWithoutEntry(const LoadURLParams& load_params);

  // Handles a navigation to a renderer-debug URL.
  void HandleRendererDebugURL(FrameTreeNode* frame_tree_node, const GURL& url);

  // Creates and returns a NavigationEntry based on |load_params| for a
  // navigation in |node|.
  // |override_user_agent|, |should_replace_current_entry| and
  // |has_user_gesture| will override the values from |load_params|. The same
  // values should be passed to CreateNavigationRequestFromLoadParams.
  std::unique_ptr<NavigationEntryImpl> CreateNavigationEntryFromLoadParams(
      FrameTreeNode* node,
      const LoadURLParams& load_params,
      bool override_user_agent,
      bool should_replace_current_entry,
      bool has_user_gesture);

  // Creates and returns a NavigationRequest based on |load_params| for a
  // new navigation in |node|.
  // Will return nullptr if the parameters are invalid and the navigation cannot
  // start.
  // |override_user_agent|, |should_replace_current_entry| and
  // |has_user_gesture| will override the values from |load_params|. The same
  // values should be passed to CreateNavigationEntryFromLoadParams.
  // TODO(clamy): Remove the dependency on NavigationEntry and
  // FrameNavigationEntry.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestFromLoadParams(
      FrameTreeNode* node,
      const LoadURLParams& load_params,
      bool override_user_agent,
      bool should_replace_current_entry,
      bool has_user_gesture,
      ReloadType reload_type,
      const NavigationEntryImpl& entry,
      FrameNavigationEntry* frame_entry);

  // Creates and returns a NavigationRequest for a navigation to |entry|. Will
  // return nullptr if the parameters are invalid and the navigation cannot
  // start.
  // TODO(clamy): Ensure this is only called for navigations to existing
  // NavigationEntries.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestFromEntry(
      FrameTreeNode* frame_tree_node,
      const NavigationEntryImpl& entry,
      FrameNavigationEntry* frame_entry,
      ReloadType reload_type,
      bool is_same_document_history_load,
      bool is_history_navigation_in_new_child);

  // Returns whether there is a pending NavigationEntry whose unique ID matches
  // the given NavigationHandle's pending_nav_entry_id.
  bool PendingEntryMatchesHandle(NavigationHandleImpl* handle) const;

  // Classifies the given renderer navigation (see the NavigationType enum).
  NavigationType ClassifyNavigation(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) const;

  // Handlers for the different types of navigation types. They will actually
  // handle the navigations corresponding to the different NavClasses above.
  // They will NOT broadcast the commit notification, that should be handled by
  // the caller.
  //
  // RendererDidNavigateAutoSubframe is special, it may not actually change
  // anything if some random subframe is loaded. It will return true if anything
  // changed, or false if not.
  //
  // The NewPage and NewSubframe functions take in |replace_entry| to pass to
  // InsertOrReplaceEntry, in case the newly created NavigationEntry is meant to
  // replace the current one (e.g., for location.replace or successful loads
  // after net errors), in contrast to updating a NavigationEntry in place
  // (e.g., for history.replaceState).
  void RendererDidNavigateToNewPage(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_same_document,
      bool replace_entry,
      NavigationHandleImpl* handle);
  void RendererDidNavigateToExistingPage(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_same_document,
      bool was_restored,
      NavigationHandleImpl* handle);
  void RendererDidNavigateToSamePage(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      NavigationHandleImpl* handle);
  void RendererDidNavigateNewSubframe(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_same_document,
      bool replace_entry);
  bool RendererDidNavigateAutoSubframe(
      RenderFrameHostImpl* rfh,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(LoadCommittedDetails* details);

  // Updates the virtual URL of an entry to match a new URL, for cases where
  // the real renderer URL is derived from the virtual URL, like view-source:
  void UpdateVirtualURLToURL(NavigationEntryImpl* entry,
                             const GURL& new_url);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents.
  void FinishRestore(int selected_index, RestoreType type);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  void InsertOrReplaceEntry(std::unique_ptr<NavigationEntryImpl> entry,
                            bool replace);

  // Removes the entry at |index|, as long as it is not the current entry.
  void RemoveEntryAtIndexInternal(int index);

  // Discards both the pending and transient entries.
  void DiscardNonCommittedEntriesInternal();

  // Discards only the transient entry.
  void DiscardTransientEntry();

  // If we have the maximum number of entries, remove the oldest one in
  // preparation to add another.
  void PruneOldestEntryIfFull();

  // Removes all entries except the last committed entry.  If there is a new
  // pending navigation it is preserved. In contrast to
  // PruneAllButLastCommitted() this does not update the session history of the
  // RenderView.  Callers must ensure that |CanPruneAllButLastCommitted| returns
  // true before calling this.
  void PruneAllButLastCommittedInternal();

  // Inserts up to |max_index| entries from |source| into this. This does NOT
  // adjust any of the members that reference entries_
  // (last_committed_entry_index_, pending_entry_index_ or
  // transient_entry_index_).
  void InsertEntriesFrom(const NavigationControllerImpl& source, int max_index);

  // Returns the navigation index that differs from the current entry by the
  // specified |offset|.  The index returned is not guaranteed to be valid.
  int GetIndexForOffset(int offset) const;

  // ---------------------------------------------------------------------------

  // The user browser context associated with this controller.
  BrowserContext* browser_context_;

  // List of |NavigationEntry|s for this controller.
  std::vector<std::unique_ptr<NavigationEntryImpl>> entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // != -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  NavigationEntryImpl* pending_entry_;

  // If a new entry fails loading, details about it are temporarily held here
  // until the error page is shown (or 0 otherwise).
  //
  // TODO(avi): We need a better way to handle the connection between failed
  // loads and the subsequent load of the error page. This current approach has
  // issues: 1. This might hang around longer than we'd like if there is no
  // error page loaded, and 2. This doesn't work very well for frames.
  // http://crbug.com/474261
  int failed_pending_entry_id_;

  // The index of the currently visible entry.
  int last_committed_entry_index_;

  // The index of the pending entry if it is in entries_, or -1 if
  // pending_entry_ is a new entry (created by LoadURL).
  int pending_entry_index_;

  // The index for the entry that is shown until a navigation occurs.  This is
  // used for interstitial pages. -1 if there are no such entry.
  // Note that this entry really appears in the list of entries, but only
  // temporarily (until the next navigation).  Any index pointing to an entry
  // after the transient entry will become invalid if you navigate forward.
  int transient_entry_index_;

  // The delegate associated with the controller. Possibly NULL during
  // setup.
  NavigationControllerDelegate* delegate_;

  // Manages the SSL security UI.
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_;

  // Whether this is the initial navigation.
  // Becomes false when initial navigation commits.
  bool is_initial_navigation_;

  // Prevent unsafe re-entrant calls to NavigateToPendingEntry.
  bool in_navigate_to_pending_entry_;

  // Used to find the appropriate SessionStorageNamespace for the storage
  // partition of a NavigationEntry.
  //
  // A NavigationController may contain NavigationEntries that correspond to
  // different StoragePartitions. Even though they are part of the same
  // NavigationController, only entries in the same StoragePartition may
  // share session storage state with one another.
  SessionStorageNamespaceMap session_storage_namespace_map_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_for_testing_;

  // If a repost is pending, its type (RELOAD or RELOAD_BYPASSING_CACHE),
  // NO_RELOAD otherwise.
  ReloadType pending_reload_;

  // Used to get timestamps for newly-created navigation entries.
  base::Callback<base::Time()> get_timestamp_callback_;

  // Used to smooth out timestamps from |get_timestamp_callback_|.
  // Without this, whenever there is a run of redirects or
  // code-generated navigations, those navigations may occur within
  // the timer resolution, leading to things sometimes showing up in
  // the wrong order in the history view.
  TimeSmoother time_smoother_;

  std::unique_ptr<NavigationEntryScreenshotManager> screenshot_manager_;

  // Used for tracking consecutive reload requests.  If the last user-initiated
  // navigation (either browser-initiated or renderer-initiated with a user
  // gesture) was a reload, these hold the ReloadType and timestamp.  Otherwise
  // these are ReloadType::NONE and a null timestamp, respectively.
  ReloadType last_committed_reload_type_;
  base::Time last_committed_reload_time_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_IMPL_H_
