// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preconnector.h"

#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MockAnchorElementPreconnector : public AnchorElementPreconnectDelegate {
 public:
  explicit MockAnchorElementPreconnector(RenderFrameHost& render_frame_host) {}
  ~MockAnchorElementPreconnector() override = default;

  void MaybePreconnect(const GURL& target) override { target_ = target; }
  std::optional<GURL>& Target() { return target_; }

  base::WeakPtr<MockAnchorElementPreconnector> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::optional<GURL> target_;
  base::WeakPtrFactory<MockAnchorElementPreconnector> weak_ptr_factory_{this};
};

class MockContentBrowserClient : public TestContentBrowserClient {
 public:
  MockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }
  ~MockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  std::unique_ptr<AnchorElementPreconnectDelegate>
  CreateAnchorElementPreconnectDelegate(
      RenderFrameHost& render_frame_host) override {
    auto delegate =
        std::make_unique<MockAnchorElementPreconnector>(render_frame_host);
    delegate_ = delegate->AsWeakPtr();
    return delegate;
  }

  base::WeakPtr<MockAnchorElementPreconnector> GetDelegate() {
    return delegate_;
  }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
  base::WeakPtr<MockAnchorElementPreconnector> delegate_;
};

class PreconnectorTest : public RenderViewHostTestHarness {
 public:
  PreconnectorTest() = default;
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    web_contents_->NavigateAndCommit(GURL("https://example.com"));
  }
  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& GetPrimaryMainFrame() {
    return web_contents_->GetPrimaryPage().GetMainDocument();
  }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
};

TEST_F(PreconnectorTest, MaybePreconnect) {
  MockContentBrowserClient browser_client;
  auto preconnector = Preconnector(GetPrimaryMainFrame());
  base::WeakPtr<MockAnchorElementPreconnector> delegate =
      browser_client.GetDelegate();
  ASSERT_TRUE(delegate);

  const auto url = GURL("https://example.com/page1");
  preconnector.MaybePreconnect(url);
  EXPECT_TRUE(delegate->Target().has_value());
  EXPECT_EQ(url, delegate->Target().value());
}

}  // namespace
}  // namespace content
