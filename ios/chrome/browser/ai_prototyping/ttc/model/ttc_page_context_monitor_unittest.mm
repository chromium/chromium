// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Fake `TtcPageContextMonitor::Delegate` recording fetched `PageContext`
// responses.
class FakeTtcPageContextMonitorDelegate
    : public TtcPageContextMonitor::Delegate {
 public:
  FakeTtcPageContextMonitorDelegate() = default;
  ~FakeTtcPageContextMonitorDelegate() override = default;

  const std::vector<optimization_guide::proto::PageContext>& fetched_contexts()
      const {
    return fetched_contexts_;
  }

  // `TtcPageContextMonitor::Delegate` implementation:
  void OnPageContextFetched(
      PageContextWrapperCallbackResponse response) override {
    if (response.has_value() && response.value()) {
      fetched_contexts_.push_back(*response.value());
    }
  }

 private:
  std::vector<optimization_guide::proto::PageContext> fetched_contexts_;
};

// Builds a synthetic `PageContext` proto for `url` and `title`.
PageContextWrapperCallbackResponse MakePageContextResponse(
    const GURL& url,
    const std::string& title) {
  auto context = std::make_unique<optimization_guide::proto::PageContext>();
  context->set_url(url.spec());
  context->set_title(title);
  auto* apc = context->mutable_annotated_page_content();
  apc->mutable_main_frame_data()->set_url(url.spec());
  apc->mutable_main_frame_data()->set_title(title);
  return base::ok(std::move(context));
}

}  // namespace

// Test fixture for `TtcPageContextMonitor`.
class TtcPageContextMonitorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    ConfigureEligibleWebState(web_state_, GURL("https://example.com/initial"));
  }

  // Configures `web_state` as a realized, finished-loading HTML page at `url`.
  void ConfigureEligibleWebState(web::FakeWebState& web_state,
                                 const GURL& url) {
    web_state.SetBrowserState(profile_.get());
    web_state.SetIsRealized(true);
    web_state.SetCurrentURL(url);
    web_state.SetContentsMimeType("text/html");
    web_state.SetLoading(false);
    for (web::ContentWorld world : {web::ContentWorld::kIsolatedWorld,
                                    web::ContentWorld::kPageContentWorld}) {
      web_state.SetWebFramesManager(
          world, std::make_unique<web::FakeWebFramesManager>());
    }
  }

  // Returns `true` if `monitor` currently holds an in-flight
  // `PageContextWrapper`.
  bool HasPendingWrapper(const TtcPageContextMonitor& monitor) const {
    return monitor.page_context_wrapper_ != nil;
  }

  // Simulates completion of `monitor`'s pending extraction for `expected_url`
  // with `response`.
  void SimulateExtractionCompleted(
      TtcPageContextMonitor& monitor,
      const GURL& expected_url,
      PageContextWrapperCallbackResponse response) {
    monitor.OnPageContextExtracted(expected_url, std::move(response));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  FakeTtcPageContextMonitorDelegate delegate_;
};

// Tests that constructing `TtcPageContextMonitor` on a realized, non-loading,
// eligible `WebState` starts an initial extraction immediately and forwards the
// result to the delegate.
TEST_F(TtcPageContextMonitorTest, ExtractsOnConstructionWhenLoaded) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_EQ(monitor.web_state(), &web_state_);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  const GURL url("https://example.com/initial");
  SimulateExtractionCompleted(monitor, url,
                              MakePageContextResponse(url, "Initial Page"));
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_contexts().size(), 1u);
  EXPECT_EQ(delegate_.fetched_contexts()[0].url(), url.spec());
  EXPECT_EQ(delegate_.fetched_contexts()[0].title(), "Initial Page");
}

// Tests that constructing `TtcPageContextMonitor` while the `WebState` is
// loading waits until `PageLoaded(SUCCESS)` before starting extraction.
TEST_F(TtcPageContextMonitorTest, WaitsForPageLoadedWhenInitiallyLoading) {
  web_state_.SetLoading(true);
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  // A failed page load must not trigger extraction.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  web_state_.SetLoading(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  // A subsequent failed page load cancels any in-flight extraction.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that ineligible pages (such as `chrome://newtab`) do not trigger
// extraction.
TEST_F(TtcPageContextMonitorTest, SkipsIneligiblePages) {
  web_state_.SetCurrentURL(GURL("chrome://newtab"));
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that `Disconnect()` stops observing the `WebState` and suppresses
// further delegate callbacks.
TEST_F(TtcPageContextMonitorTest, DisconnectStopsObservation) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  monitor.Disconnect();
  EXPECT_EQ(monitor.web_state(), nullptr);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that constructing `TtcPageContextMonitor` on an unrealized `WebState`
// does not start extraction.
TEST_F(TtcPageContextMonitorTest, SkipsUnrealizedWebStateOnConstruction) {
  web_state_.SetIsRealized(false);
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that destroying the monitored `WebState` cancels any pending extraction
// and clears `web_state()`.
TEST_F(TtcPageContextMonitorTest, WebStateDestroyedCancelsPendingExtraction) {
  auto transient_web_state = std::make_unique<web::FakeWebState>();
  ConfigureEligibleWebState(*transient_web_state,
                            GURL("https://example.com/transient"));

  TtcPageContextMonitor monitor(*transient_web_state, &delegate_);
  EXPECT_EQ(monitor.web_state(), transient_web_state.get());
  EXPECT_TRUE(HasPendingWrapper(monitor));

  transient_web_state.reset();
  EXPECT_EQ(monitor.web_state(), nullptr);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}
