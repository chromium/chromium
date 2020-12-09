// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

class PrerenderHostRegistryTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostRegistryTest() = default;
  ~PrerenderHostRegistryTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
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
};

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  registry->CreateAndStartHost(std::move(attributes),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Artificially finish navigation to make the prerender host ready to activate
  // the prerendered page.
  prerender_host->DidFinishNavigation(nullptr);

  EXPECT_TRUE(registry->SelectForNavigation(
      kPrerenderingUrl, *render_frame_host->frame_tree_node()));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHostForSameURL) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");

  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl;

  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  registry->CreateAndStartHost(std::move(attributes1),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  registry->CreateAndStartHost(std::move(attributes2),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);

  // Artificially finish navigation to make the prerender host ready to activate
  // the prerendered page.
  prerender_host1->DidFinishNavigation(nullptr);

  EXPECT_TRUE(registry->SelectForNavigation(
      kPrerenderingUrl, *render_frame_host->frame_tree_node()));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHostForDifferentURLs) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl1("https://example.com/next1");
  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl1;

  const GURL kPrerenderingUrl2("https://example.com/next2");
  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl2;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  registry->CreateAndStartHost(std::move(attributes1),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  registry->CreateAndStartHost(std::move(attributes2),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl1);
  PrerenderHost* prerender_host2 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl2);

  // Artificially finish navigation to make the prerender hosts ready to
  // activate the prerendered pages.
  prerender_host1->DidFinishNavigation(nullptr);
  prerender_host2->DidFinishNavigation(nullptr);

  // Select the first host.
  EXPECT_TRUE(registry->SelectForNavigation(
      kPrerenderingUrl1, *render_frame_host->frame_tree_node()));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  // The second host should still be findable.
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2),
            prerender_host2);

  // Select the second host.
  EXPECT_TRUE(registry->SelectForNavigation(
      kPrerenderingUrl2, *render_frame_host->frame_tree_node()));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
}

TEST_F(PrerenderHostRegistryTest, SelectForNavigationBeforeReadyForActivation) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  registry->CreateAndStartHost(std::move(attributes),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // The prerender host is not ready for activation yet, so the registry
  // shouldn't select the host and instead should abandon it.
  ASSERT_FALSE(prerender_host->is_ready_for_activation());
  EXPECT_FALSE(registry->SelectForNavigation(
      kPrerenderingUrl, *render_frame_host->frame_tree_node()));
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

TEST_F(PrerenderHostRegistryTest, AbandonHost) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  registry->CreateAndStartHost(std::move(attributes),
                               render_frame_host->GetGlobalFrameRoutingId(),
                               render_frame_host->GetLastCommittedOrigin());
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  registry->AbandonHost(kPrerenderingUrl);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

}  // namespace
}  // namespace content
