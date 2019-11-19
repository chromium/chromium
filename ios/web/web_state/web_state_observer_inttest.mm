// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/ios/ios_util.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#include "ios/web/navigation/web_kit_constants.h"
#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/deprecated/crw_native_content_holder.h"
#import "ios/web/public/deprecated/test_native_content.h"
#import "ios/web/public/deprecated/test_native_content_provider.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/test/error_test_util.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

using wk_navigation_util::CreateRedirectUrl;

const char kExpectedMimeType[] = "text/html";

const char kFailedTitle[] = "failed_title";

// Location of a test page.
const char kTestPageURL[] = "/pony.html";

// A text string from the test HTML page at |kTestPageURL|.
const char kTestSessionStoragePageText[] = "pony";

// Returns a session storage with a single committed entry of a test HTML page.
CRWSessionStorage* GetTestSessionStorage(const GURL& testUrl) {
  CRWSessionStorage* result = [[CRWSessionStorage alloc] init];
  result.lastCommittedItemIndex = 0;
  CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
  [item setVirtualURL:testUrl];
  [result setItemStorages:@[ item ]];
  return result;
}

// WebStateObserverTest is parameterized on this enum to test both
// LegacyNavigationManagerImpl and WKBasedNavigationManagerImpl.
enum NavigationManagerChoice {
  TEST_LEGACY_NAVIGATION_MANAGER,
  TEST_WK_BASED_NAVIGATION_MANAGER,
};

// Calls Stop() on the given WebState.
ACTION_P(ReturnTrueAndStopNavigation, web_state) {
  dispatch_async(dispatch_get_main_queue(), ^{
    web_state->Stop();
  });
  return true;
}

