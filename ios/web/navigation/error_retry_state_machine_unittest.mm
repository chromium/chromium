// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/error_retry_state_machine.h"

#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

static const char kTestUrl[] = "http://test.com";

typedef PlatformTest ErrorRetryStateMachineTest;

// Tests load failure during provisional navigation.
TEST_F(ErrorRetryStateMachineTest, OfflineThenReload) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);
  ASSERT_EQ(ErrorRetryState::kNewRequest, machine.state());

  const GURL placeholder_url =
      wk_navigation_util::CreatePlaceholderUrlForUrl(test_url);

  // Initial load fails.
  ASSERT_EQ(ErrorRetryCommand::kLoadPlaceholder,
            machine.DidFailProvisionalNavigation(GURL::EmptyGURL(), test_url));
  ASSERT_EQ(ErrorRetryState::kLoadingPlaceholder, machine.state());

  // Placeholder load finishes.
  ASSERT_EQ(ErrorRetryCommand::kLoadError,
            machine.DidFinishNavigation(placeholder_url));
  ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, machine.state());

  // Presents error.
  machine.SetDisplayingWebError();
  ASSERT_EQ(ErrorRetryState::kDisplayingError, machine.state());

  // Reload the failed navigation.
  ASSERT_EQ(ErrorRetryCommand::kRewriteToWebViewURL,
            machine.DidFinishNavigation(placeholder_url));
  ASSERT_EQ(ErrorRetryState::kNavigatingToFailedNavigationItem,
            machine.state());

  // Simulate Web View URL rewrite success.
  ASSERT_EQ(ErrorRetryCommand::kReload, machine.DidFinishNavigation(test_url));
  ASSERT_EQ(ErrorRetryState::kRetryFailedNavigationItem, machine.state());

  // Reload succeeds.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kDoNothing,
              clone.DidFinishNavigation(test_url));
    ASSERT_EQ(ErrorRetryState::kNoNavigationError, clone.state());
  }

  // Reload fails again in provisional navigation (e.g. still in airplane mode).
  {
    ErrorRetryStateMachine clone(machine);
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      ASSERT_EQ(ErrorRetryCommand::kRewriteToPlaceholderURL,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kLoadingPlaceholder, clone.state());
    } else {
      ASSERT_EQ(ErrorRetryCommand::kLoadError,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
    }
  }

  // Reload fails after navigation is committed.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kLoadError, clone.DidFailNavigation(test_url));
    ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
  }
}

// Tests state transitions for displaying error in web view.
TEST_F(ErrorRetryStateMachineTest, WebErrorPageThenReload) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);
  ASSERT_EQ(ErrorRetryState::kNewRequest, machine.state());

  const GURL placeholder_url =
      wk_navigation_util::CreatePlaceholderUrlForUrl(test_url);

  // Initial load fails.
  ASSERT_EQ(ErrorRetryCommand::kLoadPlaceholder,
            machine.DidFailProvisionalNavigation(GURL::EmptyGURL(), test_url));
  ASSERT_EQ(ErrorRetryState::kLoadingPlaceholder, machine.state());

  // Placeholder load finishes.
  ASSERT_EQ(ErrorRetryCommand::kLoadError,
            machine.DidFinishNavigation(placeholder_url));
  ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, machine.state());

  // Presents error in web view.
  ASSERT_EQ(ErrorRetryCommand::kDoNothing,
            machine.DidFinishNavigation(test_url));
  ASSERT_EQ(ErrorRetryState::kDisplayingError, machine.state());

  // Reload the failed navigation succeeds.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kDoNothing,
              clone.DidFinishNavigation(test_url));
    ASSERT_EQ(ErrorRetryState::kNoNavigationError, clone.state());
  }

  // Back-forward navigation to failed navigation rewrites Web View URL.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kRewriteToWebViewURL,
              clone.DidFinishNavigation(placeholder_url));
    ASSERT_EQ(ErrorRetryState::kNavigatingToFailedNavigationItem,
              clone.state());
  }

  // Reload fails again in provisional navigation.
  {
    ErrorRetryStateMachine clone(machine);
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      ASSERT_EQ(ErrorRetryCommand::kRewriteToPlaceholderURL,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kLoadingPlaceholder, clone.state());
    } else {
      ASSERT_EQ(ErrorRetryCommand::kLoadError,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
    }
  }

  // Reload fails after navigation is committed.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kLoadError, clone.DidFailNavigation(test_url));
    ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
  }

  // Simulate back/forward navigation to a restored session entry that never
  // succeeded in loading.
  {
    const GURL restore_session_url =
        wk_navigation_util::CreateRedirectUrl(test_url);
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kDoNothing,
              clone.DidFinishNavigation(restore_session_url));
    ASSERT_EQ(ErrorRetryState::kNoNavigationError, clone.state());
  }
}

