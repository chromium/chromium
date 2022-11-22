// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/anchor_element_interaction_host_impl.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class PreloadingObserverImpl : public PreloadingDeciderObserverForTesting {
 public:
  void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override {}
  void OnPointerDown(const GURL& url) override { on_pointer_down_url_ = url; }
  void OnPointerHover(const GURL& url) override { on_pointer_hover_url_ = url; }

  absl::optional<GURL> on_pointer_down_url_;
  absl::optional<GURL> on_pointer_hover_url_;
};

class AnchorElementInteractionHostImplTest : public RenderViewHostTestHarness {
 public:
  AnchorElementInteractionHostImplTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(GetRenderFrameHost());
    auto observer = std::make_unique<PreloadingObserverImpl>();
    observer_ = observer.get();
    preloading_decider->SetObserverForTesting(std::move(observer));
  }

  void TearDown() override {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(GetRenderFrameHost());
    preloading_decider->ResetObserverForTesting();
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl* GetRenderFrameHost() {
    return web_contents_->GetPrimaryMainFrame();
  }

  PreloadingObserverImpl* GetObserver() { return observer_; }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  raw_ptr<PreloadingObserverImpl> observer_;
};

TEST_F(AnchorElementInteractionHostImplTest, OnPointerEvents) {
  RenderFrameHostImpl* render_frame_host = GetRenderFrameHost();

  mojo::Remote<blink::mojom::AnchorElementInteractionHost> remote;
  AnchorElementInteractionHostImpl::Create(render_frame_host,
                                           remote.BindNewPipeAndPassReceiver());

  GetObserver()->on_pointer_down_url_.reset();
  GetObserver()->on_pointer_hover_url_.reset();
  const auto pointer_down_url = GURL("www.example.com/page1.html");
  remote->OnPointerDown(pointer_down_url);
  remote.FlushForTesting();
  EXPECT_EQ(pointer_down_url, GetObserver()->on_pointer_down_url_);
  EXPECT_FALSE(GetObserver()->on_pointer_hover_url_.has_value());

  GetObserver()->on_pointer_down_url_.reset();
  GetObserver()->on_pointer_hover_url_.reset();
  const auto pointer_hover_url = GURL("www.example.com/page2.html");
  remote->OnPointerHover(pointer_hover_url);
  remote.FlushForTesting();
  EXPECT_FALSE(GetObserver()->on_pointer_down_url_.has_value());
  EXPECT_EQ(pointer_hover_url, GetObserver()->on_pointer_hover_url_);
}

}  // namespace
}  // namespace content