// Verifies correctness of WebState's title.
ACTION_P(VerifyTitle, expected_title) {
  WebState* web_state = arg0;
  EXPECT_EQ(expected_title, base::UTF16ToASCII(web_state->GetTitle()));
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P5(VerifyPageStartedContext,
          web_state,
          url,
          transition,
          context,
          nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(transition, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetPendingItem();
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer. This action is used to verify one of multiple pending navigations.
ACTION_P6(VerifyPageConcurrentlyStartedContext,
          web_state,
          context_url,
          item_url,
          transition,
          context,
          nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(context_url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(transition, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  // GetPendingItem() returns last pending item. Not item associated with
  // the given navigation context.
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetPendingItem();
  EXPECT_EQ(item_url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for data navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P5(VerifyDataStartedContext,
          web_state,
          url,
          transition,
          context,
          nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(transition, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_TRUE(web_state->IsLoading());
  } else {
    EXPECT_FALSE(web_state->IsLoading());
  }
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for navigation for
// stopped load. Stores |NavigationContext| in |context| pointer.
ACTION_P5(VerifyAbortedNavigationStartedContext,
          web_state,
          url,
          transition,
          context,
          nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(transition, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  EXPECT_FALSE(web_state->IsLoading());
  // Pending Item was removed by Stop call (see crbug.com/969915).
  EXPECT_FALSE(web_state->GetNavigationManager()->GetPendingItem());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P6(VerifyNewPageFinishedContext,
          web_state,
          url,
          mime_type,
          content_is_html,
          context,
          nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  if (!url.SchemeIs(url::kAboutScheme)) {
    ASSERT_TRUE((*context)->GetResponseHeaders());
    std::string actual_mime_type;
    (*context)->GetResponseHeaders()->GetMimeType(&actual_mime_type);
    EXPECT_EQ(mime_type, actual_mime_type);
    EXPECT_EQ(mime_type, actual_mime_type);
  }
  ASSERT_TRUE(web_state->IsLoading());
  ASSERT_EQ(content_is_html, web_state->ContentIsHTML());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for file:// URL
// navigation passed to |DidFinishNavigation|. Asserts that |NavigationContext|
// the same as |context|.
ACTION_P4(VerifyPdfFileUrlFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  ASSERT_FALSE(web_state->ContentIsHTML());
  ASSERT_EQ("application/pdf", web_state->GetContentsMimeType());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for data navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifyDataFinishedContext,
          web_state,
          url,
          mime_type,
          context,
          nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  EXPECT_TRUE(web_state->ContentIsHTML());
  EXPECT_EQ(mime_type, web_state->GetContentsMimeType());
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_TRUE(web_state->IsLoading());
  } else {
    EXPECT_FALSE(web_state->IsLoading());
  }
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for failed navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P6(VerifyErrorFinishedContext,
          web_state,
          url,
          context,
          nav_id,
          committed,
          error_code) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_EQ(committed, (*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  // The error code will be different on bots and for local runs. Allow both.
  NSInteger actual_error_code = (*context)->GetError().code;
  EXPECT_EQ(error_code, actual_error_code);
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  if (web::features::UseWKWebViewLoading()) {
    ASSERT_TRUE(!web_state->IsLoading());
  } else {
    ASSERT_TRUE(web_state->IsLoading());
  }
  ASSERT_FALSE(web_state->ContentIsHTML());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  if (committed) {
    ASSERT_TRUE(item);
    EXPECT_FALSE(item->GetTimestamp().is_null());
    EXPECT_EQ(url, item->GetURL());
  } else {
    EXPECT_FALSE(item);
  }
}

// Verifies correctness of |NavigationContext| (|arg1|) passed to
// |DidFinishNavigation| for navigation canceled due to a rejected response.
// Asserts that |NavigationContext| the same as |context|.
ACTION_P4(VerifyResponseRejectedFinishedContext,
          web_state,
          url,
          context,
          nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  // When the response is rejected discard non committed items is called and
  // no item should be committed.
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  ASSERT_FALSE(web_state->IsLoading());
  ASSERT_FALSE(web_state->ContentIsHTML());
}

// Verifies correctness of |NavigationContext| (|arg1|) for navigations via POST
// HTTP methods passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P6(VerifyPostStartedContext,
          web_state,
          url,
          has_user_gesture,
          context,
          nav_id,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_EQ(has_user_gesture, (*context)->HasUserGesture());
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_TRUE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_EQ(renderer_initiated, (*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  // TODO(crbug.com/676129): Reload does not create a pending item. Remove this
  // workaround once the bug is fixed. The slim navigation manager fixes this
  // bug.
  if (GetWebClient()->IsSlimNavigationManagerEnabled() ||
      !ui::PageTransitionTypeIncludingQualifiersIs(
          ui::PageTransition::PAGE_TRANSITION_RELOAD,
          (*context)->GetPageTransition())) {
    NavigationManager* navigation_manager = web_state->GetNavigationManager();
    NavigationItem* item = navigation_manager->GetPendingItem();
    EXPECT_EQ(url, item->GetURL());
  }
}

// Verifies correctness of |NavigationContext| (|arg1|) for navigations via POST
// HTTP methods passed to |DidFinishNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P6(VerifyPostFinishedContext,
          web_state,
          url,
          has_user_gesture,
          context,
          nav_id,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_EQ(has_user_gesture, (*context)->HasUserGesture());
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_TRUE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_EQ(renderer_initiated, (*context)->IsRendererInitiated());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for same page navigation
// passed to |DidFinishNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P7(VerifySameDocumentStartedContext,
          web_state,
          url,
          has_user_gesture,
          context,
          nav_id,
          page_transition,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_EQ(has_user_gesture, (*context)->HasUserGesture());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
}

// Verifies correctness of |NavigationContext| (|arg1|) for same page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P7(VerifySameDocumentFinishedContext,
          web_state,
          url,
          has_user_gesture,
          context,
          nav_id,
          page_transition,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_EQ(has_user_gesture, (*context)->HasUserGesture());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// to native URLs passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P4(VerifyNewNativePageStartedContext, web_state, url, context, nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetPendingItem();
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// to native URLs passed to |DidFinishNavigation|. Asserts that
// |NavigationContext| the same as |context|.
ACTION_P4(VerifyNewNativePageFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for reload navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P4(VerifyReloadStartedContext, web_state, url, context, nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  // TODO(crbug.com/676129): Reload does not create a pending item. Check
  // pending item once the bug is fixed. The slim navigation manager fixes this
  // bug.
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    NavigationManager* navigation_manager = web_state->GetNavigationManager();
    NavigationItem* item = navigation_manager->GetPendingItem();
    EXPECT_EQ(url, item->GetURL());
  } else {
    EXPECT_FALSE(web_state->GetNavigationManager()->GetPendingItem());
  }
}

// Verifies correctness of |NavigationContext| (|arg1|) for reload navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifyReloadFinishedContext,
          web_state,
          url,
          context,
          nav_id,
          is_web_page) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  if (is_web_page) {
    ASSERT_TRUE((*context)->GetResponseHeaders());
    std::string mime_type;
    (*context)->GetResponseHeaders()->GetMimeType(&mime_type);
    EXPECT_EQ(kExpectedMimeType, mime_type);
  } else {
    EXPECT_FALSE((*context)->GetResponseHeaders());
  }
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for download navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P4(VerifyDownloadFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE((*context)->HasUserGesture());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_TRUE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
}

// Verifies correctness of |NavigationContext| (|arg1|) for restoration
// navigation passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P4(VerifyRestorationStartedContext, web_state, url, context, nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/877671): restoration navigation should be
    // browser-initiated and should have user gesture.
    EXPECT_FALSE((*context)->HasUserGesture());
  } else {
    EXPECT_TRUE((*context)->HasUserGesture());
  }
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      ui::PageTransition::PAGE_TRANSITION_RELOAD, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/877671): restoration navigation should be
    // browser-initiated.
    EXPECT_TRUE((*context)->IsRendererInitiated());
  } else {
    EXPECT_FALSE((*context)->IsRendererInitiated());
  }
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());

  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  ASSERT_TRUE(navigation_manager->GetPendingItem());
  EXPECT_EQ(url, navigation_manager->GetPendingItem()->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for restoration
// navigation passed to |DidFinishNavigation|. Asserts that |NavigationContext|
// the same as |context|.
ACTION_P5(VerifyRestorationFinishedContext,
          web_state,
          url,
          mime_type,
          context,
          nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/877671): restoration navigation should be
    // browser-initiated and should have user gesture.
    EXPECT_FALSE((*context)->HasUserGesture());
  } else {
    EXPECT_TRUE((*context)->HasUserGesture());
  }
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      ui::PageTransition::PAGE_TRANSITION_RELOAD, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/877671): restoration navigation should be
    // browser-initiated.
    EXPECT_TRUE((*context)->IsRendererInitiated());
  } else {
    EXPECT_FALSE((*context)->IsRendererInitiated());
  }
  ASSERT_TRUE((*context)->GetResponseHeaders());
  std::string actual_mime_type;
  (*context)->GetResponseHeaders()->GetMimeType(&actual_mime_type);
  ASSERT_TRUE(web_state->IsLoading());
  EXPECT_EQ(mime_type, actual_mime_type);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// A Google Mock matcher which matches |target_frame_is_main| member of
// WebStatePolicyDecider::RequestInfo. This is needed because
// WebStatePolicyDecider::RequestInfo doesn't support operator==.
MATCHER_P(RequestInfoMatch, expected_request_info, /*description=*/"") {
  bool transition_type_match = ui::PageTransitionTypeIncludingQualifiersIs(
      arg.transition_type, expected_request_info.transition_type);
  EXPECT_TRUE(transition_type_match)
      << "expected transition type: "
      << PageTransitionGetCoreTransitionString(
             expected_request_info.transition_type)
      << " actual transition type: "
      << PageTransitionGetCoreTransitionString(arg.transition_type);
  EXPECT_EQ(expected_request_info.target_frame_is_main,
            arg.target_frame_is_main);
  EXPECT_EQ(expected_request_info.has_user_gesture, arg.has_user_gesture);

  return transition_type_match &&
         arg.target_frame_is_main ==
             expected_request_info.target_frame_is_main &&
         arg.has_user_gesture == expected_request_info.has_user_gesture;
}

// A GMock matcher that matches |URL| member of |arg| with |expected_url|. |arg|
// is expected to be either an NSURLRequest or NSURLResponse.
MATCHER_P(URLMatch, expected_url, /*description=*/"") {
  return expected_url == net::GURLWithNSURL(arg.URL);
}

// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public WebStateObserver {
 public:
  WebStateObserverMock() = default;

  MOCK_METHOD2(NavigationItemsPruned, void(WebState*, size_t));
  MOCK_METHOD2(DidStartNavigation, void(WebState*, NavigationContext*));
  MOCK_METHOD2(DidFinishNavigation, void(WebState*, NavigationContext*));
  MOCK_METHOD1(DidStartLoading, void(WebState*));
  MOCK_METHOD1(DidStopLoading, void(WebState*));
  MOCK_METHOD2(PageLoaded, void(WebState*, PageLoadCompletionStatus));
  MOCK_METHOD1(DidChangeBackForwardState, void(WebState*));
  MOCK_METHOD1(TitleWasSet, void(WebState*));
  void WebStateDestroyed(WebState* web_state) override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverMock);
};

// Mocks WebStatePolicyDecider decision callbacks.
class PolicyDeciderMock : public WebStatePolicyDecider {
 public:
  PolicyDeciderMock(WebState* web_state) : WebStatePolicyDecider(web_state) {}
  MOCK_METHOD2(ShouldAllowRequest,
               bool(NSURLRequest*,
                    const WebStatePolicyDecider::RequestInfo& request_info));
  MOCK_METHOD2(ShouldAllowResponse, bool(NSURLResponse*, bool for_main_frame));
};

}  // namespace

using net::test_server::EmbeddedTestServer;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using test::WaitForWebViewContainingText;

// Test fixture to test navigation and load callbacks from WebStateObserver and
// WebStatePolicyDecider.
class WebStateObserverTest
    : public WebIntTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 public:
  WebStateObserverTest() : scoped_observer_(&observer_) {}

  void SetUp() override {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSlimNavigationManager);
    }

    WebIntTest::SetUp();
    decider_ = std::make_unique<StrictMock<PolicyDeciderMock>>(web_state());
    scoped_observer_.Add(web_state());

    // Stub out NativeContent objects.
    provider_ = [[TestNativeContentProvider alloc] init];
    content_ = [[TestNativeContent alloc] initWithURL:GURL::EmptyGURL()
                                           virtualURL:GURL::EmptyGURL()];

    WebStateImpl* web_state_impl = reinterpret_cast<WebStateImpl*>(web_state());
    [web_state_impl->GetWebController() nativeContentHolder].nativeProvider =
        provider_;

    test_server_ = std::make_unique<EmbeddedTestServer>();
    test_server_->RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/form",
                            base::BindRepeating(::testing::HandleForm)));
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/download",
        base::BindRepeating(::testing::HandleDownload)));
    RegisterDefaultHandlers(test_server_.get());
    test_server_->ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_->Start());
  }

  void TearDown() override {
    scoped_observer_.RemoveAll();
    WebIntTest::TearDown();
  }

 protected:
  TestNativeContentProvider* provider_;
  TestNativeContent* content_;
  std::unique_ptr<StrictMock<PolicyDeciderMock>> decider_;
  StrictMock<WebStateObserverMock> observer_;
  std::unique_ptr<EmbeddedTestServer> test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedObserver<WebState, WebStateObserver> scoped_observer_;
  ::testing::InSequence callbacks_sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(WebStateObserverTest);
};

