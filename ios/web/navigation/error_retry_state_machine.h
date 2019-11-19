// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_
#define IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_

#include "url/gurl.h"

namespace web {

// Defines the states of a NavigationItem that failed to load. This is used to
// coordinate the display of error page such that back/forward/reload navigation
// to a loaded error page will load the original URL. This is achieved in four
// steps:
// 1) A NavigationItem is set to gkDisplayingError when it first failed to load
//    and a web error is displayed. If the failure occurred during provisional
//    navigation, a placeholder entry is inserted into WKBackForwardList for
//    this item.
// 2) Upon navigation to this item, use |loadHTMLString:| to modify the URL of
//    the placeholder entry to the original URL and change the item state to
//    kNavigatingToFailedNavigationItem.
// 3) Upon completion of the previous navigation, force a reload in WKWebView to
//    reload the original URL and change the item state to
//    kRetryFailedNavigationItem.
// 4) Upon completion of the reload, if successful, the item state is changed to
//    kNoNavigationError.
// Full state transition diagram:
// https://docs.google.com/spreadsheets/d/1AdwRIuCShbRy4gEFB-SxaupmPmmO2MO7oLZ4F87XjRE/edit?usp=sharing
enum class ErrorRetryState {
  // This is a new navigation request.
  kNewRequest,
  // This navigation item loaded without error.
  kNoNavigationError,
  // This navigation item failed to load and is in the process of loading a
  // placeholder.
  kLoadingPlaceholder,
  // This navigation item failed to load and was stored in the WKBackForwardList
  // and is now being reloaded.
  kRetryPlaceholderNavigation,
  // This navigation item has an entry in WKBackForwardList. Ready to present
  // error in native view.
  kReadyToDisplayError,
  // This navigation item failed to load and a web error is displayed.
  kDisplayingError,
  // This navigation item is reactivated due to back/forward navigation and
  // needs to try reloading.
  kNavigatingToFailedNavigationItem,
  // This navigation item is ready to be reloaded in web view.
  kRetryFailedNavigationItem,
  // Test only, used to ignore a placeholder navigation used to support
  // LoadHtml.
  kIgnorePlaceholderNavigation,
};

// Commands for CRWWebController to execute the state transition.
enum class ErrorRetryCommand {
  // WebView should load placeholder request.
  kLoadPlaceholder,
  // WebView should load error view.
  kLoadError,
  // WebView should reload.
  kReload,
  // WebView should rewrite its URL (assumed to be a placeholder URL) to the
  // navigation item's URL to prepare for reload.
  kRewriteToWebViewURL,
  // WebView should rewrite its URL to a placeholder URL to prepare for loading
  // the error view.
  kRewriteToPlaceholderURL,
  // WebView doesn't need to do anything.
  kDoNothing,
};

// A state machine that manages error and retry state of a navigation item. This
// is used to adapt WKWebView, which does not add provisional navigation failure
// to the back-forward list, to the Chrome convention, where such navigations
// are added to the back-forward list.
class ErrorRetryStateMachine {
 public:
  ErrorRetryStateMachine();
  explicit ErrorRetryStateMachine(const ErrorRetryStateMachine& machine);

  // Sets the original request URL to be tracked by this state machine. It is
  // used for invariant detection.
  void SetURL(const GURL& url);

  // Returns the current error retry state.
  ErrorRetryState state() const;

  // Transitions the state machine to kNoNavigationError.
  void SetNoNavigationError();

  // Transitions the state machine to kDisplayingError.
  void SetDisplayingWebError();

  // Transitions the state machine to kRetryPlaceholderNavigation.
  void SetRetryPlaceholderNavigation();

  // Only used for testing. Sets state to kIgnorePlaceholderNavigation, which
  // will ignore the placeholder navigation used to support LoadHtml in tests.
  void SetIgnorePlaceholderNavigation();

  // Runs state transitions upon a failed provisional navigation.
  ErrorRetryCommand DidFailProvisionalNavigation(const GURL& web_view_url,
                                                 const GURL& error_url);

  // Runs state transitions upon a failure after the navigation is committed.
  ErrorRetryCommand DidFailNavigation(const GURL& web_view_url);

  // Runs state transitions upon a successful navigation.
  ErrorRetryCommand DidFinishNavigation(const GURL& web_view_url);

 private:
  ErrorRetryCommand BackForwardOrReloadFailed();

  ErrorRetryState state_;
  GURL url_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_
