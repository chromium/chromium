// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "third_party/blink/public/mojom/navigation/system_entropy.mojom.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class RefCountedString;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class BackForwardCache;
class BrowserContext;
class NavigationEntry;
class RenderFrameHost;
class NavigationHandle;
struct OpenURLParams;

// A NavigationController manages session history, i.e., a back-forward list
// of navigation entries.
//
// FOR CONTENT EMBEDDERS: You can think of each WebContents as having one
// NavigationController. Technically, this is the NavigationController for
// the primary frame tree of the WebContents. See the comments for
// WebContents::GetPrimaryPage() for more about primary vs non-primary frame
// trees. This NavigationController is retrievable by
// WebContents::GetController(). It is the only one that affects the actual
// user-exposed session history list (e.g., via back/forward buttons). It is
// not intended to expose other NavigationControllers to the content/public
// API.
//
// FOR CONTENT INTERNALS: Be aware that NavigationControllerImpl is 1:1 with a
// FrameTree. With MPArch there can be multiple FrameTrees associated with a
// WebContents, so there can be multiple NavigationControllers associated with
// a WebContents. However only the primary one, and the
// NavigationEntries/events originating from it, is exposed to //content
// embedders. See docs/frame_trees.md for more details.
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
    // |virtual_url_for_special_cases|.
    LOAD_TYPE_DATA,

#if BUILDFLAG(IS_ANDROID)
    // Load a pdf page. Used on Android only.
    LOAD_TYPE_PDF_ANDROID
