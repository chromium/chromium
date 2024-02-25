// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Displays placeholder to cover what WebState is actually displaying. Can be
// used to display the cached image of the web page during the Tab restoration.
// The placeholder is added as a subview on the WebState's view. Properly
// positioning the placeholder requires that the WebState's view is in a view
// hierarchy that has the Content Area named guide.
class PagePlaceholderTabHelper
    : public web::WebStateUserData<PagePlaceholderTabHelper>,
      public web::WebStateObserver {
 public:
  PagePlaceholderTabHelper(const PagePlaceholderTabHelper&) = delete;
  PagePlaceholderTabHelper& operator=(const PagePlaceholderTabHelper&) = delete;

  ~PagePlaceholderTabHelper() override;

  // Displays placeholder between DidStartNavigation and PageLoaded
  // WebStateObserver callbacks. If navigation takes too long, then placeholder
  // will be removed before navigation is finished. The placeholder is only ever
  // displayed when the tab is visible.
  void AddPlaceholderForNextNavigation();

  // Cancels displaying placeholder during the next navigation. If placeholder
  // is displayed, then it is removed.
  void CancelPlaceholderForNextNavigation();

  // true if placeholder is currently being displayed.
  bool displaying_placeholder() const { return displaying_placeholder_; }

  // true if placeholder will be displayed between DidStartNavigation and
  // PageLoaded WebStateObserver callbacks.
  bool will_add_placeholder_for_next_navigation() const {
    return add_placeholder_for_next_navigation_;
  }

 private:
  friend class web::WebStateUserData<PagePlaceholderTabHelper>;

  explicit PagePlaceholderTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  void AddPlaceholder();
  void RemovePlaceholder();

  // Adds the given `snapshot` image to the `web_state_`'s view. The
  // `web_state_`'s view must be visible, and it must be in a view hierarchy
  // that has the Content Area named guide.
  void DisplaySnapshotImage(UIImage* snapshot);

  // Display image in a placeholder after retrieval from SnapshotTabHelper.
  void OnImageRetrieved(UIImage* image);

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // View used to display the placeholder.
  TopAlignedImageView* placeholder_view_ = nil;

  // true if placeholder is currently being displayed.
  bool displaying_placeholder_ = false;

  // true if placeholder must be displayed during the next navigation.
  bool add_placeholder_for_next_navigation_ = false;

  base::WeakPtrFactory<PagePlaceholderTabHelper> weak_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PAGE_PLACEHOLDER_TAB_HELPER_H_
