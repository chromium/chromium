// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_test_web_state_observer.h"

#include <memory>

#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
TestUpdateFaviconUrlCandidatesInfo::TestUpdateFaviconUrlCandidatesInfo() {}
TestUpdateFaviconUrlCandidatesInfo::~TestUpdateFaviconUrlCandidatesInfo() =
    default;
}

@implementation CRWTestWebStateObserver {
  // Arguments passed to |webStateWasShown:|.
  std::unique_ptr<web::TestWasShownInfo> _wasShownInfo;
  // Arguments passed to |webStateWasHidden:|.
  std::unique_ptr<web::TestWasHiddenInfo> _wasHiddenInfo;
  // Arguments passed to |webState:didPruneNavigationItemsWithCount:|.
  std::unique_ptr<web::TestNavigationItemsPrunedInfo>
      _navigationItemsPrunedInfo;
  // Arguments passed to |webState:didStartNavigation:|.
  std::unique_ptr<web::TestDidStartNavigationInfo> _didStartNavigationInfo;
  // Arguments passed to |webState:didFinishNavigationForURL:|.
  std::unique_ptr<web::TestDidFinishNavigationInfo> _didFinishNavigationInfo;
  // Arguments passed to |webState:didLoadPageWithSuccess:|.
  std::unique_ptr<web::TestLoadPageInfo> _loadPageInfo;
  // Arguments passed to |webState:didChangeLoadingProgress:|.
  std::unique_ptr<web::TestChangeLoadingProgressInfo>
      _changeLoadingProgressInfo;
  // Arguments passed to |webStateDidChangeTitle:|.
  std::unique_ptr<web::TestTitleWasSetInfo> _titleWasSetInfo;
  // Arguments passed to |webStateDidChangeVisibleSecurityState:|.
  std::unique_ptr<web::TestDidChangeVisibleSecurityStateInfo>
      _didChangeVisibleSecurityStateInfo;
  // Arguments passed to |webState:didUpdateFaviconURLCandidates|.
  std::unique_ptr<web::TestUpdateFaviconUrlCandidatesInfo>
      _updateFaviconUrlCandidatesInfo;
  // Arguments passed to |webState:renderProcessGoneForWebState:|.
  std::unique_ptr<web::TestRenderProcessGoneInfo> _renderProcessGoneInfo;
  // Arguments passed to |webStateDestroyed:|.
  std::unique_ptr<web::TestWebStateDestroyedInfo> _webStateDestroyedInfo;
  // Arguments passed to |webStateDidStopLoading:|.
  std::unique_ptr<web::TestStopLoadingInfo> _stopLoadingInfo;
  // Arguments passed to |webStateDidStartLoading:|.
  std::unique_ptr<web::TestStartLoadingInfo> _startLoadingInfo;
}

- (web::TestWasShownInfo*)wasShownInfo {
  return _wasShownInfo.get();
}

- (web::TestWasHiddenInfo*)wasHiddenInfo {
  return _wasHiddenInfo.get();
}

- (web::TestNavigationItemsPrunedInfo*)navigationItemsPrunedInfo {
  return _navigationItemsPrunedInfo.get();
}

- (web::TestDidStartNavigationInfo*)didStartNavigationInfo {
  return _didStartNavigationInfo.get();
}

- (web::TestDidFinishNavigationInfo*)didFinishNavigationInfo {
  return _didFinishNavigationInfo.get();
}

- (web::TestLoadPageInfo*)loadPageInfo {
  return _loadPageInfo.get();
}

- (web::TestChangeLoadingProgressInfo*)changeLoadingProgressInfo {
  return _changeLoadingProgressInfo.get();
}

- (web::TestTitleWasSetInfo*)titleWasSetInfo {
  return _titleWasSetInfo.get();
}

- (web::TestDidChangeVisibleSecurityStateInfo*)
    didChangeVisibleSecurityStateInfo {
  return _didChangeVisibleSecurityStateInfo.get();
}

- (web::TestUpdateFaviconUrlCandidatesInfo*)updateFaviconUrlCandidatesInfo {
  return _updateFaviconUrlCandidatesInfo.get();
}

