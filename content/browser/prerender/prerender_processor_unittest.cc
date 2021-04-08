// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_processor.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/system/functions.h"
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

class PrerenderProcessorTest
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<PrerenderTestType> {
 public:
  PrerenderProcessorTest() {
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

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  bool IsMPArchActive() const {
    switch (GetParam()) {
      case kWebContents:
        return false;
      case kMPArch:
        return true;
    }
  }

  RenderFrameHostImpl* GetRenderFrameHost() {
    return web_contents_->GetMainFrame();
  }

  TestWebContents* GetWebContents() { return web_contents_.get(); }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  GURL GetCrossSiteUrl(const std::string& path) {
    return GURL("https://example.test" + path);
  }

  PrerenderHostRegistry* GetPrerenderHostRegistry() const {
    return static_cast<StoragePartitionImpl*>(
               BrowserContext::GetDefaultStoragePartition(
                   browser_context_.get()))
        ->GetPrerenderHostRegistry();
  }

  void NavigateAndCommit(const GURL& url) {
    web_contents_->NavigateAndCommit(url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
};

TEST_P(PrerenderProcessorTest, StartCancel) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  // Cancel() call should abandon the prerender host.
  remote->Cancel();
  remote.FlushForTesting();
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

TEST_P(PrerenderProcessorTest, StartDisconnect) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  // Connection lost should abandon the prerender host.
  remote.reset();
  // FlushForTesting() is no longer available. Instead, use base::RunLoop.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

TEST_P(PrerenderProcessorTest, CancelOnDestruction) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call should register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  // The test assumes `render_frame_host` to be deleted. Disable the
  // back-forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(GetWebContents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

  // Navigate the primary page to a cross-site URL that induces destruction of
  // the render frame host.
  RenderFrameDeletedObserver observer(render_frame_host);
  const GURL kCrossSiteUrl = GetCrossSiteUrl("/cross-site");
  NavigationSimulator::NavigateAndCommitFromBrowser(GetWebContents(),
                                                    kCrossSiteUrl);
  observer.WaitUntilDeleted();

  // The destruction of the render frame host should destroy PrerenderProcessor
  // and cancel prerendering.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

TEST_P(PrerenderProcessorTest, StartTwice) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl;
  attributes1->referrer = blink::mojom::Referrer::New();

  // Start() call should register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes1));
  remote.FlushForTesting();
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  auto attributes2 = blink::mojom::PrerenderAttributes::New();
  attributes2->url = kPrerenderingUrl;
  attributes2->referrer = blink::mojom::Referrer::New();

  // Call Start() again. This should be reported as a bad mojo message.
  ASSERT_TRUE(bad_message_error.empty());
  remote->Start(std::move(attributes2));
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "PP_START_TWICE");
}

TEST_P(PrerenderProcessorTest, CancelBeforeStart) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes1 = blink::mojom::PrerenderAttributes::New();
  attributes1->url = kPrerenderingUrl;
  attributes1->referrer = blink::mojom::Referrer::New();

  // Call Cancel() before Start(). This should be reported as a bad mojo
  // message.
  ASSERT_TRUE(bad_message_error.empty());
  remote->Cancel();
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "PP_CANCEL_BEFORE_START");
}

// Tests that prerendering a cross-origin URL is aborted. Cross-origin
// prerendering is not supported for now, but we plan to support it later
// (https://crbug.com/1176054).
TEST_P(PrerenderProcessorTest, CrossOrigin) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  const GURL kPrerenderingUrl = GetCrossOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call with the cross-origin URL should be reported as a bad message.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_EQ(bad_message_error, "PP_CROSS_ORIGIN");
}

// Tests that prerendering triggered by <link rel=next> is aborted. This trigger
// is not supported for now, but we may want to support it if NoStatePrefetch
// re-enables it again. See https://crbug.com/1161545.
TEST_P(PrerenderProcessorTest, RelTypeNext) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  // Set kNext instead of the default kPrerender.
  attributes->rel_type = blink::mojom::PrerenderRelType::kNext;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call with kNext should be aborted.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  // Start() call with kNext is a valid request, currently it's not supported
  // though. The request shouldn't result in a bad message failure.
  EXPECT_TRUE(bad_message_error.empty());

  // Cancel() call should not be reported as a bad mojo message as well.
  remote->Cancel();
  remote.FlushForTesting();
  EXPECT_TRUE(bad_message_error.empty());
}

TEST_P(PrerenderProcessorTest, StartAfterNavigation) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  mojo::Remote<blink::mojom::PrerenderProcessor> remote;
  render_frame_host->BindPrerenderProcessor(
      remote.BindNewPipeAndPassReceiver());

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();

  // Navigate to a same-site, but different origin URL.
  NavigateAndCommit(GetCrossOriginUrl("/navigate"));

  // Start() call should not register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  remote->Start(std::move(attributes));
  remote.FlushForTesting();
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderProcessorTest,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

}  // namespace
}  // namespace content
