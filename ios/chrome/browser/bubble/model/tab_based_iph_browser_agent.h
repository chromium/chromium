// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_BUBBLE_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "components/reading_list/core/reading_list_model_observer.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace reading_list {
enum EntrySource;
}  // namespace reading_list

class Browser;
class BrowserViewVisibilityNotifierBrowserAgent;
enum class BrowserViewVisibilityState;
@class CommandDispatcher;
@protocol HelpCommands;
@protocol PopupMenuCommands;
class ReadingListModel;
class UrlLoadingNotifierBrowserAgent;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// A browser agent that serves a central manager for all IPHs features that
// should be triggered by tab and/or tab list changes.
class TabBasedIPHBrowserAgent : public bookmarks::BaseBookmarkModelObserver,
                                public BrowserUserData<TabBasedIPHBrowserAgent>,
                                public BrowserObserver,
                                public BrowserViewVisibilityObserver,
                                public ReadingListModelObserver,
                                public UrlLoadingObserver,
                                public web::WebStateObserver {
 public:
  TabBasedIPHBrowserAgent(const TabBasedIPHBrowserAgent&) = delete;
  TabBasedIPHBrowserAgent& operator=(const TabBasedIPHBrowserAgent&) = delete;

  ~TabBasedIPHBrowserAgent() override;

#pragma mark - Public methods

  // Notifies the browser agent that the user has performed a multi-gesture tab
  // refresh. If the page happened to be scrolled to the top when it happened, a
  // in-product help for pull-to-refresh would be attempted.
  void NotifyMultiGestureRefreshEvent();

  // Notifies that the user has tapped the back/forward button in the toolbar,
  // so that the related in-product help would be attempted.
  void NotifyBackForwardButtonTap();

  // Notifies that the user has used the tab grid solely to switch to an
  // adjacent tab.
  void NotifySwitchToAdjacentTabFromTabGrid();

#pragma mark - Observer headers

  // bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // BrowserViewVisibilityObserver
  void BrowserViewVisibilityStateDidChange(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state) override;

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingShutdown(const ReadingListModel* model) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;

  // UrlLoadingObserver
  void TabDidLoadUrl(const GURL& url,
                     ui::PageTransition transition_type) override;

  // WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void DidStopLoading(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

#pragma mark - Private methods

 private:
  friend class BrowserUserData<TabBasedIPHBrowserAgent>;

  explicit TabBasedIPHBrowserAgent(Browser* browser);

  // Stops observing the bookmark model for changes. This is typically called
  // when `TabBasedIPHBrowserAgent` is being destroyed or when it should no
  // longer react to bookmark modifications.
  void StopObservingBookmarkModel();

  // Stops observing changes to the reading list model. This is typically
  // called when `TabBasedIPHBrowserAgent` is being destroyed, when reactions
  // to reading list modifications are no longer needed, or when the reading
  // list model itself is being destroyed.
  void StopObservingReadingListModel();

  // For all IPH features managed by this class, resets their tracker variables
  // to `false`, and remove currently displaying IPH views from the view.
  void ResetFeatureStatesAndRemoveIPHViews();

  // Command handler for help commands.
  id<HelpCommands> HelpHandler();

  // Command handler for popup menu commands.
  id<PopupMenuCommands> PopupMenuHandler();

  // Observer for the browser's web state list and the active web state.
  raw_ptr<WebStateList> web_state_list_;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      active_web_state_observer_;

#pragma mark - Observers variables
  // `BookmarkModel` instance providing access to bookmarks. May be `nullptr`.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;
  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      bookmark_model_observation_{this};
  // `ReadingListModel` instance providing access to the reading list. May be
  // `nullptr`.
  raw_ptr<ReadingListModel> reading_list_model_ = nullptr;
  // Tracks whether the reading list model has finished loading. Until the model
  // is fully loaded, it is unsafe to use or interact with it.
  bool reading_list_model_loaded_ = false;
  // Automatically removes this observer from the reading list model when
  // destroyed.
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_observation_{this};
  // Observer for URL loading.
  raw_ptr<UrlLoadingNotifierBrowserAgent> url_loading_notifier_;
  // Observer for browser view visibility.
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent>
      browser_view_visibility_notifier_;
  // Command dispatcher for the browser; used to retrieve help handler.
  CommandDispatcher* command_dispatcher_;
  // Records events for the use of in-product help.
  raw_ptr<feature_engagement::Tracker> engagement_tracker_;

#pragma mark - IPH feature invocation tracking variables

  // Whether a multi-gesture refresh is currently happening.
  bool multi_gesture_refresh_ = false;
  // Whether the user has just tapped back/forward button in the toolbar; will
  // be reset to `false` after the navigation has completed.
  bool back_forward_button_tapped_ = false;
  // Whether the user has just tapped an adjacent tab through the tab grid; will
  // be reset to `false` once the active tab is changed.
  bool tapped_adjacent_tab_ = false;
};

#endif  // IOS_CHROME_BROWSER_BUBBLE_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_