- (web::TestRenderProcessGoneInfo*)renderProcessGoneInfo {
  return _renderProcessGoneInfo.get();
}

- (web::TestWebStateDestroyedInfo*)webStateDestroyedInfo {
  return _webStateDestroyedInfo.get();
}

- (web::TestStopLoadingInfo*)stopLoadingInfo {
  return _stopLoadingInfo.get();
}

- (web::TestStartLoadingInfo*)startLoadingInfo {
  return _startLoadingInfo.get();
}

#pragma mark CRWWebStateObserver methods -

- (void)webStateWasShown:(web::WebState*)webState {
  _wasShownInfo = std::make_unique<web::TestWasShownInfo>();
  _wasShownInfo->web_state = webState;
}

- (void)webStateWasHidden:(web::WebState*)webState {
  _wasHiddenInfo = std::make_unique<web::TestWasHiddenInfo>();
  _wasHiddenInfo->web_state = webState;
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  _navigationItemsPrunedInfo =
      std::make_unique<web::TestNavigationItemsPrunedInfo>();
  _navigationItemsPrunedInfo->web_state = webState;
  _navigationItemsPrunedInfo->count = pruned_item_count;
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  ASSERT_TRUE(!navigation->GetError() || !navigation->IsSameDocument());
  _didStartNavigationInfo = std::make_unique<web::TestDidStartNavigationInfo>();
  _didStartNavigationInfo->web_state = webState;
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          navigation->GetWebState(), navigation->GetUrl(),
          navigation->HasUserGesture(), navigation->GetPageTransition(),
          navigation->IsRendererInitiated());
  context->SetIsSameDocument(navigation->IsSameDocument());
  context->SetError(navigation->GetError());
  _didStartNavigationInfo->context = std::move(context);
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  ASSERT_TRUE(!navigation->GetError() || !navigation->IsSameDocument());
  _didFinishNavigationInfo =
      std::make_unique<web::TestDidFinishNavigationInfo>();
  _didFinishNavigationInfo->web_state = webState;
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          navigation->GetWebState(), navigation->GetUrl(),
          navigation->HasUserGesture(), navigation->GetPageTransition(),
          navigation->IsRendererInitiated());
  context->SetIsSameDocument(navigation->IsSameDocument());
  context->SetError(navigation->GetError());
  _didFinishNavigationInfo->context = std::move(context);
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  _loadPageInfo = std::make_unique<web::TestLoadPageInfo>();
  _loadPageInfo->web_state = webState;
  _loadPageInfo->success = success;
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  _changeLoadingProgressInfo =
      std::make_unique<web::TestChangeLoadingProgressInfo>();
  _changeLoadingProgressInfo->web_state = webState;
  _changeLoadingProgressInfo->progress = progress;
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  _titleWasSetInfo = std::make_unique<web::TestTitleWasSetInfo>();
  _titleWasSetInfo->web_state = webState;
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  _didChangeVisibleSecurityStateInfo =
      std::make_unique<web::TestDidChangeVisibleSecurityStateInfo>();
  _didChangeVisibleSecurityStateInfo->web_state = webState;
}

- (void)webState:(web::WebState*)webState
    didUpdateFaviconURLCandidates:
        (const std::vector<web::FaviconURL>&)candidates {
  _updateFaviconUrlCandidatesInfo =
      std::make_unique<web::TestUpdateFaviconUrlCandidatesInfo>();
  _updateFaviconUrlCandidatesInfo->web_state = webState;
  _updateFaviconUrlCandidatesInfo->candidates = candidates;
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  _renderProcessGoneInfo = std::make_unique<web::TestRenderProcessGoneInfo>();
  _renderProcessGoneInfo->web_state = webState;
}

- (void)webStateDestroyed:(web::WebState*)webState {
  _webStateDestroyedInfo = std::make_unique<web::TestWebStateDestroyedInfo>();
  _webStateDestroyedInfo->web_state = webState;
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  _stopLoadingInfo = std::make_unique<web::TestStopLoadingInfo>();
  _stopLoadingInfo->web_state = webState;
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  _startLoadingInfo = std::make_unique<web::TestStartLoadingInfo>();
  _startLoadingInfo->web_state = webState;
}

@end
