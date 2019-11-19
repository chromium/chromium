// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/error_retry_state_machine.h"

#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using wk_navigation_util::CreatePlaceholderUrlForUrl;
using wk_navigation_util::CreateRedirectUrl;
using wk_navigation_util::IsPlaceholderUrl;
using wk_navigation_util::IsRestoreSessionUrl;
using wk_navigation_util::ExtractTargetURL;

ErrorRetryStateMachine::ErrorRetryStateMachine()
    : state_(ErrorRetryState::kNewRequest) {}

ErrorRetryStateMachine::ErrorRetryStateMachine(
    const ErrorRetryStateMachine& machine)
    : state_(machine.state_), url_(machine.url_) {}

void ErrorRetryStateMachine::SetURL(const GURL& url) {
  url_ = url;
}

ErrorRetryState ErrorRetryStateMachine::state() const {
  return state_;
}

void ErrorRetryStateMachine::SetNoNavigationError() {
  state_ = ErrorRetryState::kNoNavigationError;
}

void ErrorRetryStateMachine::SetDisplayingWebError() {
  // Web error is displayed in two scenarios:
  // (1) Placeholder entry for network load error finished loading in web view.
  //     This is the common case.
  // (2) Retry of a previously failed load failed in SSL error. This can happen
  //     when the first navigation failed in offline mode. SSL interstitial does
  //     not normally trigger ErrorRetryStateMachine because the error page is
  //     not to become part of the navigation history. This leaves the item
  //     stuck in the transient kRetryFailedNavigationItem state. So for this
  //     specific case, treat the SSL interstitial as a web error so that
  //     error retry works as expected on subsequent back/forward navigations.
  DCHECK(state_ == ErrorRetryState::kReadyToDisplayError ||
         state_ == ErrorRetryState::kRetryFailedNavigationItem)
      << "Unexpected error retry state: " << static_cast<int>(state_);
  state_ = ErrorRetryState::kDisplayingError;
}

void ErrorRetryStateMachine::SetRetryPlaceholderNavigation() {
  DCHECK(state_ == web::ErrorRetryState::kNoNavigationError);
  state_ = ErrorRetryState::kRetryPlaceholderNavigation;
}