// Tests successful navigation to a new page.
TEST_P(WebStateObserverTest, NewPageNavigation) {
  const GURL url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
}

// Tests loading about://newtab and immediately loading another web page without
// waiting until about://newtab navigation finishes.
TEST_P(WebStateObserverTest, AboutNewTabNavigation) {
  GURL first_url("about://newtab/");
  const GURL second_url = test_server_->GetURL("/echoall");

  // Perform about://newtab navigation and immediately perform the second
  // navigation without waiting until the first navigation finishes.

  // Load |first_url|.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));

  // WKWebView.URL will change from |first_url| to |second_url|, then to nil
  // while WKWebView.loading changing to false and back to true immediately,
  // then to |first_url| again and the first navigation will finish.
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));

  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageConcurrentlyStartedContext(
          web_state(), first_url, second_url,
          ui::PageTransition::PAGE_TRANSITION_TYPED, &context, &nav_id));

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), first_url, /*mime_type=*/std::string(),
          /*content_is_html=*/false, &context, &nav_id));
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  // Load |second_url|.
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), second_url, ui::PageTransition::PAGE_TRANSITION_TYPED,
          &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), second_url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(second_url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  // Finish loading |second_url|.
  __block bool page_loaded = false;
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS))
      .WillOnce(::testing::Assign(&page_loaded, true));

  test::LoadUrl(web_state(), first_url);
  test::LoadUrl(web_state(), second_url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return page_loaded;
  }));
}

// Tests that if web usage is already enabled, enabling it again would not cause
// any page loads (related to restoring cached session). This is a regression
// test for crbug.com/781916.
TEST_P(WebStateObserverTest, EnableWebUsageTwice) {
  const GURL url = test_server_->GetURL("/echo");

  // Only expect one set of load events from the first LoadUrl(), not subsequent
  // SetWebUsageEnabled(true) calls. Web usage is already enabled, so the
  // subsequent calls should be noops.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  ASSERT_TRUE(LoadUrl(url));
  web_state()->SetWebUsageEnabled(true);
  web_state()->SetWebUsageEnabled(true);
}

// Tests failed navigation to a new page.
TEST_P(WebStateObserverTest, FailedNavigation) {
  const GURL url = test_server_->GetURL("/close-socket");

  // Perform a navigation to url with unsupported scheme, which will fail.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
    // Load placeholder by [WKWebView loadRequest].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyErrorFinishedContext(web_state(), url, &context, &nav_id,
                                           /*committed=*/true,
                                           NSURLErrorNetworkConnectionLost));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));

  if (web::features::UseWKWebViewLoading()) {
    // Load error page HTML by [WKWebView loadHTMLString:baseURL:].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  test::LoadUrl(web_state(), url);

  // Ensure that title is not overridden by a placeholder navigation.
  web::NavigationManager* manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();
  item->SetTitle(base::UTF8ToUTF16(kFailedTitle));
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), url, "NSURLErrorDomain",
                                         /*error_code=*/-1005,
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*has_ssl_info=*/false)));
  DCHECK_EQ(item->GetTitle(), base::UTF8ToUTF16(kFailedTitle));
}

