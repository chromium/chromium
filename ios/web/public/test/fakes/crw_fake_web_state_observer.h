// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_OBSERVER_H_

#include "ios/web/public/test/fakes/fake_web_state_observer_util.h"
#import "ios/web/public/web_state_observer_bridge.h"

// Test implementation of CRWWebStateObserver protocol.
@interface CRWFakeWebStateObserver : NSObject <CRWWebStateObserver>

// Arguments passed to `webStateWasShown:`.
@property(nonatomic, readonly) web::TestWasShownInfo* wasShownInfo;
// Arguments passed to `webStateWasHidden:`.
@property(nonatomic, readonly) web::TestWasHiddenInfo* wasHiddenInfo;
// Arguments passed to `webState:didStartNavigation:`.
@property(nonatomic, readonly)
    web::TestDidStartNavigationInfo* didStartNavigationInfo;
// Arguments passed to `webState:didRedirectNavigation:`.
@property(nonatomic, readonly)
    web::TestDidRedirectNavigationInfo* didRedirectNavigationInfo;
// Arguments passed to `webState:didFinishNavigation:`.
@property(nonatomic, readonly)
    web::TestDidFinishNavigationInfo* didFinishNavigationInfo;
// Arguments passed to `webStateDidStartLoading:`.
@property(nonatomic, readonly) web::TestStartLoadingInfo* startLoadingInfo;
// Arguments passed to `webStateDidStopLoading:`.
@property(nonatomic, readonly) web::TestStopLoadingInfo* stopLoadingInfo;
// Arguments passed to `webState:didLoadPageWithSuccess:`.
@property(nonatomic, readonly) web::TestLoadPageInfo* loadPageInfo;
// Arguments passed to `webState:didChangeLoadingProgress:`.
@property(nonatomic, readonly)
    web::TestChangeLoadingProgressInfo* changeLoadingProgressInfo;
// Arguments passed to `webStateDidChangeBackForwardState:`.
@property(nonatomic, readonly)
    web::TestDidChangeBackForwardStateInfo* changeBackForwardStateInfo;
// Arguments passed to `webStateDidChangeTitle:`.
@property(nonatomic, readonly) web::TestTitleWasSetInfo* titleWasSetInfo;
// Arguments passed to `webStateDidChangeVisibleSecurityState:`.
@property(nonatomic, readonly) web::TestDidChangeVisibleSecurityStateInfo*
    didChangeVisibleSecurityStateInfo;
// Arguments passed to `webState:didUpdateFaviconURLCandidates`.
@property(nonatomic, readonly)
    web::TestUpdateFaviconUrlCandidatesInfo* updateFaviconUrlCandidatesInfo;
// Arguments passed to `webStateDidChangeUnderPageBackgroundColor:`.
@property(nonatomic, readonly) web::TestUnderPageBackgroundColorChangedInfo*
    underPageBackgroundColorChangedInfo;
// Arguments passed to `renderProcessGoneForWebState:`.
@property(nonatomic, readonly)
    web::TestRenderProcessGoneInfo* renderProcessGoneInfo;
// Arguments passed to `webStateRealized:`.
@property(nonatomic, readonly)
    web::TestWebStateRealizedInfo* webStateRealizedInfo;
// Arguments passed to `webStateDestroyed:`.
@property(nonatomic, readonly)
    web::TestWebStateDestroyedInfo* webStateDestroyedInfo;
// Arguments passed to `webState:didChangeStateForPermission:`.
@property(nonatomic, readonly)
    web::TestWebStatePermissionStateChangedInfo* permissionStateChangedInfo;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_WEB_STATE_OBSERVER_H_
