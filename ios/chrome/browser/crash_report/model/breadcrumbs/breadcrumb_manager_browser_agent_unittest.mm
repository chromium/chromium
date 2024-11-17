// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"

#import "base/containers/circular_deque.h"
#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/confirm_download_replacing_overlay.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_confirm_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

namespace {

// Creates test state, inserts it into WebState list and activates.
void InsertWebState(Browser* browser) {
  auto web_state = std::make_unique<web::FakeWebState>();
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  BreadcrumbManagerTabHelper::CreateForWebState(web_state.get());
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
}

const base::circular_deque<std::string>& GetEvents() {
  return breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
}

}  // namespace

// Test fixture for testing BreadcrumbManagerBrowserAgent class.
class BreadcrumbManagerBrowserAgentTest : public PlatformTest {
 protected:
  BreadcrumbManagerBrowserAgentTest() {
    TestProfileIOS::Builder test_profile_builder;
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);
  }

  ~BreadcrumbManagerBrowserAgentTest() override { browser_.reset(); }

  web::WebTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  FakeOverlayPresentationContext presentation_context_;
};

// Tests that an event logged by the BrowserAgent is returned with events for
// the associated `profile_`.
TEST_F(BreadcrumbManagerBrowserAgentTest, LogEvent) {
  ASSERT_EQ(0u, GetEvents().size());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  InsertWebState(browser_.get());

  EXPECT_EQ(1u, GetEvents().size());
}

// Tests that events logged through BrowserAgents associated with different
// Browser instances are returned with events for the associated
// `profile_` and are uniquely identifiable.
TEST_F(BreadcrumbManagerBrowserAgentTest, MultipleBrowsers) {
  ASSERT_EQ(0u, GetEvents().size());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  // Insert WebState into `browser`.
  InsertWebState(browser_.get());

  // Create and setup second Browser.
  std::unique_ptr<Browser> browser2 =
      std::make_unique<TestBrowser>(profile_.get());
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser2.get());

  // Insert WebState into `browser2`.
  InsertWebState(browser2.get());

  const auto& events = GetEvents();
  EXPECT_EQ(2u, events.size());

  // Seperately compare the start and end of the event strings to ensure
  // uniqueness at both the Browser and WebState layer.
  std::size_t browser1_split_pos = events.front().find("Insert");
  std::size_t browser2_split_pos = events.back().find("Insert");
  // The start of the string must be unique to differentiate the associated
  // Browser object by including the BreadcrumbManagerBrowserAgent's
  // `unique_id_`.
  // (The Timestamp will match due to TimeSource::MOCK_TIME in the `task_env_`.)
  std::string browser1_start = events.front().substr(browser1_split_pos);
  std::string browser2_start = events.back().substr(browser2_split_pos);
  EXPECT_STRNE(browser1_start.c_str(), browser2_start.c_str());
  // The end of the string must be unique because the WebStates are different
  // and that needs to be represented in the event string.
  std::string browser1_end = events.front().substr(
      browser1_split_pos, events.front().length() - browser1_split_pos);
  std::string browser2_end = events.back().substr(
      browser2_split_pos, events.back().length() - browser2_split_pos);
  EXPECT_STRNE(browser1_end.c_str(), browser2_end.c_str());
}

// Tests WebStateList's batch insertion and closing.
TEST_F(BreadcrumbManagerBrowserAgentTest, BatchOperations) {
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  // Insert multiple WebStates in a batch operation.
  {
    WebStateList::ScopedBatchOperation lock =
        browser_->GetWebStateList()->StartBatchOperation();
    InsertWebState(browser_.get());
    InsertWebState(browser_.get());
  }

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(base::Contains(events.front(), "Inserted 2 tabs"))
      << events.front();

  // Close multiple WebStates in a batch operation.
  {
    WebStateList::ScopedBatchOperation lock =
        browser_->GetWebStateList()->StartBatchOperation();
    browser_->GetWebStateList()->CloseWebStateAt(
        0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    browser_->GetWebStateList()->CloseWebStateAt(
        0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }

  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(events.back(), "Closed 2 tabs")) << events.back();
}

// Tests logging kBreadcrumbOverlayJsAlert.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptAlertOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertDialogRequest>(
          browser_->GetWebStateList()->GetWebStateAt(0), GURL(),
          /*is_main_frame=*/true, @"message"));
  queue->CancelAllRequests();

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayJsAlert))
      << events.back();
}

// Tests logging kBreadcrumbOverlayJsConfirm.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptConfirmOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptConfirmDialogRequest>(
          browser_->GetWebStateList()->GetWebStateAt(0), GURL(),
          /*is_main_frame=*/true, @"message"));
  queue->CancelAllRequests();

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayJsConfirm))
      << events.back();
}

// Tests logging kBreadcrumbOverlayJsPrompt.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptPromptOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptPromptDialogRequest>(
          browser_->GetWebStateList()->GetWebStateAt(0), GURL(),
          /*is_main_frame=*/true, @"message",
          /*default_text_field_value=*/nil));
  queue->CancelAllRequests();

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayJsPrompt))
      << events.back();
}

// Tests logging kBreadcrumbOverlayHttpAuth.
TEST_F(BreadcrumbManagerBrowserAgentTest, HttpAuthOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          GURL(), "message", "default text"));
  queue->CancelAllRequests();

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayHttpAuth))
      << events.back();
}

// Tests logging kBreadcrumbOverlayAppLaunch.
TEST_F(BreadcrumbManagerBrowserAgentTest, AppLaunchOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    app_launcher_overlays::AppLaunchConfirmationRequest>(
      app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther));
  queue->CancelAllRequests();

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayAppLaunch))
      << events.back();
}

// Tests logging kBreadcrumbOverlayAlert with initial and repeated presentation.
TEST_F(BreadcrumbManagerBrowserAgentTest, AlertOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0),
      OverlayModality::kWebContentArea);
  // ConfirmDownloadReplacingRequest logged as generic alert.
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<ConfirmDownloadReplacingRequest>());

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlay))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), kBreadcrumbOverlayAlert))
      << events.back();
  EXPECT_FALSE(base::Contains(events.back(), kBreadcrumbOverlayActivated))
      << events.back();

  // Switching tabs should log new overlay presentations.
  InsertWebState(browser_.get());
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(events.back(), "Insert active Tab"))
      << events.back();

  browser_->GetWebStateList()->ActivateWebStateAt(0);
  ASSERT_EQ(4u, events.size());
  auto activation = std::next(events.begin(), 2);
  EXPECT_TRUE(base::Contains(*activation, kBreadcrumbOverlay)) << *activation;
  EXPECT_TRUE(base::Contains(*activation, kBreadcrumbOverlayAlert))
      << *activation;
  EXPECT_TRUE(base::Contains(*activation, kBreadcrumbOverlayActivated))
      << *activation;
  queue->CancelAllRequests();
}