// Tests navigation to a URL with /..; suffix. On iOS 12 and earlier this
// navigation fails becasue WebKit rewrites valid URL to invalid during the
// navigation. On iOS 13+ this navigation sucessfully completes.
TEST_P(WebStateObserverTest, UrlWithSpecialSuffixNavigation) {
  const std::string kBadSuffix = "/..;";
  GURL url = test_server_->GetURL(kBadSuffix);

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));

  if (@available(iOS 13, *)) {
    // Starting from iOS 13 WebKit, does not rewrite URL.
    WebStatePolicyDecider::RequestInfo expected_request_info(
        ui::PageTransition::PAGE_TRANSITION_TYPED,
        /*target_main_frame=*/true, /*has_user_gesture=*/false);
    EXPECT_CALL(*decider_,
                ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
        .WillOnce(Return(true));
    EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
        .WillOnce(VerifyPageStartedContext(
            web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED,
            &context, &nav_id));
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
    EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
        .WillOnce(VerifyNewPageFinishedContext(
            web_state(), url, /*mime_type*/ std::string(),
            /*content_is_html=*/false, &context, &nav_id));
    EXPECT_CALL(observer_, TitleWasSet(web_state()))
        .WillOnce(VerifyTitle(url.GetContent()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));

    EXPECT_CALL(observer_,
                PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

    ASSERT_TRUE(LoadUrl(url));
    EXPECT_EQ(url, web_state()->GetVisibleURL());
  } else {
    // Perform a navigation to a url, which will be rewritten by WebKit to
    // invalid and will fail.
    std::string webkit_rewritten_url_spec = url.spec();
    // Prior to iOS 13, WebKit rewrites "http://127.0.0.1:80/..;" (valid) to
    // "http://127.0.0.1:80;/" (invalid).
    webkit_rewritten_url_spec.replace(url.spec().size() - kBadSuffix.size(),
                                      kBadSuffix.size(), ";/");

    WebStatePolicyDecider::RequestInfo expected_request_info(
        // Can't match NavigationContext and determine navigation type prior to
        // iOS 13, because context is matched by URL, which is rewritten by
        // WebKit.
        ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
        /*target_main_frame=*/true, /*has_user_gesture=*/false);
    EXPECT_CALL(*decider_,
                ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
        .WillOnce(Return(true));

    // WKWebView.URL changes from |url| nil and then to rewritten URL, while
    // WKWebView.loading changes from true to false and then back to true.
    if (web::features::UseWKWebViewLoading()) {
      EXPECT_CALL(observer_, DidStopLoading(web_state()));
      EXPECT_CALL(observer_, DidStartLoading(web_state()));
    }

    EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
        .WillOnce(VerifyPageStartedContext(
            web_state(), GURL(webkit_rewritten_url_spec),
            ui::PageTransition::PAGE_TRANSITION_TYPED, &context, &nav_id));

    if (web::features::UseWKWebViewLoading()) {
      EXPECT_CALL(observer_, DidStopLoading(web_state()));
    }

    EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
        .WillOnce(VerifyErrorFinishedContext(
            web_state(), GURL(webkit_rewritten_url_spec), &context, &nav_id,
            /*committed=*/false, kWebKitErrorCannotShowUrl));

    if (!web::features::UseWKWebViewLoading()) {
      EXPECT_CALL(observer_, DidStopLoading(web_state()));
    }
    EXPECT_CALL(observer_,
                PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));

    ASSERT_TRUE(LoadUrl(url));
    EXPECT_EQ("", web_state()->GetVisibleURL());
  }
}

// Tests failed navigation because URL scheme is not supported by WKWebView.
TEST_P(WebStateObserverTest, WebViewUnsupportedSchemeNavigation) {
  GURL url(url::SchemeHostPort(kTestAppSpecificScheme, "foo", 0).Serialize());

  // Perform a navigation to url with unsupported scheme, which will fail.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
    // Load placeholder by [WKWebView loadRequest].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyErrorFinishedContext(web_state(), url, &context, &nav_id,
                                           /*committed=*/true,
                                           NSURLErrorUnsupportedURL));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));

  if (web::features::UseWKWebViewLoading()) {
    // Load error page HTML by [WKWebView loadHTMLString:baseURL:].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), url, "NSURLErrorDomain",
                                         /*error_code=*/-1002,
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*has_ssl_info=*/false)));
}

// Tests failed navigation because URL with a space is not supported by
// WKWebView (crbug.com/934379).
TEST_P(WebStateObserverTest, WebViewUnsupportedUrlNavigation) {
  GURL url("http:// .test");

  // Perform a navigation to url with unsupported url, which will fail.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
    // Load placeholder by [WKWebView loadRequest].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyErrorFinishedContext(web_state(), url, &context, &nav_id,
                                           /*committed=*/true,
                                           web::kWebKitErrorCannotShowUrl));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));

  if (web::features::UseWKWebViewLoading()) {
    // Load error page HTML by [WKWebView loadHTMLString:baseURL:].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), url, "WebKitErrorDomain",
                                         /*error_code=*/101,
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*has_ssl_info=*/false)));
}

// Tests failed navigation because URL scheme is not supported by WebState.
TEST_P(WebStateObserverTest, WebStateUnsupportedSchemeNavigation) {
  GURL url("ftp://foo.test/");

  // Perform a navigation to url with unsupported scheme.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  // ShouldAllowRequest is called to give embedder a chance to handle
  // this unsupported URL scheme.
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Typed URL should be discarded after the navigation is rejected.
  EXPECT_FALSE(web_state()->GetNavigationManager()->GetVisibleItem());
}

// Tests web page reload navigation.
TEST_P(WebStateObserverTest, WebPageReloadNavigation) {
  const GURL url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Reload web page.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo reload_request_info(
      ui::PageTransition::PAGE_TRANSITION_RELOAD,
      /*target_main_frame=*/true,
      /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(reload_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(
          VerifyReloadStartedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context, &nav_id,
                                            true /* is_web_page */));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 /*check_for_repost=*/false);
  }));
}

// Tests web page reload with user agent override.
TEST_P(WebStateObserverTest, ReloadWithUserAgentType) {
  // TODO(crbug.com/851119): temporarily disable this failing test.
  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    return;
  }
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Reload web page with desktop user agent.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_reload_request_info(
      ui::PageTransition::PAGE_TRANSITION_RELOAD,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_, ShouldAllowRequest(
                             _, RequestInfoMatch(expected_reload_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_RELOAD,
          &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  // TODO(crbug.com/798836): verify the correct User-Agent header is sent.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);
  }));
}

// Tests user-initiated hash change.
TEST_P(WebStateObserverTest, UserInitiatedHashChangeNavigation) {
  const GURL url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform same-document navigation.
  const GURL hash_url = test_server_->GetURL("/echoall#1");
  WebStatePolicyDecider::RequestInfo hash_url_expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);

  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(
      *decider_,
      ShouldAllowRequest(_, RequestInfoMatch(hash_url_expected_request_info)))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, /*has_user_gesture=*/true, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));

  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, /*has_user_gesture=*/true, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(hash_url));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  // Perform same-document navigation by going back.
  // No ShouldAllowRequest callback for same-document back-forward navigations.
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Called once each for CanGoBack and CanGoForward.
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
  } else {
    // TODO(crbug.com/913813): This callback is not really needed, but Page Load
    // Count metric relies on it DidStartLoading.
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  ui::PageTransition expected_transition = static_cast<ui::PageTransition>(
      ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK |
      ui::PageTransition::PAGE_TRANSITION_TYPED);
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), url, /*has_user_gesture=*/true, &context, &nav_id,
          expected_transition, /*renderer_initiated=*/false));

  // No ShouldAllowResponse callbacks for same-document back-forward
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), url, /*has_user_gesture=*/true, &context, &nav_id,
          expected_transition, /*renderer_initiated=*/false));

  if (!GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/913813): This callback is not really needed, but Page Load
    // Count metric relies on it DidStartLoading.
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  }));
}