#endif

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
      std::optional<url::Origin> initiator_origin,
      std::optional<GURL> initiator_base_url,
      ui::PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  // Extra optional parameters for LoadURLWithParams.
  struct CONTENT_EXPORT LoadURLParams {
    explicit LoadURLParams(const GURL& url);

    // Copies |open_url_params| into LoadURLParams, attempting to copy all
    // fields that are present in both structs (some properties are ignored
    // because they are unique to LoadURLParams or OpenURLParams).
    explicit LoadURLParams(const OpenURLParams& open_url_params);
    LoadURLParams(const LoadURLParams&) = delete;
    LoadURLParams(LoadURLParams&&);
    LoadURLParams& operator=(const LoadURLParams&) = delete;
    LoadURLParams& operator=(LoadURLParams&&);
    ~LoadURLParams();

    // The url to load. This field is required.
    GURL url;

    // The frame token of the initiator of the navigation if the navigation was
    // initiated through trusted, non-web-influenced UI (e.g. via omnibox, the
    // bookmarks bar, local NTP, etc.). This frame is not guaranteed to exist at
    // any point during navigation. This can be an invalid id if the navigation
    // was not associated with a frame, or if the initiating frame did not exist
    // by the time navigation started. This parameter is defined if and only if
    // |initiator_process_id| below is.
    std::optional<blink::LocalFrameToken> initiator_frame_token;

    // ID of the renderer process of the frame host that initiated the
    // navigation. This is defined if and only if |initiator_frame_token| above
    // is, and it is only valid in conjunction with it.
    int initiator_process_id = ChildProcessHost::kInvalidUniqueID;

    // The origin of the initiator of the navigation or std::nullopt if the
    // navigation was initiated through trusted, non-web-influenced UI
    // (e.g. via omnibox, the bookmarks bar, local NTP, etc.).
    //
    // All renderer-initiated navigations must have a non-null
    // |initiator_origin|, but it is theoretically possible that some
    // browser-initiated navigations may also use a non-null |initiator_origin|
    // (if these navigations can be somehow triggered or influenced by web
    // content).
    std::optional<url::Origin> initiator_origin;

    // The base url of the initiator, to be passed to about:blank and srcdoc
    // frames. As with `initiator_origin`, some browser-initiated navigations
    // may not have an initiator, and in those cases this will be null. It will
    // also be null for non-about:blank/about:srcdoc navigations.
    std::optional<GURL> initiator_base_url;

    // SiteInstance of the frame that initiated the navigation or null if we
    // don't know it.
    scoped_refptr<SiteInstance> source_site_instance;

    // See LoadURLType comments above.
    LoadURLType load_type = LOAD_TYPE_DEFAULT;

    // PageTransition for this load. See PageTransition for details.
    // Note the default value in constructor below.
    ui::PageTransition transition_type = ui::PAGE_TRANSITION_LINK;

    // The browser-global FrameTreeNode ID for the frame to navigate, or the
    // default-constructed invalid value to indicate the main frame.
    FrameTreeNodeId frame_tree_node_id;

    // Referrer for this load. Empty if none.
    Referrer referrer;

    // Any redirect URLs that occurred for this navigation before |url|.
    // Defaults to an empty vector.
    std::vector<GURL> redirect_chain;

    // Extra headers for this load, separated by \n.
    std::string extra_headers;

    // True for renderer-initiated navigations. This is
    // important for tracking whether to display pending URLs.
    bool is_renderer_initiated = false;

    // Whether a navigation in a new window has the opener suppressed. False if
    // the navigation is not in a new window. Can only be true when
    // |is_renderer_initiated| is true.
    bool was_opener_suppressed = false;

    // User agent override for this load. See comments in
    // UserAgentOverrideOption definition.
    UserAgentOverrideOption override_user_agent = UA_OVERRIDE_INHERIT;

    // Used in LOAD_TYPE_DATA loads only. Used for specifying a base URL
    // for pages loaded via data URLs.
    GURL base_url_for_data_url;

    // Used in LOAD_TYPE_DATA and LOAD_TYPE_PDF_ANDROID loads only. URL
    // displayed to the user for data or pdf loads.
    GURL virtual_url_for_special_cases;

#if BUILDFLAG(IS_ANDROID)
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

    // Content type for a form submission for LOAD_TYPE_HTTP_POST.
    std::string post_content_type;

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
    blink::mojom::WasActivatedOption was_activated =
        blink::mojom::WasActivatedOption::kUnknown;

    // If this navigation was initiated from a link that specified the
    // hrefTranslate attribute, this contains the attribute's value (a BCP47
    // language code). Empty otherwise.
    std::string href_translate;

    // Indicates the reload type of this navigation.
    ReloadType reload_type = ReloadType::NONE;

    // Indicates the suggested system entropy captured when the navigation
    // began.
    blink::mojom::SystemEntropy suggested_system_entropy =
        blink::mojom::SystemEntropy::kNormal;

    // Indicates a form submission created this navigation.
    bool is_form_submission = false;

    // Impression info associated with this navigation. Should only be populated
    // for navigations originating from a link click.
    std::optional<blink::Impression> impression;

    // Download policy to be applied if this navigation turns into a download.
    blink::NavigationDownloadPolicy download_policy;

    // Common begin navigation status.
    blink::mojom::NavigationInitiatorActivationAndAdStatus
        initiator_activation_and_ad_status =
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kDidNotStartWithTransientActivation;

    // Indicates that this navigation is for PDF content in a renderer.
    bool is_pdf = false;

    // Indicates this navigation should use a new BrowsingInstance. For example,
    // this is used in web platform tests to guarantee that each test starts in
    // a fresh BrowsingInstance.
    bool force_new_browsing_instance = false;

    // True if the initiator explicitly asked for opener relationships to be
    // preserved, via rel="opener".
    bool has_rel_opener = false;

    // True if the navigation should not be upgraded to HTTPS. This should only
    // be set in very specific circumstances like navigations to captive portal
    // login URLs which may be broken by HTTPS Upgrades due to the portal's
    // unconventional handling of HTTPS URLs.
    bool force_no_https_upgrade = false;
  };

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  CONTENT_EXPORT static void DisablePromptOnRepost();

  virtual ~NavigationController() = default;

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

  // Active entry --------------------------------------------------------------

  // THIS IS DEPRECATED. DO NOT USE. Use GetVisibleEntry instead.
  // See http://crbug.com/273710.
  //
  // Returns the active entry, which is the pending entry if a navigation is in
  // progress or the last committed entry otherwise.
  virtual NavigationEntry* GetActiveEntry() = 0;

  // Returns the entry that should be displayed to the user in the address bar.
  // This is the pending entry if a navigation is in progress *and* is safe to
  // display to the user (see below), or the last committed entry otherwise.
  //
  // A pending entry is safe to display if it started in the browser process or
  // if it's a renderer-initiated navigation in a new tab which hasn't been
  // accessed by another tab.  (If it has been accessed, it risks a URL spoof.)
  virtual NavigationEntry* GetVisibleEntry() = 0;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  virtual int GetCurrentEntryIndex() = 0;

  // Returns the last "committed" entry. Note that even when no navigation has
  // actually committed, this will never return null as long as the FrameTree
  // associated with the NavigationController is already initialized, as a
  // FrameTree will always start with the initial NavigationEntry.
  virtual NavigationEntry* GetLastCommittedEntry() = 0;

  // Returns the index of the last committed entry.
  virtual int GetLastCommittedEntryIndex() = 0;

  // Returns true if the source for the current entry can be viewed.
  virtual bool CanViewSource() = 0;

  // Navigation list -----------------------------------------------------------

  // Returns the number of entries in the NavigationController, excluding
  // the pending entry if there is one.
  virtual int GetEntryCount() = 0;

  virtual NavigationEntry* GetEntryAtIndex(int index) = 0;

  // Returns the entry at the specified offset from current.  Returns nullptr
  // if out of bounds.
  virtual NavigationEntry* GetEntryAtOffset(int offset) = 0;

  // Pending entry -------------------------------------------------------------

  // Discards the pending entry if any.
  virtual void DiscardNonCommittedEntries() = 0;

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  virtual NavigationEntry* GetPendingEntry() = 0;

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  virtual int GetPendingEntryIndex() = 0;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL, specifying extra http headers to add to the
  // request. Extra headers are separated by \n.
  //
  // Returns NavigationHandle for the initiated navigation (might be null if
  // the navigation couldn't be started for some reason). WeakPtr is used as if
  // the navigation is cancelled before it reaches DidStartNavigation, the
  // WebContentsObserver::DidFinishNavigation callback won't be dispatched.
  virtual base::WeakPtr<NavigationHandle> LoadURL(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition type,
      const std::string& extra_headers) = 0;

  // More general version of LoadURL. See comments in LoadURLParams for
  // using |params|.
  virtual base::WeakPtr<NavigationHandle> LoadURLWithParams(
      const LoadURLParams& params) = 0;

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet or if the load was
  // explicitly requested using SetNeedsReload().
  virtual void LoadIfNecessary() = 0;

  // Reloads the current entry using the original URL used to create it. This is
  // used for cases where the user wants to refresh a page using a different
  // user agent after following a redirect. It is also used in the case of an
  // intervention (e.g., preview) being served on the page and the user
  // requesting the page without the intervention.
  //
  // If the current entry's original URL matches the current URL, is invalid, or
  // contains POST data, this will result in a normal reload rather than an
  // attempt to load the original URL.
  virtual void LoadOriginalRequestURL() = 0;

  // Navigates directly to an error page in response to an event on the last
  // committed page (e.g., triggered by a subresource), with |error_page_html|
  // as the contents and |url| as the URL.
  //
  // The error page will create a NavigationEntry that temporarily replaces the
  // original page's entry. The original entry will be put back into the entry
  // list after any other navigation.
  //
  // Returns the handle to the navigation for the error page, which may be null
  // if the navigation is immediately canceled.
  virtual base::WeakPtr<NavigationHandle> LoadPostCommitErrorPage(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      const std::string& error_page_html) = 0;

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  virtual bool CanGoBack() = 0;
  virtual bool CanGoForward() = 0;
  virtual bool CanGoToOffset(int offset) = 0;
  // `CanGoBack`/`CanGoForward` are preconditions for these respective methods.
  virtual void GoBack() = 0;
  virtual void GoForward() = 0;

  // Navigates to the specified absolute index. Should only be used for
  // browser-initiated navigations.
  virtual void GoToIndex(int index) = 0;

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  virtual void GoToOffset(int offset) = 0;

  // Reloads the current entry under the specified ReloadType.  If
  // |check_for_repost| is true and the current entry has POST data the user is
  // prompted to see if they really want to reload the page.  In nearly all
  // cases pass in true in production code, but would do false for testing, or
  // in cases where no user interface is available for prompting.
  // NOTE: |reload_type| should never be NONE.
  virtual void Reload(ReloadType reload_type, bool check_for_repost) = 0;

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  If the index is the last
  // committed index or the pending entry, this does nothing and returns false.
  // Otherwise this call discards any pending entry.
  virtual bool RemoveEntryAtIndex(int index) = 0;

  // Discards any pending entry, then discards all entries after the current
  // entry index.
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

  // Returns whether it is safe to call PruneAllButLastCommitted. There must be
  // a last committed entry, and if there is a pending entry, it must be new and
  // not an existing entry.
  //
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

  // Gets the BackForwardCache for this NavigationController.
  virtual BackForwardCache& GetBackForwardCache() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class NavigationControllerImpl;
  NavigationController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
