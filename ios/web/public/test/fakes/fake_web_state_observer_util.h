// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_

#include <Foundation/Foundation.h>
#include <memory>

#include "ios/web/public/favicon/favicon_url.h"
#include "url/gurl.h"

namespace web {

class NavigationContext;
enum Permission : NSUInteger;
struct SSLStatus;
class WebFrame;
class WebState;

// Arguments passed to `WasShown`.
struct TestWasShownInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `WasHidden`.
struct TestWasHiddenInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `DidStartNavigation`.
struct TestDidStartNavigationInfo {
  TestDidStartNavigationInfo();
  ~TestDidStartNavigationInfo();
  WebState* web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidRedirectNavigation`.
struct TestDidRedirectNavigationInfo {
  TestDidRedirectNavigationInfo();
  ~TestDidRedirectNavigationInfo();
  WebState* web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidFinishNavigation`.
struct TestDidFinishNavigationInfo {
  TestDidFinishNavigationInfo();
  ~TestDidFinishNavigationInfo();
  WebState* web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidStartLoading`.
struct TestStartLoadingInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `DidStopLoading`.
struct TestStopLoadingInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `PageLoaded`.
struct TestLoadPageInfo {
  WebState* web_state = nullptr;
  bool success = false;
};

// Arguments passed to `LoadProgressChanged`.
struct TestChangeLoadingProgressInfo {
  WebState* web_state = nullptr;
  double progress = 0.0;
};

// Arguments passed to `DidChangeBackForwardState`.
struct TestDidChangeBackForwardStateInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `TitleWasSet`.
struct TestTitleWasSetInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `DidChangeVisibleSecurityState` and SSLStatus of the
// visible navigation item.
struct TestDidChangeVisibleSecurityStateInfo {
  TestDidChangeVisibleSecurityStateInfo();
  ~TestDidChangeVisibleSecurityStateInfo();
  WebState* web_state = nullptr;

  // SSLStatus of the visible navigation item when
  // DidChangeVisibleSecurityState was called.
  std::unique_ptr<SSLStatus> visible_ssl_status;
};

// Arguments passed to `FaviconUrlUpdated`.
struct TestUpdateFaviconUrlCandidatesInfo {
  TestUpdateFaviconUrlCandidatesInfo();
  ~TestUpdateFaviconUrlCandidatesInfo();
  WebState* web_state = nullptr;
  std::vector<web::FaviconURL> candidates;
};

// Arguments passed to `WebFrameDidBecomeAvailable` or
// `WebFrameWillBecomeUnavailable`.
struct TestWebFrameAvailabilityInfo {
  WebState* web_state = nullptr;
  WebFrame* web_frame = nullptr;
};

// Arguments passed to `RenderProcessGone`.
struct TestRenderProcessGoneInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `WebStateRealized`.
struct TestWebStateRealizedInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `WebStateDestroyed`.
struct TestWebStateDestroyedInfo {
  WebState* web_state = nullptr;
};

// Arguments passed to `PermissionStateChanged`.
struct TestWebStatePermissionStateChangedInfo {
  WebState* web_state = nullptr;
  web::Permission permission;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_
