// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_mac.h"

#include <memory>
#include <optional>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/test/cocoa_helper.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

namespace {

class FakeContentBrowserClient : public TestContentBrowserClient {
 public:
  bool IsDragAllowedByPolicy(const ClipboardEndpoint& source,
                             const DropData& drop_data) override {
    was_called_ = true;
    return false;
  }

  bool was_called() const { return was_called_; }

 private:
  bool was_called_ = false;
};

class WebContentsViewMacTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    const char kGoogleUrl[] = "https://google.com/";
    NavigateAndCommit(GURL(kGoogleUrl));
    original_client_ = SetBrowserClientForTesting(&fake_client_);

    view_ = std::make_unique<WebContentsViewMac>(
        static_cast<WebContentsImpl*>(contents()), nullptr);

    view_->CreateView(gfx::NativeView());
  }

  void TearDown() override {
    view_.reset();
    SetBrowserClientForTesting(original_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

  gfx::ImageSkia CreateValidDragImage() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return static_cast<RenderWidgetHostImpl*>(
        main_test_rfh()->GetRenderWidgetHost());
  }

  WebContentsViewMac* view() { return view_.get(); }
  FakeContentBrowserClient& fake_client() { return fake_client_; }

 private:
  FakeContentBrowserClient fake_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  std::unique_ptr<WebContentsViewMac> view_;
};

TEST_F(WebContentsViewMacTest, StartDragging_DisallowedByPolicy) {
  DropData drop_data;
  drop_data.text = u"test data";

  view()->StartDragging(drop_data, url::Origin(), blink::kDragOperationCopy,
                        CreateValidDragImage(), gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo(),
                        GetRenderWidgetHost());

  EXPECT_TRUE(fake_client().was_called());
}

}  // namespace
}  // namespace content
