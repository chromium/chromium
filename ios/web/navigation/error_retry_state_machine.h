// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_
#define IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_

#include "url/gurl.h"

namespace web {

// Defines the states of a NavigationItem that failed to load. This is used by
// CRWWebController to coordinate the display of native error views such that
// back/forward navigation to a native error view automatically triggers a
// reload of the original URL. This is achieved in four steps:
// 1) A NavigationItem is put into
//    kDisplaying(Native|Web)ErrorForFailedNavigation when it first failed to
//    load and a web error is displayed. If the failure occurred during
//    provisional navigation, a placeholder entry is inserted into
//    WKBackForwardList for this item.
// 2) Upon navigation to this item, use |loadHTMLString:| to modify the URL of
//    the placeholder entry to the original URL and change the item state to
//    kNavigatingToFailedNavigationItem.
// 3) Upon completion of the previous navigation, force a reload in WKWebView to
//    reload the original URL and change the item state to
//    kRetryFailedNavigationItem.
// 4) Upon completion of the reload, if successful, the item state is changed to
//    kNoNavigationError.
// See https://bit.ly/2sxWJgs for the full state transition diagram.
enum class ErrorRetryState {
  // This is a new navigation request.
  kNewRequest,
  // This navigation item loaded without error.
  kNoNavigationError,
  // This navigation item failed to load and is in the process of loading a
  // placeholder.
  kLoadingPlaceholder,
  // This navigation item has an entry in WKBackForwardList. Ready to present
  // error in native view.
  kReadyToDisplayErrorForFailedNavigation,
  // This navigation item failed to load and a native error is displayed.
  kDisplayingNativeErrorForFailedNavigation,
  // This navigation item failed to load and a web error is displayed.
  kDisplayingWebErrorForFailedNavigation,
  // This navigation item is reactivated due to back/forward navigation and
  // needs to try reloading.
  kNavigatingToFailedNavigationItem,
  // This navigation item is ready to be reloaded in web view.
  kRetryFailedNavigationItem,
};

// Commands for CRWWebController to execute the state transition.
enum class ErrorRetryCommand {
  // WebView should load placeholder request.
  kLoadPlaceholder,
  // WebView should load error view.
  kLoadErrorView,
  // WebView should reload.
  kReload,
  // WebView should rewrite its URL (assumed to be a placeholder URL) to the
  // navigation item's URL to prepare for reload.
  kRewriteWebViewURL,
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

  // Transitions the state machine to kDisplayingWebErrorForFailedNavigation.
  void SetDisplayingWebError();

  // Runs state transitions upon a failed provisional navigation.
  ErrorRetryCommand DidFailProvisionalNavigation(const GURL& web_view_url,
                                                 const GURL& error_url);

  // Runs state transitions upon a failure after the navigation is committed.
  ErrorRetryCommand DidFailNavigation(const GURL& web_view_url,
                                      const GURL& error_url);

  // Runs state transitions upon a successful navigation.
  ErrorRetryCommand DidFinishNavigation(const GURL& web_view_url);

 private:
  ErrorRetryCommand BackForwardOrReloadFailed(const GURL& web_view_url,
                                              const GURL& error_url);

  ErrorRetryState state_;
  GURL url_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_ERROR_RETRY_STATE_MACHINE_H_
