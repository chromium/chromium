// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "content/public/common/was_activated_option.mojom.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {

class RefCountedString;

}  // namespace base

namespace content {

class BackForwardCache;
class BrowserContext;
class NavigationEntry;
class RenderFrameHost;
class WebContents;
struct OpenURLParams;

// A NavigationController maintains the back-forward list for a WebContents and
// manages all navigation within that list.
//
// Each NavigationController belongs to one WebContents; each WebContents has
// exactly one NavigationController.
class NavigationController {
 public:
  using DeletionPredicate =
      base::RepeatingCallback<bool(content::NavigationEntry* entry)>;

  // Load type used in LoadURLParams.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.content_public.browser.navigation_controller)
  // GENERATED_JAVA_PREFIX_TO_STRIP: LOAD_TYPE_
  enum LoadURLType {
    // For loads that do not fall into any types below.
    LOAD_TYPE_DEFAULT,

    // An http post load request.  The post data is passed in |post_data|.
    LOAD_TYPE_HTTP_POST,

    // Loads a 'data:' scheme URL with specified base URL and a history entry
    // URL. This is only safe to be used for browser-initiated data: URL
    // navigations, since it shows arbitrary content as if it comes from
    // |virtual_url_for_data_url|.
    LOAD_TYPE_DATA

    // Adding new LoadURLType? Also update LoadUrlParams.java static constants.
  };

  // User agent override type used in LoadURLParams.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.content_public.browser.navigation_controller)
  // GENERATED_JAVA_PREFIX_TO_STRIP: UA_OVERRIDE_
  enum UserAgentOverrideOption {
    // Use the override value from the previous NavigationEntry in the
    // NavigationController.
    UA_OVERRIDE_INHERIT,

    // Use the default user agent.
    UA_OVERRIDE_FALSE,

    // Use the user agent override, if it's available.
    UA_OVERRIDE_TRUE

    // Adding new UserAgentOverrideOption? Also update LoadUrlParams.java
    // static constants.
  };