// Tests renderer-initiated hash change.
TEST_P(WebStateObserverTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = test_server_->GetURL("/echoall#1");
  WebStatePolicyDecider::RequestInfo expected_hash_request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_, ShouldAllowRequest(
                             _, RequestInfoMatch(expected_hash_request_info)))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, /*has_user_gesture=*/false, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, /*has_user_gesture=*/false, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_P(WebStateObserverTest, StateNavigation) {
  const GURL url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform push state using JavaScript.
  const GURL push_url = test_server_->GetURL("/test.html");
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), push_url, /*has_user_gesture=*/false, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for same-document push
  // state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), push_url, /*has_user_gesture=*/false, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = test_server_->GetURL("/1.html");
  // No ShouldAllowRequest callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), replace_url, /*has_user_gesture=*/false, &context,
          &nav_id, ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), replace_url, /*has_user_gesture=*/false, &context,
          &nav_id, ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_P(WebStateObserverTest, NativeContentNavigation) {
  // UseWKWebViewLoading should be shipped with slim-nav, and native content
  // will be deprecated after slim-nav.
  if (web::features::UseWKWebViewLoading()) {
    return;
  }

  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageStartedContext(web_state(), url, &context,
                                                  &nav_id));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageFinishedContext(web_state(), url, &context,
                                                   &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("Test Title"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  ASSERT_TRUE(LoadUrl(url));
}

// Tests native content reload navigation.
TEST_P(WebStateObserverTest, NativeContentReload) {
  // UseWKWebViewLoading should be shipped after slim-nav, and native content
  // will be deprecated after slim-nav.
  if (web::features::UseWKWebViewLoading()) {
    return;
  }

  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("Test Title"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  ASSERT_TRUE(LoadUrl(url));

  // Reload native content.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  // No ShouldAllowRequest callbacks for native content navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(
          VerifyReloadStartedContext(web_state(), url, &context, &nav_id));
  // No ShouldAllowResponse callbacks for native content navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context, &nav_id,
                                            false /* is_web_page */));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
}

// Tests WasTitleSet callback triggered when navigating back to native content
// from web content.
TEST_P(WebStateObserverTest, GoBackToNativeContent) {
  // UseWKWebViewLoading should be shipped after slim-nav, and native content
  // will be deprecated after slim-nav.
  if (web::features::UseWKWebViewLoading()) {
    return;
  }

  // Load a native content URL.
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("Test Title"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  ASSERT_TRUE(LoadUrl(url));

  // Load a web navigation.
  const GURL web_url = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), web_url, ui::PageTransition::PAGE_TRANSITION_TYPED,
          &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), web_url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(web_url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(web_url));

  // Going back to native content should trigger TitleWasSet.
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
    // TODO(crbug.com/867095): fix this extra callback triggered by placeholder
    // URL load.
    EXPECT_CALL(observer_,
                PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  }
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  // This call is required to make sure WebStateObservers update their cached
  // version of current title.
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("Test Title"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  }));
}

// Tests successful navigation to a new page with post HTTP method.
TEST_P(WebStateObserverTest, UserInitiatedPostNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_GENERATED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(
          web_state(), url, /*has_user_gesture=*/true, &context, &nav_id,
          /*renderer_initiated=*/false));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(
          web_state(), url, /*has_user_gesture=*/true, &context, &nav_id,
          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  // Load request using POST HTTP method.
  NavigationManager::WebLoadParams params(url);
  params.post_data = [@"foo" dataUsingEncoding:NSUTF8StringEncoding];
  params.extra_headers = @{@"Content-Type" : @"text/html"};
  params.transition_type = ui::PageTransition::PAGE_TRANSITION_GENERATED;
  ASSERT_TRUE(LoadWithParams(params));
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), "foo"));
}

// Tests successful navigation to a new page with post HTTP method.
TEST_P(WebStateObserverTest, RendererInitiatedPostNavigation) {
  const GURL url = test_server_->GetURL("/form?echoall");
  const GURL action = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  ;
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(
      WaitForWebViewContainingText(web_state(), ::testing::kTestFormPage));

  // Submit the form using JavaScript.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  WebStatePolicyDecider::RequestInfo form_request_info(
      ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(form_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(
          web_state(), action, /*has_user_gesture=*/false, &context, &nav_id,
          /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(
          web_state(), action, /*has_user_gesture=*/false, &context, &nav_id,
          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(action.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(),
                                           ::testing::kTestFormFieldValue));
}

// Tests successful reload of a page returned for post request.
TEST_P(WebStateObserverTest, ReloadPostNavigation) {
  const GURL url = test_server_->GetURL("/form?echoall");
  const GURL action = test_server_->GetURL("/echoall");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(
      WaitForWebViewContainingText(web_state(), ::testing::kTestFormPage));

  // Submit the form using JavaScript.
  WebStatePolicyDecider::RequestInfo form_request_info(
      ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(form_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(action.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(),
                                           ::testing::kTestFormFieldValue));

  // Reload the page.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // ShouldAllowRequest() not called because SlimNavigationManager catches
    // repost before calling policy decider.
  } else {
    WebStatePolicyDecider::RequestInfo form_reload_request_info(
        ui::PageTransition::PAGE_TRANSITION_RELOAD,
        /*target_main_frame=*/true, /*has_user_gesture=*/false);

    EXPECT_CALL(*decider_, ShouldAllowRequest(
                               _, RequestInfoMatch(form_reload_request_info)))
        .WillOnce(Return(true));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(
          web_state(), action, /*has_user_gesture=*/true, &context, &nav_id,
          /*reload_is_renderer_initiated=*/false));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(
          web_state(), action, /*has_user_gesture=*/true, &context, &nav_id,
          /*reload_is_renderer_initiated=*/false));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(action.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 false /*check_for_repost*/);
  }));
}