void ErrorRetryStateMachine::SetIgnorePlaceholderNavigation() {
  state_ = ErrorRetryState::kIgnorePlaceholderNavigation;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFailProvisionalNavigation(
    const GURL& web_view_url,
    const GURL& error_url) {
  switch (state_) {
    case ErrorRetryState::kNewRequest:
      if (web_view_url == CreateRedirectUrl(error_url)) {
        // Client redirect in restore_session.html failed. A placeholder is not
        // needed here because a back/forward item already exists for
        // restore_session.html.
        state_ = ErrorRetryState::kReadyToDisplayError;
        return ErrorRetryCommand::kLoadError;
      }
      // Provisional navigation failed on a new item.
      state_ = ErrorRetryState::kLoadingPlaceholder;
      return ErrorRetryCommand::kLoadPlaceholder;

    // Reload of a previously successful load fails.
    case ErrorRetryState::kNoNavigationError:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kRetryFailedNavigationItem:
    // This case happens for the second back/forward navigation in offline mode
    // to a page that initially loaded successfully.
    case ErrorRetryState::kDisplayingError:
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        // In this case, a back/forward item already exists. Rewriting the
        // WebView's URL to the placeholder URL before loading the error page
        // ensures that WebKit doesn't associate the (empty) contents of an
        // immediately-preceding kRewriteToWebViewURL step with the actual URL
        // in its page cache. See http://crbug.com/950489.
        state_ = ErrorRetryState::kLoadingPlaceholder;
        return ErrorRetryCommand::kRewriteToPlaceholderURL;
      } else {
        return BackForwardOrReloadFailed();
      }

    case ErrorRetryState::kLoadingPlaceholder:
    case ErrorRetryState::kRetryPlaceholderNavigation:
    case ErrorRetryState::kReadyToDisplayError:
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
    case ErrorRetryState::kIgnorePlaceholderNavigation:
      NOTREACHED() << "Unexpected error retry state: "
                   << static_cast<unsigned>(state_);
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFailNavigation(
    const GURL& web_view_url) {
  switch (state_) {
    case ErrorRetryState::kNewRequest:
      state_ = ErrorRetryState::kReadyToDisplayError;
      return ErrorRetryCommand::kLoadError;

    // Reload of a previously successful load fails.
    case ErrorRetryState::kNoNavigationError:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kRetryFailedNavigationItem:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kDisplayingError:
      return BackForwardOrReloadFailed();

    case ErrorRetryState::kLoadingPlaceholder:
    case ErrorRetryState::kRetryPlaceholderNavigation:
    case ErrorRetryState::kReadyToDisplayError:
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
    case ErrorRetryState::kIgnorePlaceholderNavigation:
      NOTREACHED() << "Unexpected error retry state: "
                   << static_cast<unsigned>(state_);
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFinishNavigation(
    const GURL& web_view_url) {
  switch (state_) {
    case ErrorRetryState::kLoadingPlaceholder:
      // (1) Placeholder load for initial failure succeeded.
      if (@available(iOS 13, *)) {
        // This DCHECK is hit on iOS 12 when navigating to restricted URL. See
        // crbug.com/1000366 for more details.
        DCHECK_EQ(web_view_url, CreatePlaceholderUrlForUrl(url_));
      }
      state_ = ErrorRetryState::kReadyToDisplayError;
      return ErrorRetryCommand::kLoadError;

    case ErrorRetryState::kRetryPlaceholderNavigation:
      if (IsPlaceholderUrl(web_view_url)) {
        // (11) Explicitly keep the state the same so after rewriting to the non
        // placeholder url the else block will trigger.
        DCHECK_EQ(web_view_url, CreatePlaceholderUrlForUrl(url_));
        state_ = ErrorRetryState::kRetryPlaceholderNavigation;
        return ErrorRetryCommand::kRewriteToWebViewURL;
      } else {
        // The url was written by kRewriteToWebViewURL in the if block, so on
        // this navigation load an error view.
        state_ = ErrorRetryState::kReadyToDisplayError;
        return ErrorRetryCommand::kLoadError;
      }
    case ErrorRetryState::kNewRequest:
      if (IsRestoreSessionUrl(web_view_url)) {
        // (8) Initial load of restore_session.html. Don't change state or
        // issue command. Wait for the client-side redirect.
      } else if (IsPlaceholderUrl(web_view_url) &&
                 web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        state_ = ErrorRetryState::kNavigatingToFailedNavigationItem;
        return ErrorRetryCommand::kRewriteToWebViewURL;
      } else {
        // (2) Initial load succeeded.
        state_ = ErrorRetryState::kNoNavigationError;
      }
      break;

    case ErrorRetryState::kReadyToDisplayError:
      // (3) Finished loading error in web view.
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kDisplayingError;
      break;

    case ErrorRetryState::kDisplayingError:
      if (web_view_url == CreatePlaceholderUrlForUrl(url_)) {
        // (4) Back/forward to or reload of placeholder URL. Rewrite WebView URL
        // to prepare for retry.
        state_ = ErrorRetryState::kNavigatingToFailedNavigationItem;
        return ErrorRetryCommand::kRewriteToWebViewURL;
      }

      if (IsRestoreSessionUrl(web_view_url)) {
        GURL target_url;
        if (ExtractTargetURL(web_view_url, &target_url) && target_url == url_) {
          // (10) Back/forward navigation to a restored session entry in offline
          // mode. It is OK to consider this load succeeded for now because the
          // failure delegate will be triggered again if the load fails.
          state_ = ErrorRetryState::kNoNavigationError;
          return ErrorRetryCommand::kDoNothing;
        }
      }

      // (5) This is either a reload of the original URL that succeeded in
      // WebView (either because it was already in Page Cache or the network
      // load succeded), or a back/forward of a previous WebUI error that is
      // served from Page Cache. It's impossible to distinguish between the two
      // because in both cases, |web_view_url| is the original URL. This can
      // lead to network error being displayed even when network condition
      // is regained. User has to reload explicitly to retry loading online.
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kNoNavigationError;
      break;

    // (6) Successfully rewritten the WebView URL from placeholder URL to
    // original URL. Ready to try reload.
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kRetryFailedNavigationItem;
      return ErrorRetryCommand::kReload;

    // (7) Retry loading succeeded.
    case ErrorRetryState::kRetryFailedNavigationItem:
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kNoNavigationError;
      break;

    case ErrorRetryState::kIgnorePlaceholderNavigation:
      state_ = ErrorRetryState::kNoNavigationError;
      break;

    // (9) Back/forward to or reload of a previously successfull load succeeds
    // again.
    case ErrorRetryState::kNoNavigationError:
      break;
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::BackForwardOrReloadFailed() {
  state_ = ErrorRetryState::kReadyToDisplayError;
  return ErrorRetryCommand::kLoadError;
}

}  // namespace web
