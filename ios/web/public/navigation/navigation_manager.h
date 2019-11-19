// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_MANAGER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_MANAGER_H_

#include <stddef.h>

#include "base/callback.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/deprecated/navigation_item_list.h"
#include "ios/web/public/navigation/browser_url_rewriter.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ui/base/page_transition_types.h"

@class NSDictionary;
@class NSData;

namespace web {

class BrowserState;
class NavigationItem;
class WebState;

// A NavigationManager maintains the back-forward list for a WebState and
// manages all navigation within that list.
//
// Each NavigationManager belongs to one WebState; each WebState has
// exactly one NavigationManager.
class NavigationManager {
 public:
  // User agent override option used in LoadURLParams.
  enum class UserAgentOverrideOption : short {
    // Inherit the user agent type from the last committed non-native item if
    // there is one, otherwise keep the default type, which is NONE for native
    // item and MOBILE for non-native item.
    INHERIT = 0,

    // Use the mobile user agent.
    MOBILE,

    // Use the desktop user agent.
    DESKTOP,
  };

  // Parameters for URL loading. Most parameters are optional, and can be left
  // at the default values set by the constructor.
  struct WebLoadParams {
   public:
    // The URL to load. Must be set.
    GURL url;

    // The URL to display in Omnibox. If empty, |url| will be displayed.
    GURL virtual_url;

    // The referrer for the load. May be empty.
    Referrer referrer;

    // The transition type for the load. Defaults to PAGE_TRANSITION_LINK.
    ui::PageTransition transition_type;

    // The user agent override option for the load. Defualts to INHERIT.
    UserAgentOverrideOption user_agent_override_option;

    // True for renderer-initiated navigations. This is
    // important for tracking whether to display pending URLs.
    bool is_renderer_initiated;

    // Any extra HTTP headers to add to the load.
    NSDictionary* extra_headers;

    // Any post data to send with the load. When setting this, you should
    // generally set a Content-Type header as well.
    NSData* post_data;

    // Create a new WebLoadParams with the given URL and defaults for all other
    // parameters.
    explicit WebLoadParams(const GURL& url);
    ~WebLoadParams();

    // Allow copying WebLoadParams.
    WebLoadParams(const WebLoadParams& other);
    WebLoadParams& operator=(const WebLoadParams& other);
  };

  virtual ~NavigationManager() {}

  // Gets the BrowserState associated with this NavigationManager. Can never
  // return null.
  virtual BrowserState* GetBrowserState() const = 0;

  // Gets the WebState associated with this NavigationManager.
  virtual WebState* GetWebState() const = 0;

  // Returns the NavigationItem that should be used when displaying info about
  // the current item to the user. It ignores certain pending entries, to
  // prevent spoofing attacks using slow-loading navigations.
  virtual NavigationItem* GetVisibleItem() const = 0;

  // Returns the last committed NavigationItem, which may be null if there
  // are no committed entries or session restoration is in-progress.
  virtual NavigationItem* GetLastCommittedItem() const = 0;

  // Returns the pending item corresponding to the navigation that is currently
  // in progress, or null if there is none.
  virtual NavigationItem* GetPendingItem() const = 0;

  // Returns the transient item if any. This is an item which is removed and
  // discarded if any navigation occurs. Note that the returned item is owned
  // by the navigation manager and may be deleted at any time.
  virtual NavigationItem* GetTransientItem() const = 0;

  // Removes the transient and pending NavigationItems.
  virtual void DiscardNonCommittedItems() = 0;

  // Loads the URL with specified |params|.
  virtual void LoadURLWithParams(
      const NavigationManager::WebLoadParams& params) = 0;

  // Loads the current page in the following cases:
  //  - NavigationManager was restored from history and the current page has not
  //    loaded yet.
  //  - Renderer process has crashed.
  //  - Web usage was disabled and re-enabled.
  virtual void LoadIfNecessary() = 0;

  // Adds |rewriter| to a transient list of URL rewriters.  Transient URL
  // rewriters will be executed before the rewriters already added to the
  // BrowserURLRewriter singleton, and the list will be cleared after the next
  // attempted page load.  |rewriter| must not be null.
  virtual void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) = 0;

