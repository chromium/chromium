// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_fullscreen_disabler.h"

#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The modality used in tests.
const OverlayModality kModality = OverlayModality::kWebContentArea;
// Request config used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(kConfig);
}  // namespace

// Test fixture for OverlayPresentationContextFullscreenDisabler.
class OverlayPresentationContextFullscreenDisablerTest : public PlatformTest {
 public:
  OverlayPresentationContextFullscreenDisablerTest()
      : disabler_(&browser_, kModality) {
    // Set up the fake presentation context so OverlayPresenterObserver
    // callbacks are sent.
    overlay_presenter()->SetPresentationContext(&presentation_context_);
    // Insert and activate a WebState.
    browser_.GetWebStateList()->InsertWebState(
        0, std::make_unique<web::TestWebState>(), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
  }
  ~OverlayPresentationContextFullscreenDisablerTest() override {
    overlay_presenter()->SetPresentationContext(nullptr);
  }

  bool fullscreen_enabled() {
    if (fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
      return FullscreenController::FromBrowser(&browser_)->IsEnabled();
    } else {
      return FullscreenController::FromBrowserState(browser_.GetBrowserState())
          ->IsEnabled();
    }
  }
  OverlayPresenter* overlay_presenter() {
    return OverlayPresenter::FromBrowser(&browser_, kModality);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(
        browser_.GetWebStateList()->GetActiveWebState(), kModality);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestBrowser browser_;
  OverlayContainerFullscreenDisabler disabler_;
  FakeOverlayPresentationContext presentation_context_;
};

// Tests that OverlayPresentationContextFullscreenDisabler disables fullscreen
// when overlays are displayed.
TEST_F(OverlayPresentationContextFullscreenDisablerTest,
       DisableForPresentedOverlays) {
  ASSERT_TRUE(fullscreen_enabled());

  // Add an OverlayRequest to the active WebState's queue and verify that
  // fullscreen is disabled.
  queue()->AddRequest(OverlayRequest::CreateWithConfig<kConfig>());
  EXPECT_FALSE(fullscreen_enabled());

  // Cancel the request and verify that fullscreen is re-enabled.
  queue()->CancelAllRequests();
  EXPECT_TRUE(fullscreen_enabled());
}