// Tests going forward to a page rendered from post response.
TEST_P(WebStateObserverTest, ForwardPostNavigation) {
  const GURL url = test_server_->GetURL("/form?echo");
  const GURL action = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(
      WaitForWebViewContainingText(web_state(), ::testing::kTestFormPage));

  // Submit the form using JavaScript.
  WebStatePolicyDecider::RequestInfo form_request_info(
      ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(form_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(action.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(),
                                           ::testing::kTestFormFieldValue));

  // Go Back.
  WebStatePolicyDecider::RequestInfo back_request_info(
      static_cast<ui::PageTransition>(
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK |
          ui::PageTransition::PAGE_TRANSITION_TYPED),
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
  }
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(back_request_info)))
      .WillOnce(Return(true));

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  }));

  // Go forward.
  WebStatePolicyDecider::RequestInfo forward_request_info(
      static_cast<ui::PageTransition>(
          ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT |
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK),
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
  }
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(forward_request_info)))
      .WillOnce(Return(true));

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action,
                                         /*has_user_gesture=*/true, &context,
                                         &nav_id,
                                         /*renderer_initiated=*/false));

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action,
                                          /*has_user_gesture=*/true, &context,
                                          &nav_id,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(action.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->GoForward();
  }));
}

// Tests server redirect navigation.
TEST_P(WebStateObserverTest, RedirectNavigation) {
  const GURL url = test_server_->GetURL("/server-redirect-301?"
                                        "server-redirect-302?"
                                        "server-redirect-303?"
                                        "server-redirect-307?"
                                        "server-redirect-308?"
                                        "echoall");
  const GURL redirect_url = test_server_->GetURL("/echoall");

  // Load url which replies with redirect.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));

  // 5 calls on ShouldAllowRequest for redirections.
  WebStatePolicyDecider::RequestInfo expected_redirect_request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(
      *decider_,
      ShouldAllowRequest(_, RequestInfoMatch(expected_redirect_request_info)))
      .Times(5)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), redirect_url, kExpectedMimeType,
          /*content_is_html=*/true, &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(redirect_url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
}

// Tests download navigation.
TEST_P(WebStateObserverTest, DownloadNavigation) {
  GURL url = test_server_->GetURL("/download");

  // Perform download navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(
          VerifyDownloadFinishedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  EXPECT_FALSE(web_state()->GetNavigationManager()->GetPendingItem());
}

// Tests failed load after the navigation is sucessfully finished.
// TODO(crbug.com/954232): this test is flaky on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FailedLoad FailedLoad
#else
#define MAYBE_FailedLoad FLAKY_FailedLoad
#endif
TEST_P(WebStateObserverTest, MAYBE_FailedLoad) {
  GURL url = test_server_->GetURL("/exabyte_response");

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, /*mime_type=*/"",
                                             /*content_is_html=*/false,
                                             &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));

  if (web::features::UseWKWebViewLoading()) {
    // Load error page HTML by [WKWebView loadHTMLString:baseURL:].
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  test::LoadUrl(web_state(), url);

  // Server will never stop responding. Wait until the navigation is committed.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return context && context->HasCommitted();
  }));

  // At this point the navigation should be finished. Shutdown the server and
  // wait until web state stop loading.
  ASSERT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));
}

// Tests navigation to a page with self signed SSL cert.
TEST_P(WebStateObserverTest, FailedSslConnection) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());

  const GURL url = https_server.GetURL("/");
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, RequestInfoMatch(request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  // TODO(crbug.com/921916): DidFinishNavigation is not called for SSL errors.
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !web_state()->IsLoading();
  }));
}

// Tests rejecting the navigation from ShouldAllowRequest. The load should stop,
// but no other callbacks are called.
TEST_P(WebStateObserverTest, DisallowRequest) {
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  test::LoadUrl(web_state(), test_server_->GetURL("/echo"));
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Typed URL should be discarded after the navigation is rejected.
  EXPECT_FALSE(web_state()->GetNavigationManager()->GetVisibleItem());
}

// Tests rejecting the navigation from ShouldAllowResponse. PageLoaded callback
// is not called.
TEST_P(WebStateObserverTest, DisallowResponse) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyResponseRejectedFinishedContext(web_state(), url,
                                                      &context, &nav_id));
  test::LoadUrl(web_state(), test_server_->GetURL("/echo"));
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ("", web_state()->GetVisibleURL());
}

// Tests stopping a navigation. Did FinishLoading and PageLoaded are never
// called.
TEST_P(WebStateObserverTest, ImmediatelyStopNavigation) {
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  test::LoadUrl(web_state(), test_server_->GetURL("/hung"));
  web_state()->Stop();
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ("", web_state()->GetVisibleURL());
}

// Tests stopping a navigation after allowing the navigation from
// WebStatePolicyDecider. DidStartNavigation and DidFinishNavigation callbacks
// are still called on simulator. On iOS 12.2 device the navigation never
// starts, which is ok, but the behavior can not be tested so the test is
// simulator-only.
#if TARGET_IPHONE_SIMULATOR
TEST_P(WebStateObserverTest, StopNavigationAfterPolicyDeciderCallback) {
  if (@available(iOS 13, *)) {
    // The navigation may or may not start, which is ok, but there is no need to
    // test this scenario.
    return;
  }

  GURL url(test_server_->GetURL("/hung"));
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(ReturnTrueAndStopNavigation(web_state()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyAbortedNavigationStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyResponseRejectedFinishedContext(web_state(), url,
                                                      &context, &nav_id));

  // Load the page and wait for DidFinishNavigation callback.
  TestWebStateObserver page_loaded_observer(web_state());
  test::LoadUrl(web_state(), url);
  TestWebStateObserver* page_loaded_observer_ptr = &page_loaded_observer;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return page_loaded_observer_ptr->did_finish_navigation_info();
  }));
}
#endif  // TARGET_IPHONE_SIMULATOR

// Tests stopping a finished navigation. PageLoaded is never called.
TEST_P(WebStateObserverTest, StopFinishedNavigation) {
  GURL url = test_server_->GetURL("/exabyte_response");

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, /*mime_type=*/"",
                                             /*content_is_html=*/false,
                                             &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  test::LoadUrl(web_state(), url);

  // Server will never stop responding. Wait until the navigation is committed.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return context && context->HasCommitted();
  }));

  // Stop the loading.
  web_state()->Stop();
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));
}