// Tests load failure after navigation is committed.
TEST_F(ErrorRetryStateMachineTest, LoadFailedAfterCommit) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);

  ASSERT_EQ(ErrorRetryCommand::kLoadError,
            machine.DidFailNavigation(GURL::EmptyGURL()));
  ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, machine.state());
}

// Tests reloading a previously successful navigation.
TEST_F(ErrorRetryStateMachineTest, SuccessThenReloadOffline) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);

  const GURL placeholder_url =
      wk_navigation_util::CreatePlaceholderUrlForUrl(test_url);

  // Simulate a successful load.
  ASSERT_EQ(ErrorRetryCommand::kDoNothing,
            machine.DidFinishNavigation(test_url));
  ASSERT_EQ(ErrorRetryState::kNoNavigationError, machine.state());

  // Reload succeeds.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kDoNothing,
              clone.DidFinishNavigation(test_url));
  }

  // Reloads fails provisional navigation.
  {
    ErrorRetryStateMachine clone(machine);
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      ASSERT_EQ(ErrorRetryCommand::kRewriteToPlaceholderURL,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kLoadingPlaceholder, clone.state());
    } else {
      ASSERT_EQ(ErrorRetryCommand::kLoadError,
                clone.DidFailProvisionalNavigation(test_url, test_url));
      ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
    }
  }

  // Reload fails after commit.
  {
    ErrorRetryStateMachine clone(machine);
    ASSERT_EQ(ErrorRetryCommand::kLoadError, clone.DidFailNavigation(test_url));
    ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, clone.state());
  }
}

// Tests that cold restart in offline mode does not insert placeholder.
TEST_F(ErrorRetryStateMachineTest, OfflineAfterColdStart) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);

  GURL redirect_url = wk_navigation_util::CreateRedirectUrl(test_url);

  // restore_session.html is expected to finish without error.
  ASSERT_EQ(ErrorRetryCommand::kDoNothing,
            machine.DidFinishNavigation(redirect_url));
  ASSERT_EQ(ErrorRetryState::kNewRequest, machine.state());

  // Simulate failure in restore_session.html client redirect.
  ASSERT_EQ(ErrorRetryCommand::kLoadError,
            machine.DidFailProvisionalNavigation(redirect_url, test_url));
  ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, machine.state());
}

// Tests retrying a placeholder navigation.
TEST_F(ErrorRetryStateMachineTest, RetryPlaceholderNavigation) {
  GURL test_url(kTestUrl);
  ErrorRetryStateMachine machine;
  machine.SetURL(test_url);
  ASSERT_EQ(ErrorRetryState::kNewRequest, machine.state());

  // First trigger the cached placeholder load.
  machine.SetNoNavigationError();
  const GURL placeholder_url =
      wk_navigation_util::CreatePlaceholderUrlForUrl(test_url);
  ASSERT_EQ(ErrorRetryCommand::kDoNothing,
            machine.DidFinishNavigation(placeholder_url));
  ASSERT_EQ(ErrorRetryState::kNoNavigationError, machine.state());

  // Then trigger a retry.
  machine.SetRetryPlaceholderNavigation();
  ASSERT_EQ(ErrorRetryCommand::kRewriteToWebViewURL,
            machine.DidFinishNavigation(placeholder_url));
  ASSERT_EQ(ErrorRetryState::kRetryPlaceholderNavigation, machine.state());

  // Lastly trigger the error view.
  const GURL target_url =
      wk_navigation_util::ExtractUrlFromPlaceholderUrl(placeholder_url);
  ASSERT_EQ(ErrorRetryCommand::kLoadError,
            machine.DidFinishNavigation(target_url));
  ASSERT_EQ(ErrorRetryState::kReadyToDisplayError, machine.state());
}

}  // namespace
}  // namespace web
