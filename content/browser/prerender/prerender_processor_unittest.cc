// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_processor.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

class PrerenderProcessorTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderProcessorTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
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

  RenderFrameHostImpl* GetRenderFrameHost() {
    return web_contents_->GetMainFrame();
  }

  GURL GetSameOriginUrl(const std::string& path) {
    return GURL("https://example.com" + path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return GURL("https://other.example.com" + path);
  }

  PrerenderHostRegistry* GetPrerenderHostRegistry() const {
    return web_contents_->GetPrerenderHostRegistry();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
};

TEST_F(PrerenderProcessorTest, StartDestruct) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  auto prerender_processor =
      std::make_unique<PrerenderProcessor>(*render_frame_host);

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();
  attributes->trigger_type =
      blink::mojom::PrerenderTriggerType::kSpeculationRule;
  // Start() call should register a new prerender host.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  prerender_processor->Start(std::move(attributes));
  EXPECT_TRUE(registry->FindHostByUrlForTesting(kPrerenderingUrl));

  // Destroying `prerender_processor` should abandon the prerender host.
  prerender_processor.reset();
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that prerendering a cross-origin URL is aborted. Cross-origin
// prerendering is not supported for now, but we plan to support it later
// (https://crbug.com/1176054).
TEST_F(PrerenderProcessorTest, CrossOrigin) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  auto prerender_processor =
      std::make_unique<PrerenderProcessor>(*render_frame_host);

  const GURL kPrerenderingUrl = GetCrossOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  attributes->referrer = blink::mojom::Referrer::New();
  attributes->trigger_type =
      blink::mojom::PrerenderTriggerType::kSpeculationRule;

  // Start() call with the cross-origin URL should hit a DCHECK.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_CHECK_DEATH(prerender_processor->Start(std::move(attributes)));
}

// Tests that prerendering triggered by <link rel=next> is aborted. This trigger
// is not supported for now, but we may want to support it if NoStatePrefetch
// re-enables it again. See https://crbug.com/1161545.
// TODO(https://crbug.com/1217045): Remove this test after Prerender2 does not
// rely on PrerenderAttributesPtr.
TEST_F(PrerenderProcessorTest, RelTypeNext) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  auto prerender_processor =
      std::make_unique<PrerenderProcessor>(*render_frame_host);

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  // Set kLinkRelNext instead of the default kSpeculationRule.
  attributes->trigger_type = blink::mojom::PrerenderTriggerType::kLinkRelNext;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call with kNext should be aborted.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  ASSERT_DCHECK_DEATH(prerender_processor->Start(std::move(attributes)));
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

// Tests that prerendering triggered by <link rel=prerender> is aborted. This
// trigger is not supported for now, but we want to support it after Prerender2
// works well.
// TODO(https://crbug.com/1217045): Remove this test after Prerender2 does not
// rely on PrerenderAttributesPtr.
TEST_F(PrerenderProcessorTest, RelTypePrerender) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();
  PrerenderHostRegistry* registry = GetPrerenderHostRegistry();

  auto prerender_processor =
      std::make_unique<PrerenderProcessor>(*render_frame_host);

  const GURL kPrerenderingUrl = GetSameOriginUrl("/next");
  auto attributes = blink::mojom::PrerenderAttributes::New();
  attributes->url = kPrerenderingUrl;
  // Set kLinkRelPrerender instead of the default kSpeculationRule.
  attributes->trigger_type =
      blink::mojom::PrerenderTriggerType::kLinkRelPrerender;
  attributes->referrer = blink::mojom::Referrer::New();

  // Start() call with kLinkRelPrerender should be aborted.
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
  ASSERT_DCHECK_DEATH(prerender_processor->Start(std::move(attributes)));
  EXPECT_FALSE(registry->FindHostByUrlForTesting(kPrerenderingUrl));
}

}  // namespace
}  // namespace content
