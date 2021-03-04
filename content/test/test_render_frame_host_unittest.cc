// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include <string>
#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestRenderFrameHostTest : public RenderViewHostImplTestHarness,
                                public WebContentsObserver {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    Observe(RenderViewHostImplTestHarness::web_contents());
  }

  std::vector<std::string>& Events() { return events_; }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    events_.push_back("DidStartNavigation");
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    events_.push_back("DidFinishNavigation");
  }

  void DidStartLoading() override { events_.push_back("DidStartLoading"); }

  void DidStopLoading() override { events_.push_back("DidStopLoading"); }

  void DocumentAvailableInMainFrame() override {
    events_.push_back("DocumentAvailableInMainFrame");
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    events_.push_back("DocumentOnLoadCompletedInMainFrame");
  }

  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    events_.push_back("DOMContentLoaded");
  }

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    events_.push_back("DidFinishLoad");
  }

  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& url,
                   int error_code) override {
    events_.push_back("DidFailLoad");
  }

 private:
  std::vector<std::string> events_;
};

// These tests check that the loading events simulated by NavigationSimulator
// together with TestRenderFrameHost::SimulateLoadingCompleted match
// the real behaviour, captured by the browser tests.
//
// Keep in sync with WebContentsImplBrowserTest.LoadingCallbacksOrder_*.
TEST_F(TestRenderFrameHostTest, LoadingCallbacksOrder_CrossDocument) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->Start();
  simulator->Commit();

  EXPECT_THAT(Events(),
              testing::ElementsAre(
                  "DidStartLoading", "DidStartNavigation",
                  "DidFinishNavigation", "DocumentAvailableInMainFrame",
                  "DOMContentLoaded", "DocumentOnLoadCompletedInMainFrame",
                  "DidFinishLoad", "DidStopLoading"));
}

TEST_F(TestRenderFrameHostTest, LoadingCallbacksOrder_SameDocument) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->Start();
  simulator->Commit();

  Events().clear();

  NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.test/#foo"), main_rfh())
      ->CommitSameDocument();

  EXPECT_THAT(Events(),
              testing::ElementsAre("DidStartLoading", "DidStartNavigation",
                                   "DidFinishNavigation", "DidStopLoading"));
}

}  // namespace content
