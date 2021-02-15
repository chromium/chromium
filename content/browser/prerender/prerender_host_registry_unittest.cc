// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host_registry.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

// This definition is needed because this constant is odr-used in gtest macros.
// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
const int kNoFrameTreeNodeId = RenderFrameHost::kNoFrameTreeNodeId;

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
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Artificially finish navigation to make the prerender host ready to activate
  // the prerendered page.
  prerender_host->DidFinishNavigation(nullptr);

  EXPECT_EQ(registry->ReserveHostToActivate(
                kPrerenderingUrl, *render_frame_host->frame_tree_node()),
            prerender_frame_tree_node_id);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  registry->AbandonReservedHost(prerender_frame_tree_node_id);
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
  const int frame_tree_node_id1 =
      registry->CreateAndStartHost(std::move(attributes1), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  const int frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  EXPECT_EQ(frame_tree_node_id1, frame_tree_node_id2);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);

  // Artificially finish navigation to make the prerender host ready to activate
  // the prerendered page.
  prerender_host1->DidFinishNavigation(nullptr);

  EXPECT_EQ(registry->ReserveHostToActivate(
                kPrerenderingUrl, *render_frame_host->frame_tree_node()),
            frame_tree_node_id1);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  registry->AbandonReservedHost(frame_tree_node_id1);
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
  const int frame_tree_node_id1 =
      registry->CreateAndStartHost(std::move(attributes1), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  const int frame_tree_node_id2 =
      registry->CreateAndStartHost(std::move(attributes2), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  EXPECT_NE(frame_tree_node_id1, frame_tree_node_id2);
  PrerenderHost* prerender_host1 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl1);
  PrerenderHost* prerender_host2 =
      registry->FindHostByUrlForTesting(kPrerenderingUrl2);

  // Artificially finish navigation to make the prerender hosts ready to
  // activate the prerendered pages.
  prerender_host1->DidFinishNavigation(nullptr);
  prerender_host2->DidFinishNavigation(nullptr);

  // Select the first host.
  EXPECT_EQ(registry->ReserveHostToActivate(
                kPrerenderingUrl1, *render_frame_host->frame_tree_node()),
            frame_tree_node_id1);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  // The second host should still be findable.
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2),
            prerender_host2);

  // Select the second host.
  EXPECT_EQ(registry->ReserveHostToActivate(
                kPrerenderingUrl2, *render_frame_host->frame_tree_node()),
            frame_tree_node_id2);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  registry->AbandonReservedHost(frame_tree_node_id1);
  registry->AbandonReservedHost(frame_tree_node_id2);
}

TEST_F(PrerenderHostRegistryTest,
       ReserveHostToActivateBeforeReadyForActivation) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("https://example.com/"));
  RenderFrameHostImpl* render_frame_host = web_contents->GetMainFrame();
  ASSERT_TRUE(render_frame_host);

  const GURL kPrerenderingUrl("https://example.com/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;

  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry->FindHostByUrlForTesting(kPrerenderingUrl);

  // The prerender host is not ready for activation yet, so the registry
  // shouldn't select the host and instead should abandon it.
  ASSERT_FALSE(prerender_host->is_ready_for_activation());
  EXPECT_EQ(registry->ReserveHostToActivate(
                kPrerenderingUrl, *render_frame_host->frame_tree_node()),
            kNoFrameTreeNodeId);
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
  const int prerender_frame_tree_node_id =
      registry->CreateAndStartHost(std::move(attributes), *web_contents,
                                   render_frame_host->GetLastCommittedOrigin());
  EXPECT_NE(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  registry->AbandonHost(prerender_frame_tree_node_id);
  EXPECT_EQ(registry->FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

}  // namespace
}  // namespace content
