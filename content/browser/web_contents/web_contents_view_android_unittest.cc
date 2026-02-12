// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <memory>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

namespace {

class MockWebContentsViewAndroid : public WebContentsViewAndroid {
 public:
  using WebContentsViewAndroid::WebContentsViewAndroid;

  void set_allowed(bool allowed) { allowed_ = allowed; }

  bool was_called() const { return was_called_; }
  bool system_drag_ended_called() const { return system_drag_ended_called_; }

  bool IsDragAllowedByDataControlPolicy(const ClipboardEndpoint& source,
                                        const DropData& drop_data) override {
    was_called_ = true;
    return allowed_;
  }

  void OnSystemDragEnded(RenderWidgetHost* source_rwh) override {
    system_drag_ended_called_ = true;
  }

 private:
  bool was_called_ = false;
  bool system_drag_ended_called_ = false;
  bool allowed_ = false;
};

class WebContentsViewAndroidTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    const char kGoogleUrl[] = "https://google.com/";
    NavigateAndCommit(GURL(kGoogleUrl));

    view_ = std::make_unique<MockWebContentsViewAndroid>(
        static_cast<WebContentsImpl*>(web_contents()), nullptr);
  }

  void TearDown() override {
    view_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  gfx::ImageSkia CreateValidDragImage() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return static_cast<RenderWidgetHostImpl*>(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

  MockWebContentsViewAndroid* view() { return view_.get(); }

 private:
  std::unique_ptr<MockWebContentsViewAndroid> view_;
};

TEST_F(WebContentsViewAndroidTest, StartDragging_BlockedByPolicy) {
  view()->set_allowed(false);

  DropData drop_data;
  drop_data.text = u"Blocked Data";

  view()->StartDragging(drop_data, url::Origin(), blink::kDragOperationCopy,
                        CreateValidDragImage(), gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo(),
                        GetRenderWidgetHost());

  EXPECT_TRUE(view()->was_called());
  EXPECT_TRUE(view()->system_drag_ended_called());
}

}  // namespace
}  // namespace content
