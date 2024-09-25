// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_fullscreen_disabler.h"

#import <WebKit/WebKit.h>

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
// The modality used in tests.
const OverlayModality kModality = OverlayModality::kWebContentArea;
// Request config used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(kConfig);
}  // namespace

// Test fixture for OverlayPresentationContextFullscreenDisabler.
class OverlayPresentationContextFullscreenDisablerTest : public PlatformTest {
 public:
  OverlayPresentationContextFullscreenDisablerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    disabler_ = std::make_unique<OverlayContainerFullscreenDisabler>(
        browser_.get(), kModality);
    web_view_ =
        [[WKWebView alloc] initWithFrame:scoped_window_.Get().bounds
                           configuration:[[WKWebViewConfiguration alloc] init]];
    content_view_ = [[CRWWebViewContentView alloc]
        initWithWebView:web_view_
             scrollView:web_view_.scrollView
        fullscreenState:CrFullscreenState::kNotInFullScreen];
    // Set up the fake presentation context so OverlayPresenterObserver
    // callbacks are sent.
    overlay_presenter()->SetPresentationContext(&presentation_context_);

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetView(content_view_);
    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    UIScrollView* scroll_view = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    web_state->SetWebViewProxy(web_view_proxy_mock);
    // Insert and activate a WebState.
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }
  ~OverlayPresentationContextFullscreenDisablerTest() override {
    overlay_presenter()->SetPresentationContext(nullptr);
  }

  bool fullscreen_enabled() {
    return FullscreenController::FromBrowser(browser_.get())->IsEnabled();
  }
  OverlayPresenter* overlay_presenter() {
    return OverlayPresenter::FromBrowser(browser_.get(), kModality);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(
        browser_->GetWebStateList()->GetActiveWebState(), kModality);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<OverlayContainerFullscreenDisabler> disabler_;
  FakeOverlayPresentationContext presentation_context_;
  ScopedKeyWindow scoped_window_;
  WKWebView* web_view_ = nil;
  CRWWebViewContentView* content_view_ = nil;
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