  // Creates a navigation entry and translates the virtual url to a real one.
  // This is a general call; prefer LoadURL[WithParams] below.
  // Extra headers are separated by \n.
  CONTENT_EXPORT static std::unique_ptr<NavigationEntry> CreateNavigationEntry(
      const GURL& url,
      Referrer referrer,
      base::Optional<url::Origin> initiator_origin,
      ui::PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  // Extra optional parameters for LoadURLWithParams.
  struct CONTENT_EXPORT LoadURLParams {
    // The url to load. This field is required.
    GURL url;

    // The origin of the initiator of the navigation or base::nullopt if the
    // navigation was initiated through through trusted, non-web-influenced UI
    // (e.g. via omnibox, the bookmarks bar, local NTP, etc.).
    //
    // All renderer-initiated navigations must have a non-null
    // |initiator_origin|, but it is theoretically possible that some
    // browser-initiated navigations may also use a non-null |initiator_origin|
    // (if these navigations can be somehow triggered or influenced by web
    // content).
    base::Optional<url::Origin> initiator_origin;

    // SiteInstance of the frame that initiated the navigation or null if we
    // don't know it.
    scoped_refptr<SiteInstance> source_site_instance;

    // See LoadURLType comments above.
    LoadURLType load_type = LOAD_TYPE_DEFAULT;

    // PageTransition for this load. See PageTransition for details.
    // Note the default value in constructor below.
    ui::PageTransition transition_type = ui::PAGE_TRANSITION_LINK;

    // The browser-global FrameTreeNode ID for the frame to navigate, or
    // RenderFrameHost::kNoFrameTreeNodeId for the main frame.
    int frame_tree_node_id = RenderFrameHost::kNoFrameTreeNodeId;

    // Referrer for this load. Empty if none.
    Referrer referrer;

    // Any redirect URLs that occurred for this navigation before |url|.
    // Defaults to an empty vector.
    std::vector<GURL> redirect_chain;

    // Extra headers for this load, separated by \n.
    std::string extra_headers;

    // True for renderer-initiated navigations. This is
    // important for tracking whether to display pending URLs.
    bool is_renderer_initiated;

    // User agent override for this load. See comments in
    // UserAgentOverrideOption definition.
    UserAgentOverrideOption override_user_agent = UA_OVERRIDE_INHERIT;

    // Used in LOAD_TYPE_DATA loads only. Used for specifying a base URL
    // for pages loaded via data URLs.
    GURL base_url_for_data_url;

    // Used in LOAD_TYPE_DATA loads only. URL displayed to the user for
    // data loads.
    GURL virtual_url_for_data_url;

#if defined(OS_ANDROID)
    // Used in LOAD_TYPE_DATA loads only. The real data URI is represented
    // as a string to circumvent the restriction on GURL size. This is only
    // needed to pass URLs that exceed the IPC limit (kMaxURLChars). Short
    // data: URLs can be passed in the |url| field.
    scoped_refptr<base::RefCountedString> data_url_as_string;
#endif

    // Used in LOAD_TYPE_HTTP_POST loads only. Carries the post data of the
    // load.  Ownership is transferred to NavigationController after
    // LoadURLWithParams call.
    scoped_refptr<network::ResourceRequestBody> post_data;

    // True if this URL should be able to access local resources.
    bool can_load_local_resources = false;

    // Indicates whether this navigation should replace the current
    // navigation entry.
    bool should_replace_current_entry = false;

    // Used to specify which frame to navigate. If empty, the main frame is
    // navigated. This is currently only used in tests.
    std::string frame_name;

    // Indicates that the navigation was triggered by a user gesture.
    bool has_user_gesture = false;

    // Indicates that during this navigation, the session history should be
    // cleared such that the resulting page is the first and only entry of the
    // session history.
    //
    // The clearing is done asynchronously, and completes when this navigation
    // commits.
    bool should_clear_history_list = false;

    // Indicates whether or not this navigation was initiated via context menu.
    bool started_from_context_menu = false;

    // Optional URLLoaderFactory to facilitate navigation to a blob URL.
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;

    // This value should only be set for main frame navigations. Subframe
    // navigations will always get their NavigationUIData from
    // ContentBrowserClient::GetNavigationUIData.
    std::unique_ptr<NavigationUIData> navigation_ui_data;

    // Whether this navigation was triggered by a x-origin redirect following a
    // prior (most likely <a download>) download attempt.
    bool from_download_cross_origin_redirect = false;

    // Time at which the input leading to this navigation occurred. This field
    // is set for links clicked by the user; the embedder is recommended to set
    // it for navigations it initiates.
    base::TimeTicks input_start;

    // Set to |kYes| if the navigation should propagate user activation. This
    // is used by embedders where the activation has occurred outside the page.
    mojom::WasActivatedOption was_activated =
        mojom::WasActivatedOption::kUnknown;

    // If this navigation was initiated from a link that specified the
    // hrefTranslate attribute, this contains the attribute's value (a BCP47
    // language code). Empty otherwise.
    std::string href_translate;

    // Indicates the reload type of this navigation.
    ReloadType reload_type = ReloadType::NONE;

    explicit LoadURLParams(const GURL& url);

    // Copies |open_url_params| into LoadURLParams, attempting to copy all
    // fields that are present in both structs (some properties are ignored
    // because they are unique to LoadURLParams or OpenURLParams).
    explicit LoadURLParams(const OpenURLParams& open_url_params);

    ~LoadURLParams();

    DISALLOW_COPY_AND_ASSIGN(LoadURLParams);
  };

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  CONTENT_EXPORT static void DisablePromptOnRepost();

  virtual ~NavigationController() {}

  // Returns the web contents associated with this controller. It can never be
  // nullptr.
  virtual WebContents* GetWebContents() = 0;

  // Get the browser context for this controller. It can never be nullptr.
  virtual BrowserContext* GetBrowserContext() = 0;

  // Initializes this NavigationController with the given saved navigations,
  // using |selected_navigation| as the currently loaded entry. Before this call
  // the controller should be unused (there should be no current entry). |type|
  // indicates where the restor comes from. This takes ownership of the
  // NavigationEntrys in |entries| and clears it out. This is used for session
  // restore.
  virtual void Restore(
      int selected_navigation,
      RestoreType type,
      std::vector<std::unique_ptr<NavigationEntry>>* entries) = 0;

  // Entries -------------------------------------------------------------------

  // There are two basic states for entries: pending and committed. When an
  // entry is navigated to, a request is sent to the server. While that request
  // has not been responded to, the NavigationEntry is pending. Once data is
  // received for that entry, that NavigationEntry is committed.

  // A transient entry is an entry that, when the user navigates away, is
  // removed and discarded rather than being added to the back-forward list.
  // Transient entries are useful for interstitial pages and the like.

  // Active entry --------------------------------------------------------------

  // THIS IS DEPRECATED. DO NOT USE. Use GetVisibleEntry instead.
  // See http://crbug.com/273710.
  //
  // Returns the active entry, which is the transient entry if any, the pending
  // entry if a navigation is in progress or the last committed entry otherwise.
  // NOTE: This can be nullptr!!
  virtual NavigationEntry* GetActiveEntry() = 0;

  // Returns the entry that should be displayed to the user in the address bar.
  // This is the transient entry if any, the pending entry if a navigation is
  // in progress *and* is safe to display to the user (see below), or the last
  // committed entry otherwise.
  // NOTE: This can be nullptr if no entry has committed!
  //
  // A pending entry is safe to display if it started in the browser process or
  // if it's a renderer-initiated navigation in a new tab which hasn't been
  // accessed by another tab.  (If it has been accessed, it risks a URL spoof.)
  virtual NavigationEntry* GetVisibleEntry() = 0;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  virtual int GetCurrentEntryIndex() = 0;

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  virtual NavigationEntry* GetLastCommittedEntry() = 0;

  // Returns the index of the last committed entry.  It will be -1 if there are
  // no entries, or if there is a transient entry before the first entry
  // commits.
  virtual int GetLastCommittedEntryIndex() = 0;

  // Returns true if the source for the current entry can be viewed.
  virtual bool CanViewSource() = 0;

  // Navigation list -----------------------------------------------------------

  // Returns the number of entries in the NavigationController, excluding
  // the pending entry if there is one, but including the transient entry if
  // any.
  virtual int GetEntryCount() = 0;

  virtual NavigationEntry* GetEntryAtIndex(int index) = 0;

  // Returns the entry at the specified offset from current.  Returns nullptr
  // if out of bounds.
  virtual NavigationEntry* GetEntryAtOffset(int offset) = 0;

  // Pending entry -------------------------------------------------------------

  // Discards the pending and transient entries if any.
  virtual void DiscardNonCommittedEntries() = 0;

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  virtual NavigationEntry* GetPendingEntry() = 0;

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  virtual int GetPendingEntryIndex() = 0;

  // Transient entry -----------------------------------------------------------

  // Returns the transient entry if any. This is an entry which is removed and
  // discarded if any navigation occurs. Note that the returned entry is owned
  // by the navigation controller and may be deleted at any time.
  virtual NavigationEntry* GetTransientEntry() = 0;

  // Adds an entry that is returned by GetActiveEntry(). The entry is
  // transient: any navigation causes it to be removed and discarded.  The
  // NavigationController becomes the owner of |entry| and deletes it when
  // it discards it. This is useful with interstitial pages that need to be
  // represented as an entry, but should go away when the user navigates away
  // from them.
  // Note that adding a transient entry does not change the active contents.
  virtual void SetTransientEntry(std::unique_ptr<NavigationEntry> entry) = 0;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL, specifying extra http headers to add to the
  // request.  Extra headers are separated by \n.
  virtual void LoadURL(const GURL& url,
                       const Referrer& referrer,
                       ui::PageTransition type,
                       const std::string& extra_headers) = 0;

  // More general version of LoadURL. See comments in LoadURLParams for
  // using |params|.
  virtual void LoadURLWithParams(const LoadURLParams& params) = 0;

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet or if the load was
  // explicitly requested using SetNeedsReload().
  virtual void LoadIfNecessary() = 0;

  // Navigates directly to an error page in response to an event on the last
  // committed page (e.g., triggered by a subresource), with |error_page_html|
  // as the contents and |url| as the URL.

  // The error page will create a NavigationEntry that temporarily replaces the
  // original page's entry. The original entry will be put back into the entry
  // list after any other navigation.
  virtual void LoadPostCommitErrorPage(RenderFrameHost* render_frame_host,
                                       const GURL& url,
                                       const std::string& error_page_html,
                                       net::Error error) = 0;

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  virtual bool CanGoBack() = 0;
  virtual bool CanGoForward() = 0;
  virtual bool CanGoToOffset(int offset) = 0;
  virtual void GoBack() = 0;
  virtual void GoForward() = 0;

  // Navigates to the specified absolute index.
  virtual void GoToIndex(int index) = 0;

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  virtual void GoToOffset(int offset) = 0;

  // Reloads the current entry under the specified ReloadType.  If
  // |check_for_repost| is true and the current entry has POST data the user is
  // prompted to see if they really want to reload the page.  In nearly all
  // cases pass in true in production code, but would do false for testing, or
  // in cases where no user interface is available for prompting.  If a
  // transient entry is showing, initiates a new navigation to its URL.
  // NOTE: |reload_type| should never be NONE.
  virtual void Reload(ReloadType reload_type, bool check_for_repost) = 0;

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  If the index is the last
  // committed index or the pending entry, this does nothing and returns false.
  // Otherwise this call discards any transient or pending entries.
  virtual bool RemoveEntryAtIndex(int index) = 0;

  // Discards any transient or pending entries, then discards all entries after
  // the current entry index.
  virtual void PruneForwardEntries() = 0;

  // Random --------------------------------------------------------------------

  // Session storage depends on dom_storage that depends on blink::WebString.
  // Returns all the SessionStorageNamespace objects that this
  // NavigationController knows about, the map key is a StoragePartition id.
  virtual const SessionStorageNamespaceMap& GetSessionStorageNamespaceMap() = 0;

  // TODO(ajwong): Remove this once prerendering, instant, and session restore
  // are migrated.
  virtual SessionStorageNamespace* GetDefaultSessionStorageNamespace() = 0;

  // Returns true if a reload happens when activated (SetActive(true) is
  // invoked). This is true for session/tab restore, cloned tabs and tabs that
  // requested a reload (using SetNeedsReload()) after their renderer was
  // killed.
  virtual bool NeedsReload() = 0;

  // Request a reload to happen when activated. This can be used when a renderer
  // backing a background tab is killed by the system on Android or ChromeOS.
  virtual void SetNeedsReload() = 0;

  // Cancels a repost that brought up a warning.
  virtual void CancelPendingReload() = 0;
  // Continues a repost that brought up a warning.
  virtual void ContinuePendingReload() = 0;

  // Returns true if this is a newly created tab or a cloned tab, which has not
  // yet committed a real page. Returns false after the initial navigation has
  // committed.
  virtual bool IsInitialNavigation() = 0;

  // Returns true if this is a newly created tab (not a clone) that has not yet
  // committed a real page.
  virtual bool IsInitialBlankNavigation() = 0;

  // Broadcasts the NOTIFICATION_NAV_ENTRY_CHANGED notification for the given
  // entry. This will keep things in sync like the saved session.
  virtual void NotifyEntryChanged(NavigationEntry* entry) = 0;

  // Copies the navigation state from the given controller to this one. This one
  // should be empty (just created). |needs_reload| indicates whether a reload
  // needs to happen when activated. If false, the WebContents remains unloaded
  // and is painted as a plain grey rectangle when activated. To force a reload,
  // call SetNeedsReload() followed by LoadIfNecessary().
  virtual void CopyStateFrom(NavigationController* source,
                             bool needs_reload) = 0;

  // A variant of CopyStateFrom. Removes all entries from this except the last
  // committed entry, and inserts all entries from |source| before and including
  // its last committed entry. For example:
  // source: A B *C* D
  // this:   E F *G*
  // result: A B C *G*
  // If there is a pending entry after *G* in |this|, it is also preserved.
  // If |replace_entry| is true, the current entry in |source| is replaced. So
  // the result above would be A B *G*.
  // This ignores any pending or transient entries in |source|.  Callers must
  // ensure that |CanPruneAllButLastCommitted| returns true before calling this,
  // or it will crash.
  virtual void CopyStateFromAndPrune(NavigationController* source,
                                     bool replace_entry) = 0;

  // Returns whether it is safe to call PruneAllButLastCommitted or
  // CopyStateFromAndPrune.  There must be a last committed entry, no transient
  // entry, and if there is a pending entry, it must be new and not an existing
  // entry.
  //
  // If there were no last committed entry, the pending entry might not commit,
  // leaving us with a blank page.  This is unsafe when used with
  // |CopyStateFromAndPrune|, which would show an existing entry above the blank
  // page.
  // If there were a transient entry, we would not want to prune the other
  // entries, which the transient entry could be referring to.
  // If there were an existing pending entry, we could not prune the last
  // committed entry, in case it did not commit.  That would leave us with no
  // sensible place to put the pending entry when it did commit, after all other
  // entries are pruned.  For example, it could be going back several entries.
  // (New pending entries are safe, because they can always commit to the end.)
  virtual bool CanPruneAllButLastCommitted() = 0;

  // Removes all the entries except the last committed entry. If there is a new
  // pending navigation it is preserved.  Callers must ensure
  // |CanPruneAllButLastCommitted| returns true before calling this, or it will
  // crash.
  virtual void PruneAllButLastCommitted() = 0;

  // Removes all navigation entries matching |deletionPredicate| except the last
  // commited entry.
  // Callers must ensure |CanPruneAllButLastCommitted| returns true before
  // calling this, or it will crash.
  virtual void DeleteNavigationEntries(
      const DeletionPredicate& deletionPredicate) = 0;

  // Returns whether entry at the given index is marked to be skipped on
  // back/forward UI. The history manipulation intervention marks entries to be
  // skipped in order to intervene against pages that manipulate browser history
  // such that the user is not able to use the back button to go to the previous
  // page they interacted with.
  virtual bool IsEntryMarkedToBeSkipped(int index) = 0;

  // Gets the BackForwardCache for this NavigationController.
  virtual BackForwardCache& GetBackForwardCache() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class NavigationControllerImpl;
  NavigationController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