// Tests that iframe navigation triggers DidChangeBackForwardState.
TEST_P(WebStateObserverTest, IframeNavigation) {
  // LegacyNavigationManager doesn't support iframe navigation history.
  if (!GetWebClient()->IsSlimNavigationManagerEnabled())
    return;

  GURL url = test_server_->GetURL("/iframe_host.html");

  // Callbacks due to loading of the main frame.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()));

  // Callbacks due to initial loading of iframe.
  WebStatePolicyDecider::RequestInfo iframe_request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_main_frame=*/false, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(iframe_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/false))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Trigger different-document load in iframe.
  WebStatePolicyDecider::RequestInfo link_clicked_request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_main_frame=*/false, /*has_user_gesture=*/true);
  EXPECT_CALL(*decider_, ShouldAllowRequest(
                             _, RequestInfoMatch(link_clicked_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/false))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  test::TapWebViewElementWithIdInIframe(web_state(), "normal-link");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    id URL = ExecuteJavaScript(@"window.frames[0].location.pathname;");
    return [@"/pony.html" isEqual:URL] == YES;
  }));
  ASSERT_TRUE(web_state()->GetNavigationManager()->CanGoBack());
  ASSERT_FALSE(web_state()->GetNavigationManager()->CanGoForward());
  id history_length = ExecuteJavaScript(@"history.length;");
  ASSERT_NSEQ(@2, history_length);

  // Go back to top.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo forward_back_request_info(
      ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
      /*target_main_frame=*/false, /*has_user_gesture=*/true);
  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()))
      .Times(2);  // called once each for canGoBack and canGoForward
  EXPECT_CALL(*decider_, ShouldAllowRequest(
                             _, RequestInfoMatch(forward_back_request_info)))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/false))
      .WillOnce(Return(true));

  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  web_state()->GetNavigationManager()->GoBack();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    id URL = ExecuteJavaScript(@"window.frames[0].location.pathname;");
    return [@"/links.html" isEqual:URL] == YES;
  }));
  ASSERT_TRUE(web_state()->GetNavigationManager()->CanGoForward());
  ASSERT_FALSE(web_state()->GetNavigationManager()->CanGoBack());

  // Trigger same-document load in iframe.
  EXPECT_CALL(*decider_, ShouldAllowRequest(
                             _, RequestInfoMatch(link_clicked_request_info)))
      .WillOnce(Return(true));
  // ShouldAllowResponse() is not called for same-document navigation.
  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()))
      .Times(2);  // called once each for canGoBack and canGoForward
  test::TapWebViewElementWithIdInIframe(web_state(), "same-page-link");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetNavigationManager()->CanGoBack();
  }));
  EXPECT_FALSE(web_state()->GetNavigationManager()->CanGoForward());
}

// Tests that new page load calls NavigationItemsPruned callback if there were
// forward navigation items.
TEST_P(WebStateObserverTest, NewPageLoadDestroysForwardItems) {
  // Perform first navigation.
  const GURL first_url = test_server_->GetURL("/echoall");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state())).Times(2);
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(first_url));

  // Perform second navigation.
  const GURL hash_url = test_server_->GetURL("/echoall#1");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));
  }
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  if (!web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(hash_url));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  // Go back to create forward navigation items.
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Called once each for CanGoBack and CanGoForward;
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
  } else {
    // TODO(crbug.com/913813): This callback is not really needed, but Page Load
    // Count metric relies on it DidStartLoading.
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowResponse callbacks for same-document back-forward
  // navigations.

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  if (!GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/913813): This callback is not really needed, but Page Load
    // Count metric relies on it DidStartLoading.
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(first_url, ^{
    navigation_manager()->GoBack();
  }));

  // New page load destroys forward navigation entries.
  const GURL url = test_server_->GetURL("/echo");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Called once each for CanGoBack and CanGoForward;
    EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(2);
  } else {
    // TODO(crbug.com/910894): NavigationItemsPruned is not called if
    // slim-navigation-manager feature is enabled.
    EXPECT_CALL(observer_,
                NavigationItemsPruned(web_state(), /*pruned_item_count=*/1));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
}

// Verifies that WebState::CreateWithStorageSession does not call any
// WebStateObserver callbacks.
// TODO(crbug.com/738020): Remove this test after deprecating legacy navigation
// manager. Restore session in slim navigation manager is better tested in
// RestoreSessionOnline.
TEST_P(WebStateObserverTest, RestoreSession) {
  // Create session storage.
  CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
  GURL url(test_server_->GetURL("/echo"));
  item.virtualURL = url;
  NSArray<CRWNavigationItemStorage*>* item_storages = @[ item ];

  // Create the session with storage and add observer.
  WebState::CreateParams params(GetBrowserState());
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = item_storages;
  auto web_state = WebState::CreateWithStorageSession(params, session_storage);
  web_state->SetKeepRenderProcessAlive(true);

  StrictMock<WebStateObserverMock> observer;
  ScopedObserver<WebState, WebStateObserver> scoped_observer(&observer);
  scoped_observer.Add(web_state.get());

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  // Load restore_session.html with session history.
  EXPECT_CALL(observer, DidStartLoading(web_state.get()));
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer, DidStopLoading(web_state.get()));
    // Load restore_session.html with targetUrl=|url|.
    EXPECT_CALL(observer, DidStartLoading(web_state.get()));
    EXPECT_CALL(observer, DidStopLoading(web_state.get()));
    // Load restored |url|.
    EXPECT_CALL(observer, DidStartLoading(web_state.get()));
  }

  EXPECT_CALL(observer, DidStartNavigation(web_state.get(), _))
      .WillOnce(VerifyRestorationStartedContext(web_state.get(), url, &context,
                                                &nav_id));

  EXPECT_CALL(observer, DidFinishNavigation(web_state.get(), _))
      .WillOnce(VerifyRestorationFinishedContext(
          web_state.get(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer, TitleWasSet(web_state.get()))
      .WillOnce(VerifyTitle(url.GetContent()));
  EXPECT_CALL(observer, DidStopLoading(web_state.get()));

  __block bool page_loaded = false;
  EXPECT_CALL(observer,
              PageLoaded(web_state.get(), PageLoadCompletionStatus::SUCCESS))
      .WillOnce(::testing::Assign(&page_loaded, true));

  // Trigger the session restoration.
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and replace this call with
  // SessionStorageBuilder::ExtractSessionState().
  navigation_manager->LoadIfNecessary();

  // Wait until the session is restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return navigation_manager->GetItemCount() == 1;
  }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    web::NavigationItem* item = navigation_manager->GetLastCommittedItem();
    return item && item->GetURL() == url;
  }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return page_loaded;
  }));
}