  // Returns the number of items in the NavigationManager, excluding
  // pending and transient entries.
  // TODO(crbug.com/533848): Update to return size_t.
  virtual int GetItemCount() const = 0;

  // Returns the committed NavigationItem at |index|.
  virtual NavigationItem* GetItemAtIndex(size_t index) const = 0;

  // Returns the index of |item| in the committed session history.
  virtual int GetIndexOfItem(const NavigationItem* item) const = 0;

  // Returns the index of the last committed item or -1 if the last
  // committed item correspond to a new navigation  or session restoration is
  // in-progress.
  // TODO(crbug.com/533848): Update to return size_t.
  virtual int GetLastCommittedItemIndex() const = 0;

  // Returns the index of the pending item or -1 if the pending item
  // corresponds to a new navigation.
  // TODO(crbug.com/533848): Update to return size_t.
  virtual int GetPendingItemIndex() const = 0;

  // Removes the item at the specified |index|.  If the index is the last
  // committed index or the pending item, this does nothing and returns false.
  // Otherwise this call discards any transient or pending entries.
  // TODO(crbug.com/533848): Update to use size_t.
  virtual bool RemoveItemAtIndex(int index) = 0;

  // Navigation relative to the current item.
  virtual bool CanGoBack() const = 0;
  virtual bool CanGoForward() const = 0;
  virtual bool CanGoToOffset(int offset) const = 0;
  virtual void GoBack() = 0;
  virtual void GoForward() = 0;

  // Navigates to the specified absolute index.
  // TODO(crbug.com/533848): Update to use size_t.
  virtual void GoToIndex(int index) = 0;

  // Reloads the visible item under the specified ReloadType. If
  // |check_for_repost| is true and the current item has POST data the user is
  // prompted to see if they really want to reload the page. Pass in true if the
  // reload is explicitly initiated by the user. If a transient item is showing,
  // initiates a new navigation to its URL.
  // TODO(crbug.com/700958): implement the logic for |check_for_repost|.
  virtual void Reload(ReloadType reload_type, bool check_for_repost) = 0;

  // Reloads the visible item under the specified UserAgentType.
  // TODO(crbug.com/738020): combine both Reload() implementations.
  virtual void ReloadWithUserAgentType(UserAgentType user_agent_type) = 0;

  // Returns a list of all non-redirected NavigationItems whose index precedes
  // or follows the current index.
  virtual NavigationItemList GetBackwardItems() const = 0;
  virtual NavigationItemList GetForwardItems() const = 0;

  // Initializes this NavigationManager with the given saved navigations, using
  // |last_committed_item_index| as the currently loaded item. Before this call
  // the NavigationManager should be unused (there should be no current item).
  // This takes ownership of |items| (must be moved).
  virtual void Restore(int last_committed_item_index,
                       std::vector<std::unique_ptr<NavigationItem>> items) = 0;

  // Returns true after session restoration has started, until the first
  // post-restore navigation is started.
  virtual bool IsRestoreSessionInProgress() const = 0;

  // Registers a callback to be run when session restoration is completed.
  // If there is no in-progress session restoration, the callback is run
  // immediately.
  // TODO(crbug.com/904502): This API is only needed for clearing cookies.
  // Remove after //ios/web exposes a proper cookie clearing API.
  virtual void AddRestoreCompletionCallback(base::OnceClosure callback) = 0;

  // Removes all items from this except the last committed item, and inserts
  // copies of all items from |source| at the beginning of the session history.
  //
  // For example:
  // source: A B *C* D
  // this:   E F *G*
  // result: A B C *G*
  //
  // If there is a pending item after *G* in |this|, it is also preserved.
  // This ignores any pending or transient entries in |source|.  This will be a
  // no-op if called while CanPruneAllButLastCommittedItem() is false.
  virtual void CopyStateFromAndPrune(const NavigationManager* source) = 0;

  // Whether the NavigationManager can prune all but the last committed item.
  // This is true when all the following conditions are met:
  // - There is a last committed NavigationItem.
  // - There is no pending history navigation.
  // - There is no transient NavigationItem.
  virtual bool CanPruneAllButLastCommittedItem() const = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_NAVIGATION_MANAGER_H_
