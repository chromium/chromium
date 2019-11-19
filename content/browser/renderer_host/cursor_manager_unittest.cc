// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cursor_manager.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/common/cursor_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

// CursorManager is only instantiated on Aura and Mac.
#if defined(USE_AURA) || defined(OS_MACOSX)

namespace content {

namespace {

class MockRenderWidgetHostViewForCursors : public TestRenderWidgetHostView {
 public:
  MockRenderWidgetHostViewForCursors(RenderWidgetHost* host, bool top_view)
      : TestRenderWidgetHostView(host) {
    if (top_view)
      cursor_manager_.reset(new CursorManager(this));
  }

  void DisplayCursor(const WebCursor& cursor) override {
    current_cursor_ = cursor;
  }

  CursorManager* GetCursorManager() override { return cursor_manager_.get(); }

  const WebCursor& cursor() { return current_cursor_; }

 private:
  WebCursor current_cursor_;
  std::unique_ptr<CursorManager> cursor_manager_;
};

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  static MockRenderWidgetHost* Create(RenderWidgetHostDelegate* delegate,
                                      RenderProcessHost* process,
                                      int32_t routing_id) {
    mojo::PendingRemote<mojom::Widget> widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(
            widget.InitWithNewPipeAndPassReceiver());

    return new MockRenderWidgetHost(delegate, process, routing_id,
                                    std::move(widget_impl), std::move(widget));
  }

 private:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojo::PendingRemote<mojom::Widget> widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             /*hidden=*/false,
                             std::make_unique<FrameTokenMessageQueue>()),
        widget_impl_(std::move(widget_impl)) {}

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

class CursorManagerTest : public testing::Test {
 public:
  CursorManagerTest() {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    process_host_.reset(new MockRenderProcessHost(browser_context_.get()));
    widget_host_.reset(MakeNewWidgetHost());
    top_view_ =
        new MockRenderWidgetHostViewForCursors(widget_host_.get(), true);
  }

  RenderWidgetHostImpl* MakeNewWidgetHost() {
    int32_t routing_id = process_host_->GetNextRoutingID();
    return MockRenderWidgetHost::Create(&delegate_, process_host_.get(),
                                        routing_id);
  }

  void TearDown() override {
    if (top_view_)
      delete top_view_;

    widget_host_.reset();
    process_host_.reset();
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;

  // Tests should set this to nullptr if they've already triggered its
  // destruction.
  MockRenderWidgetHostViewForCursors* top_view_;

  MockRenderWidgetHostDelegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorManagerTest);
};

}  // namespace

// Verify basic CursorManager functionality when no OOPIFs are present.
TEST_F(CursorManagerTest, CursorOnSingleView) {
  // Simulate mouse over the top-level frame without an UpdateCursor message.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(top_view_);

  // The view should be using the default cursor.
  EXPECT_EQ(top_view_->cursor(), WebCursor());

  CursorInfo cursor_info(ui::CursorType::kHand);
  WebCursor cursor_hand(cursor_info);

  // Update the view with a non-default cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);

  // Verify the RenderWidgetHostView now uses the correct cursor.
  EXPECT_EQ(top_view_->cursor(), cursor_hand);
}

// Verify cursor interactions between a parent frame and an out-of-process
// child frame.
TEST_F(CursorManagerTest, CursorOverChildView) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view(
      new MockRenderWidgetHostViewForCursors(widget_host.get(), false));

  CursorInfo cursor_info(ui::CursorType::kHand);
  WebCursor cursor_hand(cursor_info);

  // Set the child frame's cursor to a hand. This should not propagate to the
  // top-level view without the mouse moving over the child frame.
  top_view_->GetCursorManager()->UpdateCursor(child_view.get(), cursor_hand);
  EXPECT_NE(top_view_->cursor(), cursor_hand);

  // Now moving the mouse over the child frame should update the overall cursor.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view.get());
  EXPECT_EQ(top_view_->cursor(), cursor_hand);

  // Destruction of the child view should restore the parent frame's cursor.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view.get());
  EXPECT_NE(top_view_->cursor(), cursor_hand);
}

// Verify interactions between two independent OOPIFs, including interleaving
// cursor updates and mouse movements. This simulates potential race
// conditions between cursor updates.
TEST_F(CursorManagerTest, CursorOverMultipleChildViews) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host1(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view1(
      new MockRenderWidgetHostViewForCursors(widget_host1.get(), false));
  std::unique_ptr<RenderWidgetHostImpl> widget_host2(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view2(
      new MockRenderWidgetHostViewForCursors(widget_host2.get(), false));

  CursorInfo cursor_info_hand(ui::CursorType::kHand);
  WebCursor cursor_hand(cursor_info_hand);

  CursorInfo cursor_info_cross(ui::CursorType::kCross);
  WebCursor cursor_cross(cursor_info_cross);

  CursorInfo cursor_info_pointer(ui::CursorType::kPointer);
  WebCursor cursor_pointer(cursor_info_pointer);

  // Initialize each View to a different cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), cursor_cross);
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(),
                                              cursor_pointer);
  EXPECT_EQ(top_view_->cursor(), cursor_hand);

  // Simulate moving the mouse between child views and receiving cursor updates.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view1.get());
  EXPECT_EQ(top_view_->cursor(), cursor_cross);
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view2.get());
  EXPECT_EQ(top_view_->cursor(), cursor_pointer);

  // Simulate cursor updates to both child views and the parent view. An
  // update to child_view1 or the parent view should not change the current
  // cursor because the mouse is over child_view2.
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), cursor_hand);
  EXPECT_EQ(top_view_->cursor(), cursor_pointer);
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(), cursor_cross);
  EXPECT_EQ(top_view_->cursor(), cursor_cross);
  top_view_->GetCursorManager()->UpdateCursor(top_view_, cursor_hand);
  EXPECT_EQ(top_view_->cursor(), cursor_cross);

  // Similarly, destroying child_view1 should have no effect on the cursor,
  // but destroying child_view2 should change it.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view1.get());
  EXPECT_EQ(top_view_->cursor(), cursor_cross);
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view2.get());
  EXPECT_EQ(top_view_->cursor(), cursor_hand);
}

}  // namespace content

#endif  // defined(USE_AURA) || defined(OS_MACOSX)
