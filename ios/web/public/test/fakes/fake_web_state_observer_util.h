// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_

#include <Foundation/Foundation.h>
#include <memory>

#import "base/memory/raw_ptr.h"
#include "ios/web/public/favicon/favicon_url.h"
#include "url/gurl.h"

namespace web {

class NavigationContext;
enum Permission : NSUInteger;
struct SSLStatus;
class WebState;

// Arguments passed to `WasShown`.
struct TestWasShownInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `WasHidden`.
struct TestWasHiddenInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `DidStartNavigation`.
struct TestDidStartNavigationInfo {
  TestDidStartNavigationInfo();
  ~TestDidStartNavigationInfo();
  raw_ptr<WebState> web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidRedirectNavigation`.
struct TestDidRedirectNavigationInfo {
  TestDidRedirectNavigationInfo();
  ~TestDidRedirectNavigationInfo();
  raw_ptr<WebState> web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidFinishNavigation`.
struct TestDidFinishNavigationInfo {
  TestDidFinishNavigationInfo();
  ~TestDidFinishNavigationInfo();
  raw_ptr<WebState> web_state = nullptr;
  std::unique_ptr<web::NavigationContext> context;
};

// Arguments passed to `DidStartLoading`.
struct TestStartLoadingInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `DidStopLoading`.
struct TestStopLoadingInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `PageLoaded`.
struct TestLoadPageInfo {
  raw_ptr<WebState> web_state = nullptr;
  bool success = false;
};

// Arguments passed to `LoadProgressChanged`.
struct TestChangeLoadingProgressInfo {
  raw_ptr<WebState> web_state = nullptr;
  double progress = 0.0;
};

// Arguments passed to `DidChangeBackForwardState`.
struct TestDidChangeBackForwardStateInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `TitleWasSet`.
struct TestTitleWasSetInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `DidChangeVisibleSecurityState` and SSLStatus of the
// visible navigation item.
struct TestDidChangeVisibleSecurityStateInfo {
  TestDidChangeVisibleSecurityStateInfo();
  ~TestDidChangeVisibleSecurityStateInfo();
  raw_ptr<WebState> web_state = nullptr;

  // SSLStatus of the visible navigation item when
  // DidChangeVisibleSecurityState was called.
  std::unique_ptr<SSLStatus> visible_ssl_status;
};

// Arguments passed to `FaviconUrlUpdated`.
struct TestUpdateFaviconUrlCandidatesInfo {
  TestUpdateFaviconUrlCandidatesInfo();
  ~TestUpdateFaviconUrlCandidatesInfo();
  raw_ptr<WebState> web_state = nullptr;
  std::vector<web::FaviconURL> candidates;
};

// Arguments passed to `UnderPageBackgroundColorChanged`.
struct TestUnderPageBackgroundColorChangedInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `RenderProcessGone`.
struct TestRenderProcessGoneInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `WebStateRealized`.
struct TestWebStateRealizedInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `WebStateDestroyed`.
struct TestWebStateDestroyedInfo {
  raw_ptr<WebState> web_state = nullptr;
};

// Arguments passed to `PermissionStateChanged`.
struct TestWebStatePermissionStateChangedInfo {
  raw_ptr<WebState> web_state = nullptr;
  web::Permission permission;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_STATE_OBSERVER_UTIL_H_
