// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

enum PrerenderTestType {
  kWebContents,
  kMPArch,
};

std::string ToString(const testing::TestParamInfo<PrerenderTestType>& info) {
  switch (info.param) {
    case PrerenderTestType::kWebContents:
      return "WebContents";
    case PrerenderTestType::kMPArch:
      return "MPArch";
  }
}

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;
};

class PrerenderHostTest
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<PrerenderTestType> {
 public:
  PrerenderHostTest() {
    std::map<std::string, std::string> parameters;
    switch (GetParam()) {
      case kWebContents:
        parameters["implementation"] = "webcontents";
        break;
      case kMPArch:
        parameters["implementation"] = "mparch";
        break;
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrerender2, parameters);
  }

  ~PrerenderHostTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  void ExpectFinalStatus(PrerenderHost::FinalStatus status) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus", status, 1);
  }

  bool IsMPArchActive() const {
    switch (GetParam()) {
      case kWebContents:
        return false;
      case kMPArch:
        return true;
    }
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents_delegate_ = std::make_unique<TestWebContentsDelegate>();
    web_contents->SetDelegate(web_contents_delegate_.get());
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

  PrerenderHostRegistry* GetPrerenderHostRegistry() const {
    return static_cast<StoragePartitionImpl*>(
               BrowserContext::GetDefaultStoragePartition(
                   browser_context_.get()))
        ->GetPrerenderHostRegistry();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContentsDelegate> web_contents_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_P(PrerenderHostTest, Activate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  // Start prerendering a page.
  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  PrerenderHost* prerender_host =
      registry->FindHostById(prerender_frame_tree_node_id);

  // Finish the prerendering navigation. Normally we could use
  // EmbeddedTestServer to provide a response, but this test uses
  // RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order
  // to complete. Use NavigationSimulator to finish the navigation.
  FrameTreeNode* ftn =
      FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  sim->ReadyToCommit();
  sim->Commit();
  EXPECT_TRUE(prerender_host->is_ready_for_activation());

  // Perform a navigation in the primary frame tree which activates the
  // prerendered page.
  auto sim_2 = NavigationSimulatorImpl::CreateBrowserInitiated(
      kPrerenderingUrl, web_contents.get());
  sim_2->Start();
  if (IsMPArchActive()) {
    sim_2->Commit();
  } else {
    // The multiple WebContents impl requires ActivatePrerenderedContents() to
    // be called directly instead of just simulating navigation commit.
    prerender_host->ActivatePrerenderedContents(*initiator_rfh,
                                                *sim_2->GetNavigationHandle());
  }
  ExpectFinalStatus(PrerenderHost::FinalStatus::kActivated);
}

TEST_P(PrerenderHostTest, DontActivate) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* initiator_rfh = web_contents->GetMainFrame();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  // Start the prerendering navigation, but don't activate it.
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *initiator_rfh);
  registry->AbandonHost(prerender_frame_tree_node_id);
  ExpectFinalStatus(PrerenderHost::FinalStatus::kDestroyed);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderHostTest,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

}  // namespace
}  // namespace content
