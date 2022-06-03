// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/progress_tracker.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ProgressTrackerTest : public testing::Test, public FakeLocalFrameHost {
 public:
  ProgressTrackerTest()
      : response_(KURL("http://example.com")), last_progress_(0.0) {
    response_.SetMimeType("text/html");
    response_.SetExpectedContentLength(1024);
  }

  void SetUp() override {
    FakeLocalFrameHost::Init(
        web_frame_client_.GetRemoteNavigationAssociatedInterfaces());
    web_view_helper_.Initialize(&web_frame_client_);
  }

  void TearDown() override {
    // The WebViewHelper will crash when being reset if the TestWebFrameClient
    // is still reporting that some loads are in progress, so let's make sure
    // that's not the case via a call to ProgressTracker::ProgressCompleted().
    if (web_frame_client_.IsLoading())
      Progress().ProgressCompleted();
    web_view_helper_.Reset();
  }

  LocalFrame* GetFrame() const {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }

  ProgressTracker& Progress() const { return GetFrame()->Loader().Progress(); }

  double LastProgress() const { return last_progress_; }

  const ResourceResponse& ResponseHeaders() const { return response_; }

  // Reports a 1024-byte "main resource" (VeryHigh priority) request/response
  // to ProgressTracker with identifier 1, but tests are responsible for
  // emulating payload and load completion.
  void EmulateMainResourceRequestAndResponse() const {
    Progress().ProgressStarted();
    Progress().WillStartLoading(1ul, ResourceLoadPriority::kVeryHigh);
    EXPECT_EQ(0.0, LastProgress());
    Progress().IncrementProgress(1ul, ResponseHeaders());
    EXPECT_EQ(0.0, LastProgress());
  }

  double WaitForNextProgressChange() const {
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> current_loop(&current_run_loop_, &run_loop);
    run_loop.Run();
    return last_progress_;
  }

  // FakeLocalFrameHost:
  void DidChangeLoadProgress(double progress) override {
    last_progress_ = progress;
    current_run_loop_->Quit();
  }

 private:
  mutable base::RunLoop* current_run_loop_ = nullptr;
  frame_test_helpers::TestWebFrameClient web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  ResourceResponse response_;
  double last_progress_;
};

TEST_F(ProgressTrackerTest, Static) {
  Progress().ProgressStarted();
  EXPECT_EQ(0.0, LastProgress());
  Progress().ProgressCompleted();
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

TEST_F(ProgressTrackerTest, MainResourceOnly) {
  EmulateMainResourceRequestAndResponse();

  // .2 for committing, .25 out of .5 possible for bytes received.
  Progress().IncrementProgress(1ul, 512);
  EXPECT_EQ(0.45, WaitForNextProgressChange());

  // .2 for committing, .5 for all bytes received.
  Progress().CompleteProgress(1ul);
  EXPECT_EQ(0.7, WaitForNextProgressChange());

  Progress().FinishedParsing();
  EXPECT_EQ(0.8, WaitForNextProgressChange());

  Progress().DidFirstContentfulPaint();
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

TEST_F(ProgressTrackerTest, WithHighPriorirySubresource) {
  EmulateMainResourceRequestAndResponse();

  Progress().WillStartLoading(2ul, ResourceLoadPriority::kHigh);
  Progress().IncrementProgress(2ul, ResponseHeaders());
  EXPECT_EQ(0.0, LastProgress());

  // .2 for committing, .25 out of .5 possible for bytes received.
  Progress().IncrementProgress(1ul, 1024);
  Progress().CompleteProgress(1ul);
  EXPECT_EQ(0.45, WaitForNextProgressChange());

  // .4 for finishing parsing/painting,
  // .25 out of .5 possible for bytes received.
  Progress().FinishedParsing();
  EXPECT_EQ(0.55, WaitForNextProgressChange());

  Progress().DidFirstContentfulPaint();
  EXPECT_EQ(0.65, WaitForNextProgressChange());

  Progress().CompleteProgress(2ul);
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

TEST_F(ProgressTrackerTest, WithMediumPrioritySubresource) {
  EmulateMainResourceRequestAndResponse();

  Progress().WillStartLoading(2ul, ResourceLoadPriority::kMedium);
  Progress().IncrementProgress(2ul, ResponseHeaders());
  EXPECT_EQ(0.0, LastProgress());

  // .2 for committing, .5 for all bytes received.
  // Medium priority resource is ignored.
  Progress().CompleteProgress(1ul);
  EXPECT_EQ(0.7, WaitForNextProgressChange());

  Progress().FinishedParsing();
  EXPECT_EQ(0.8, WaitForNextProgressChange());

  Progress().DidFirstContentfulPaint();
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

TEST_F(ProgressTrackerTest, FinishParsingBeforeContentfulPaint) {
  EmulateMainResourceRequestAndResponse();

  // .2 for committing, .5 for all bytes received.
  Progress().CompleteProgress(1ul);
  EXPECT_EQ(0.7, WaitForNextProgressChange());

  Progress().FinishedParsing();
  EXPECT_EQ(0.8, WaitForNextProgressChange());

  Progress().DidFirstContentfulPaint();
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

TEST_F(ProgressTrackerTest, ContentfulPaintBeforeFinishParsing) {
  EmulateMainResourceRequestAndResponse();

  // .2 for committing, .5 for all bytes received.
  Progress().CompleteProgress(1ul);
  EXPECT_EQ(0.7, WaitForNextProgressChange());

  Progress().DidFirstContentfulPaint();
  EXPECT_EQ(0.8, WaitForNextProgressChange());

  Progress().FinishedParsing();
  EXPECT_EQ(1.0, WaitForNextProgressChange());
}

}  // namespace blink
