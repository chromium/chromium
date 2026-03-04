// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_handle_impl.h"

#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// TODO(crbug.com/485415143): Add test cases for other public methods in
// PrerenderHandleImpl.
class PrerenderHandleImplTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHandleImplTest() = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(*contents());
    contents()->NavigateAndCommit(GURL("https://example.com/"));
  }

  void TearDown() override {
    web_contents_delegate_.reset();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  PrerenderHostRegistry& registry() {
    return *contents()->GetPrerenderHostRegistry();
  }

  PrerenderAttributes GeneratePrerenderAttributes(const GURL& url) {
    return PrerenderAttributes(
        url, PreloadingTriggerType::kEmbedder, "EmbedderSuffix", std::nullopt,
        Referrer(), std::nullopt, nullptr, contents()->GetWeakPtr(),
        ui::PAGE_TRANSITION_LINK,
        /*should_warm_up_compositor=*/false,
        /*should_prepare_paint_tree=*/false,
        blink::mojom::SpeculationAction::kPrerender,
        /*url_match_predicate=*/{},
        /*prerender_navigation_handle_callback=*/{},
        PreloadPipelineInfoImpl::Create(
            /*planned_max_preloading_type=*/PreloadingType::kPrerender),
        /*allow_reuse=*/false,
        /*form_submission=*/false);
  }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
  test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(PrerenderHandleImplTest, OnResponseHeadersReceived) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const PrerenderHostId prerender_host_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  ASSERT_TRUE(prerender_host_id);

  auto handle = std::make_unique<PrerenderHandleImpl>(
      registry().GetWeakPtr(), prerender_host_id, kPrerenderingUrl,
      std::nullopt);

  EXPECT_TRUE(handle->IsWaitingForResponseHeaders());

  bool callback_triggered = false;
  handle->AddOnResponseHeadersReceivedCallback(
      base::BindLambdaForTesting([&]() { callback_triggered = true; }));

  PrerenderHost* host = registry().FindNonReservedHostById(prerender_host_id);
  ASSERT_TRUE(host);
  FrameTreeNode* ftn = FrameTreeNode::From(host->GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulatorImpl> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);

  sim->ReadyToCommit();

  EXPECT_FALSE(handle->IsWaitingForResponseHeaders());
  EXPECT_TRUE(callback_triggered);
}

TEST_F(PrerenderHandleImplTest, CallbackCalledOnFailure) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const PrerenderHostId prerender_host_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl));
  ASSERT_TRUE(prerender_host_id);

  auto handle = std::make_unique<PrerenderHandleImpl>(
      registry().GetWeakPtr(), prerender_host_id, kPrerenderingUrl,
      std::nullopt);

  EXPECT_TRUE(handle->IsWaitingForResponseHeaders());
  bool callback_triggered = false;
  handle->AddOnResponseHeadersReceivedCallback(
      base::BindLambdaForTesting([&]() { callback_triggered = true; }));

  bool error_called = false;
  handle->AddErrorCallback(
      base::BindLambdaForTesting([&]() { error_called = true; }));

  registry().CancelHost(prerender_host_id,
                        PrerenderFinalStatus::kRendererProcessCrashed);

  EXPECT_TRUE(callback_triggered);
  EXPECT_TRUE(error_called);
}

}  // namespace
}  // namespace content
