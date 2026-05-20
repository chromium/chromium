// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view_ios_uiview.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface RenderWidgetUIView (Testing)
- (void)setKeyboardHeightForTesting:(CGFloat)height;
@end

@implementation RenderWidgetUIView (Testing)
- (void)setKeyboardHeightForTesting:(CGFloat)height {
  _keyboardHeight = height;
}
@end

namespace content {

class RenderWidgetHostViewIOSTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewIOSTest() = default;
  ~RenderWidgetHostViewIOSTest() override = default;

  RenderWidgetHostViewIOS* rwhv_ios() { return rwhv_ios_; }

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
    process_ = std::make_unique<MockRenderProcessHost>(browser_context());
    site_instance_group_ = base::WrapRefCounted(
        SiteInstanceGroup::CreateForTesting(browser_context(), process_.get()));

    std::unique_ptr<MockRenderWidgetHost> mock_host =
        MockRenderWidgetHost::Create(
            &contents()->GetPrimaryFrameTree(), delegate_.get(),
            site_instance_group_->GetSafeRef(), process_->GetNextRoutingID());
    MockRenderWidgetHost* host = mock_host.get();

    render_view_host_ = new TestRenderViewHost(
        &contents()->GetPrimaryFrameTree(), site_instance_group_.get(),
        contents()
            ->GetSiteInstance()
            ->GetSecurityPrincipal()
            .GetStoragePartitionConfig(),
        std::move(mock_host), contents(), process_->GetNextRoutingID(),
        process_->GetNextRoutingID(), nullptr,
        CreateRenderViewHostCase::kDefault);

    rwhv_ios_ = new RenderWidgetHostViewIOS(host);
  }

  void TearDown() override {
    rwhv_ios_->Destroy();
    render_view_host_.reset();
    delegate_.reset();
    process_->Cleanup();
    site_instance_group_.reset();
    process_ = nullptr;

    RenderViewHostImplTestHarness::TearDown();
  }

  RenderWidgetUIView* GetUIView() {
    return static_cast<RenderWidgetUIView*>(rwhv_ios_->GetNativeView().Get());
  }

  void SetKeyboardHeight(CGFloat height) {
    [GetUIView() setKeyboardHeightForTesting:height];
  }

 private:
  std::unique_ptr<MockRenderProcessHost> process_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  scoped_refptr<RenderViewHostImpl> render_view_host_;
  raw_ptr<RenderWidgetHostViewIOS, DanglingUntriaged> rwhv_ios_ = nullptr;
};

TEST_F(RenderWidgetHostViewIOSTest, VisibleViewportSizeWithNoKeyboard) {
  gfx::Size requested = rwhv_ios()->GetRequestedRendererSize();
  gfx::Size visible = rwhv_ios()->GetVisibleViewportSize();
  EXPECT_EQ(requested, visible);
}

TEST_F(RenderWidgetHostViewIOSTest, VisibleViewportSizeWithKeyboard) {
  const int kKeyboardHeight = 300;
  SetKeyboardHeight(kKeyboardHeight);

  gfx::Size requested = rwhv_ios()->GetRequestedRendererSize();
  gfx::Size visible = rwhv_ios()->GetVisibleViewportSize();

  EXPECT_EQ(requested.width(), visible.width());
  EXPECT_EQ(requested.height() - kKeyboardHeight, visible.height());
}

TEST_F(RenderWidgetHostViewIOSTest, VisibleViewportSizeAfterKeyboardDismiss) {
  SetKeyboardHeight(300);
  EXPECT_NE(rwhv_ios()->GetRequestedRendererSize(),
            rwhv_ios()->GetVisibleViewportSize());

  SetKeyboardHeight(0);
  EXPECT_EQ(rwhv_ios()->GetRequestedRendererSize(),
            rwhv_ios()->GetVisibleViewportSize());
}

TEST_F(RenderWidgetHostViewIOSTest,
       VisibleViewportSizeDevicePxScalesFromDipSize) {
  const int kKeyboardHeight = 300;
  SetKeyboardHeight(kKeyboardHeight);

  gfx::Size visible_dip = rwhv_ios()->GetVisibleViewportSize();
  gfx::Size visible_px = rwhv_ios()->GetVisibleViewportSizeDevicePx();
  float scale = rwhv_ios()->GetDeviceScaleFactor();

  EXPECT_EQ(visible_px, gfx::ScaleToCeiledSize(visible_dip, scale));
}

TEST_F(RenderWidgetHostViewIOSTest, NegativeKeyboardHeightClampedToZero) {
  SetKeyboardHeight(-100);

  gfx::Size requested = rwhv_ios()->GetRequestedRendererSize();
  gfx::Size visible = rwhv_ios()->GetVisibleViewportSize();
  EXPECT_EQ(requested, visible);
}

}  // namespace content
