// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_

#include "ios/web/public/test/fakes/test_web_state_observer_util.h"
#import "ios/web/public/web_state_observer_bridge.h"

// Test implementation of CRWWebStateObserver protocol.
@interface CRWTestWebStateObserver : NSObject<CRWWebStateObserver>

// Arguments passed to |webStateWasShown:|.
@property(nonatomic, readonly) web::TestWasShownInfo* wasShownInfo;
// Arguments passed to |webStateWasHidden:|.
@property(nonatomic, readonly) web::TestWasHiddenInfo* wasHiddenInfo;
// Arguments passed to |webState:didPruneNavigationItemsWithCount:|.
@property(nonatomic, readonly)
    web::TestNavigationItemsPrunedInfo* navigationItemsPrunedInfo;
// Arguments passed to |webState:didStartNavigation:|.
@property(nonatomic, readonly)
    web::TestDidStartNavigationInfo* didStartNavigationInfo;
// Arguments passed to |webState:didFinishNavigation:|.
@property(nonatomic, readonly)
    web::TestDidFinishNavigationInfo* didFinishNavigationInfo;
// Arguments passed to |webState:didLoadPageWithSuccess:|.
@property(nonatomic, readonly) web::TestLoadPageInfo* loadPageInfo;
// Arguments passed to |webState:didChangeLoadingProgress:|.
@property(nonatomic, readonly)
    web::TestChangeLoadingProgressInfo* changeLoadingProgressInfo;
// Arguments passed to |webStateDidChangeTitle:|.
@property(nonatomic, readonly) web::TestTitleWasSetInfo* titleWasSetInfo;
// Arguments passed to |webStateDidChangeVisibleSecurityState:|.
@property(nonatomic, readonly) web::TestDidChangeVisibleSecurityStateInfo*
    didChangeVisibleSecurityStateInfo;
// Arguments passed to |webState:didUpdateFaviconURLCandidates|.
@property(nonatomic, readonly)
    web::TestUpdateFaviconUrlCandidatesInfo* updateFaviconUrlCandidatesInfo;
// Arguments passed to |webState:renderProcessGoneForWebState:|.
@property(nonatomic, readonly)
    web::TestRenderProcessGoneInfo* renderProcessGoneInfo;
// Arguments passed to |webStateDestroyed:|.
@property(nonatomic, readonly)
    web::TestWebStateDestroyedInfo* webStateDestroyedInfo;
// Arguments passed to |webStateDidStopLoading:|.
@property(nonatomic, readonly) web::TestStopLoadingInfo* stopLoadingInfo;
// Arguments passed to |webStateDidStartLoading:|.
@property(nonatomic, readonly) web::TestStartLoadingInfo* startLoadingInfo;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_