// Tests callbacks for restoring session and subsequently going back to
// about:blank.
TEST_P(WebStateObserverTest, RestoreSessionOnline) {
  // LegacyNavigationManager doesn't trigger load in Restore.
  if (!GetWebClient()->IsSlimNavigationManagerEnabled()) {
    return;
  }

  // Create a session of 3 items. Current item is at index 1.
  const GURL url0("about:blank");
  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(url0);

  const GURL url1 = test_server_->GetURL("/echo?1");
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(url1);

  const GURL url2 = test_server_->GetURL("/echo?2");
  auto item2 = std::make_unique<NavigationItemImpl>();
  item2->SetURL(url2);

  __block std::vector<std::unique_ptr<NavigationItem>> restored_items;
  restored_items.push_back(std::move(item0));
  restored_items.push_back(std::move(item1));
  restored_items.push_back(std::move(item2));

  // Initiate session restoration.

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));

  // Back/forward state changes due to History API calls during session
  // restoration. Called once each for CanGoBack and CanGoForward.
  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state()));

  // Client-side redirect to restore_session.html?targetUrl=url1.
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(URLMatch(CreateRedirectUrl(url1)), _))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  EXPECT_CALL(*decider_, ShouldAllowResponse(URLMatch(CreateRedirectUrl(url1)),
                                             /*for_main_frame=*/true))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  // Client-side redirect to |url1|.
  EXPECT_CALL(*decider_, ShouldAllowRequest(URLMatch(url1), _))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_,
              ShouldAllowResponse(URLMatch(url1), /*for_main_frame=*/true))
      .WillOnce(Return(true));

  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, TitleWasSet(web_state()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url1, ^{
    navigation_manager()->Restore(/*last_committed_item_index=*/1,
                                  std::move(restored_items));
  }));
  ASSERT_EQ(url1, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_TRUE(navigation_manager()->CanGoBack());
  ASSERT_TRUE(navigation_manager()->CanGoForward());

  // Go back to |item0|.

  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  // Only CanGoBackward changes state on this navigation.
  EXPECT_CALL(observer_, DidChangeBackForwardState(web_state())).Times(1);

  // Load restore_session.html?targetUrl=url0.
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(URLMatch(CreateRedirectUrl(url0)), _))
      .WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(URLMatch(CreateRedirectUrl(url0)),
                                             /*for_main_frame=*/true))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }

  // Client-side redirect to |url0|.
  EXPECT_CALL(*decider_, ShouldAllowRequest(URLMatch(url0), _))
      .WillOnce(Return(true));

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowResponse call because about:blank has no response.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url0, ^{
    navigation_manager()->GoBack();
  }));
  ASSERT_EQ(url0, navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager()->CanGoForward());
  ASSERT_FALSE(navigation_manager()->CanGoBack());
}

// Tests that if a saved session is provided when creating a new WebState, it is
// restored after the first NavigationManager::LoadIfNecessary() call.
TEST_P(WebStateObserverTest, RestoredFromHistory) {
  auto web_state = WebState::CreateWithStorageSession(
      WebState::CreateParams(GetBrowserState()),
      GetTestSessionStorage(test_server_->GetURL(kTestPageURL)));

  ASSERT_FALSE(test::IsWebViewContainingText(web_state.get(),
                                             kTestSessionStoragePageText));
  web_state->GetNavigationManager()->LoadIfNecessary();
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));
}

// Tests that NavigationManager::LoadIfNecessary() restores the page after
// disabling and re-enabling web usage.
TEST_P(WebStateObserverTest, DisableAndReenableWebUsage) {
  auto web_state = WebState::CreateWithStorageSession(
      WebState::CreateParams(GetBrowserState()),
      GetTestSessionStorage(test_server_->GetURL(kTestPageURL)));
  web_state->GetNavigationManager()->LoadIfNecessary();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));

  web_state->SetWebUsageEnabled(false);
  web_state->SetWebUsageEnabled(true);

  // NavigationManager::LoadIfNecessary() should restore the page.
  ASSERT_FALSE(test::IsWebViewContainingText(web_state.get(),
                                             kTestSessionStoragePageText));
  web_state->GetNavigationManager()->LoadIfNecessary();
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));
}

// Tests successful navigation to a PDF file:// URL.
TEST_P(WebStateObserverTest, PdfFileUrlNavigation) {
  // Construct a valid file:// URL.
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  path = path.Append(
      FILE_PATH_LITERAL("ios/testing/data/http_server_files/testpage.pdf"));

  GURL url(url::SchemeHostPort(url::kFileScheme, std::string(), 0).Serialize());
  GURL::Replacements replacements;
  replacements.SetPathStr(path.value());
  url = url.ReplaceComponents(replacements);
  ASSERT_TRUE(url.is_valid());
  ASSERT_FALSE(url.is_empty());

  // Perform file:// URL navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(
          VerifyPdfFileUrlFinishedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("testpage.pdf"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PageTransition::PAGE_TRANSITION_TYPED;
  params.virtual_url =
      GURL(url::SchemeHostPort(kTestAppSpecificScheme, "foo", 0).Serialize());
  ASSERT_TRUE(LoadWithParams(params));
}

// Tests loading Data in place of a normal URL.
TEST_P(WebStateObserverTest, LoadData) {
  // Perform first navigation.
  const GURL first_url = test_server_->GetURL("/echoall");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  WebStatePolicyDecider::RequestInfo expected_request_info(
      ui::PageTransition::PAGE_TRANSITION_TYPED,
      /*target_main_frame=*/true, /*has_user_gesture=*/false);
  EXPECT_CALL(*decider_,
              ShouldAllowRequest(_, RequestInfoMatch(expected_request_info)))
      .WillOnce(Return(true));
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), first_url, ui::PageTransition::PAGE_TRANSITION_TYPED,
          &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), first_url, kExpectedMimeType, /*content_is_html=*/true,
          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle(first_url.GetContent()));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("EmbeddedTestServer - EchoAll"));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(first_url));

  NSString* html = @"<html><body>foo</body></html>";
  GURL data_url("https://www.chromium.test");

  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  }
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  // ShouldAllowResponse is not called on loadData navigation.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyDataStartedContext(
          web_state(), data_url, ui::PageTransition::PAGE_TRANSITION_TYPED,
          &context, &nav_id));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyDataFinishedContext(web_state(), data_url, "text/html",
                                          &context, &nav_id));
  EXPECT_CALL(observer_, TitleWasSet(web_state()))
      .WillOnce(VerifyTitle("https://www.chromium.test"));
  if (web::features::UseWKWebViewLoading()) {
    EXPECT_CALL(observer_, DidStopLoading(web_state()));
  }
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  web_state()->LoadData([html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", data_url);
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "foo"));
}

INSTANTIATE_TEST_SUITE_P(
    ProgrammaticWebStateObserverTest,
    WebStateObserverTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web
