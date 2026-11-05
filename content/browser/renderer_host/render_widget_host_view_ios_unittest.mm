// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view_ios_uiview.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

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
    rwhv_ios_->DestroyOrDefer();
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

// Exercise UIKit sizing instead of the fixed viewport used by the test app.
class RenderWidgetHostViewIOSNativeLayoutTest
    : public RenderWidgetHostViewIOSTest {
 protected:
  void SetUp() override {
    RenderWidgetHostViewIOSTest::SetUp();
    rwhv_ios()->SetUseFixedSizeForTesting(false);
  }

  void AttachToParent(CGRect frame, UIEdgeInsets insets = UIEdgeInsetsZero) {
    scroll_view_ = [[UIScrollView alloc] initWithFrame:frame];
    scroll_view_.contentInset = insets;
    rwhv_ios()->InitAsChild(gfx::NativeView(scroll_view_));
    Layout();
  }

  void Layout() {
    [scroll_view_ layoutIfNeeded];
    [GetUIView() setNeedsLayout];
    [GetUIView() layoutIfNeeded];
  }

  void ExpectViewportSize(gfx::Size expected_size) {
    EXPECT_EQ(expected_size, gfx::Rect(GetUIView().bounds).size());
    EXPECT_EQ(expected_size, rwhv_ios()->GetRequestedRendererSize());
    EXPECT_EQ(expected_size,
              rwhv_ios()->BrowserCompositor()->GetRendererSize());
  }

  void AckGesture(blink::WebInputEvent::Type type) {
    blink::WebGestureEvent event(type, blink::WebInputEvent::kNoModifiers,
                                 base::TimeTicks::Now(),
                                 blink::WebGestureDevice::kTouchscreen);
    rwhv_ios()->GestureEventAck(
        event, blink::mojom::InputEventResultSource::kCompositorThread,
        blink::mojom::InputEventResultState::kConsumed);
  }

  UIScrollView* scroll_view_ = nil;
};

TEST_F(RenderWidgetHostViewIOSNativeLayoutTest, LayoutBeforeScrollMetadata) {
  AttachToParent(CGRectZero);
  ExpectViewportSize(gfx::Size());

  // A new tab receives its frame after attachment, before any scroll metadata.
  scroll_view_.frame = CGRectMake(0, 0, 390, 700);
  Layout();
  ExpectViewportSize(gfx::Size(390, 700));

  // The same resize path must retain padding in the obscured-inset layout.
  scroll_view_.contentInset = UIEdgeInsetsMake(50, 4, 40, 6);
  scroll_view_.frame = CGRectMake(0, 0, 320, 610);
  Layout();
  ExpectViewportSize(gfx::Size(310, 520));
}

TEST_F(RenderWidgetHostViewIOSNativeLayoutTest,
       ParentResizeDeferredWhileScrolling) {
  AttachToParent(CGRectMake(0, 0, 320, 610), UIEdgeInsetsMake(50, 4, 40, 6));
  const gfx::Size initial_size = rwhv_ios()->GetRequestedRendererSize();
  AckGesture(blink::WebInputEvent::Type::kGestureScrollBegin);

  scroll_view_.frame = CGRectMake(0, 0, 390, 700);
  Layout();

  EXPECT_EQ(initial_size, rwhv_ios()->GetRequestedRendererSize());
  EXPECT_NE(initial_size, gfx::Rect(GetUIView().bounds).size());

  AckGesture(blink::WebInputEvent::Type::kGestureScrollEnd);
  ExpectViewportSize(gfx::Size(380, 610));
}

}  // namespace content
