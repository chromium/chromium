// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DocumentPictureInPictureNavigationThrottleTest
    : public RenderViewHostTestHarness {
 public:
  DocumentPictureInPictureNavigationThrottleTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DocumentPictureInPictureNavigationThrottleTest(
      const DocumentPictureInPictureNavigationThrottleTest&) = delete;
  DocumentPictureInPictureNavigationThrottleTest& operator=(
      const DocumentPictureInPictureNavigationThrottleTest&) = delete;
  ~DocumentPictureInPictureNavigationThrottleTest() override = default;
};

TEST_F(DocumentPictureInPictureNavigationThrottleTest,
       ClosesPictureInPictureWindowOnNavigationStart) {
  // Simulate that we're inside a document picture-in-picture window.
  blink::mojom::PictureInPictureWindowOptions options;
  static_cast<TestWebContents*>(web_contents())
      ->SetPictureInPictureOptions(options);

  // Simulate a navigation, which should fail.
  auto nav_simulator = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://example.test/"), main_rfh());
  nav_simulator->Start();
  EXPECT_TRUE(nav_simulator->HasFailed());

  // The RenderFrameHost should be asynchronously asked to close. First,
  // RunUntilIdle() to request the close, and then wait out the unload timer via
  // FastForwardBy().
  task_environment()->RunUntilIdle();
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(
      static_cast<RenderFrameHostImpl*>(main_rfh())->IsPageReadyToBeClosed());
}

TEST_F(DocumentPictureInPictureNavigationThrottleTest,
       SameDocumentNavigationsOkay) {
  auto* committed_rfh = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://example.test/"), main_rfh());
  EXPECT_EQ(committed_rfh, main_rfh());
  EXPECT_EQ(GURL("https://example.test/"), main_rfh()->GetLastCommittedURL());

  // Simulate that we're inside a document picture-in-picture window.
  blink::mojom::PictureInPictureWindowOptions options;
  static_cast<TestWebContents*>(web_contents())
      ->SetPictureInPictureOptions(options);

  // Simulate a same-document navigation, which should succeed.
  {
    auto nav_simulator = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL("https://example.test/#"), main_rfh());
    nav_simulator->CommitSameDocument();
    EXPECT_FALSE(nav_simulator->HasFailed());
    EXPECT_EQ(GURL("https://example.test/#"),
              main_rfh()->GetLastCommittedURL());
  }
}

}  // namespace content
