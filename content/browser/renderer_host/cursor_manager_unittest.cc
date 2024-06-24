// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/cursor_manager.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

// CursorManager is only instantiated on Aura and Mac.
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)

namespace content {

namespace {

const ui::Cursor kCursorHand(ui::mojom::CursorType::kHand);
const ui::Cursor kCursorCross(ui::mojom::CursorType::kCross);
const ui::Cursor kCursorPointer(ui::mojom::CursorType::kPointer);

ui::Cursor CreateCustomCursor(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);

  return ui::Cursor::NewCustom(std::move(bitmap), /*hotspot=*/gfx::Point(0, 0),
                               /*image_scale_factor=*/1.0f);
}

const ui::Cursor kCursorCustom = CreateCustomCursor(0, 0);

class MockRenderWidgetHostViewForCursors : public TestRenderWidgetHostView {
 public:
  MockRenderWidgetHostViewForCursors(RenderWidgetHost* host, bool top_view)
      : TestRenderWidgetHostView(host) {
    if (top_view) {
      cursor_manager_ = std::make_unique<input::CursorManager>(this);
    }
  }

  void DisplayCursor(const ui::Cursor& cursor) override {
    current_cursor_ = cursor;
  }

  input::CursorManager* GetCursorManager() override {
    return cursor_manager_.get();
  }

  const ui::Cursor& cursor() { return current_cursor_; }

 private:
  ui::Cursor current_cursor_;
  std::unique_ptr<input::CursorManager> cursor_manager_;
};

class CursorManagerTest : public testing::Test {
 public:
  CursorManagerTest() = default;

  CursorManagerTest(const CursorManagerTest&) = delete;
  CursorManagerTest& operator=(const CursorManagerTest&) = delete;

  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_host_.get()));
    widget_host_ = MakeNewWidgetHost();
    top_view_ = std::make_unique<MockRenderWidgetHostViewForCursors>(
        widget_host_.get(), true);
  }

  std::unique_ptr<RenderWidgetHostImpl> MakeNewWidgetHost() {
    int32_t routing_id = process_host_->GetNextRoutingID();
    return MockRenderWidgetHost::Create(
        /*frame_tree=*/nullptr, &delegate_, site_instance_group_->GetSafeRef(),
        routing_id);
  }

  void TearDown() override {
    top_view_.reset();
    widget_host_ = nullptr;
    process_host_->Cleanup();
    site_instance_group_.reset();
    process_host_ = nullptr;
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  std::unique_ptr<MockRenderWidgetHostViewForCursors> top_view_;

  MockRenderWidgetHostDelegate delegate_;
};

}  // namespace

// Verify basic CursorManager functionality when no OOPIFs are present.
TEST_F(CursorManagerTest, CursorOnSingleView) {
  // Simulate mouse over the top-level frame without an UpdateCursor message.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(top_view_.get());

  // The view should be using the default cursor.
  EXPECT_EQ(top_view_->cursor(), ui::Cursor());

  // Update the view with a non-default cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);

  // Verify the RenderWidgetHostView now uses the correct cursor.
  EXPECT_EQ(top_view_->cursor(), kCursorHand);
}

// Verify cursor interactions between a parent frame and an out-of-process
// child frame.
TEST_F(CursorManagerTest, CursorOverChildView) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view(
      new MockRenderWidgetHostViewForCursors(widget_host.get(), false));

  // Set the child frame's cursor to a hand. This should not propagate to the
  // top-level view without the mouse moving over the child frame.
  top_view_->GetCursorManager()->UpdateCursor(child_view.get(), kCursorHand);
  EXPECT_NE(top_view_->cursor(), kCursorHand);

  // Now moving the mouse over the child frame should update the overall cursor.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view.get());
  EXPECT_EQ(top_view_->cursor(), kCursorHand);

  // Destruction of the child view should restore the parent frame's cursor.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view.get());
  EXPECT_NE(top_view_->cursor(), kCursorHand);
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

  // Initialize each View to a different cursor.
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), kCursorCross);
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(),
                                              kCursorPointer);
  EXPECT_EQ(top_view_->cursor(), kCursorHand);

  // Simulate moving the mouse between child views and receiving cursor updates.
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view1.get());
  EXPECT_EQ(top_view_->cursor(), kCursorCross);
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view2.get());
  EXPECT_EQ(top_view_->cursor(), kCursorPointer);

  // Simulate cursor updates to both child views and the parent view. An
  // update to child_view1 or the parent view should not change the current
  // cursor because the mouse is over child_view2.
  top_view_->GetCursorManager()->UpdateCursor(child_view1.get(), kCursorHand);
  EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  top_view_->GetCursorManager()->UpdateCursor(child_view2.get(), kCursorCross);
  EXPECT_EQ(top_view_->cursor(), kCursorCross);
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  EXPECT_EQ(top_view_->cursor(), kCursorCross);

  // Similarly, destroying child_view1 should have no effect on the cursor,
  // but destroying child_view2 should change it.
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view1.get());
  EXPECT_EQ(top_view_->cursor(), kCursorCross);
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view2.get());
  EXPECT_EQ(top_view_->cursor(), kCursorHand);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorsAreNotAllowed) {
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorCustom);
  EXPECT_EQ(top_view_->cursor(), kCursorCustom);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustom);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorsAreNotAllowedAboveSizeLimit) {
  const ui::Cursor kCursorCustomLarge = CreateCustomCursor(20, 50);

  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(),
                                              kCursorCustomLarge);
  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/40);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorsAreAllowedBelowSizeLimit) {
  const ui::Cursor kCursorCustomLarge = CreateCustomCursor(20, 35);

  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(),
                                              kCursorCustomLarge);
  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/40);
    EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorSubjectToMultipleSizeLimits) {
  const ui::Cursor kCursorCustomLarge = CreateCustomCursor(20, 35);

  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(),
                                              kCursorCustomLarge);
  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);

  {
    auto disallow_scope1 =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/40);
    EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);

    auto disallow_scope2 =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/30);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);

    // Running the first closure leaves the restriction from the second closure
    // in place.
    disallow_scope1.RunAndReset();
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustomLarge);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_OtherCursorsStillAllowed) {
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  EXPECT_EQ(top_view_->cursor(), kCursorHand);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorHand);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorHand);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorSetDuringScope) {
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  EXPECT_EQ(top_view_->cursor(), kCursorHand);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorHand);

    top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorCustom);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustom);
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorRemovedDuringScope) {
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorCustom);
  EXPECT_EQ(top_view_->cursor(), kCursorCustom);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);

    top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
    EXPECT_EQ(top_view_->cursor(), kCursorHand);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorHand);
}

TEST_F(CursorManagerTest, CustomCursorDisallowedScope_MultipleScopes) {
  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorCustom);
  EXPECT_EQ(top_view_->cursor(), kCursorCustom);

  auto disallow_scope1 =
      top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
          /*max_dimension_dips=*/0);
  auto disallow_scope2 =
      top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
          /*max_dimension_dips=*/0);
  EXPECT_EQ(top_view_->cursor(), kCursorPointer);

  disallow_scope1.RunAndReset();
  EXPECT_EQ(top_view_->cursor(), kCursorPointer);

  auto disallow_scope3 =
      top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
          /*max_dimension_dips=*/0);
  disallow_scope2.RunAndReset();
  EXPECT_EQ(top_view_->cursor(), kCursorPointer);

  disallow_scope3.RunAndReset();
  EXPECT_EQ(top_view_->cursor(), kCursorCustom);
}

TEST_F(CursorManagerTest, CustomCursorDisallowedScope_CustomCursorViewFocused) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view(
      new MockRenderWidgetHostViewForCursors(widget_host.get(), false));

  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  top_view_->GetCursorManager()->UpdateCursor(child_view.get(), kCursorCustom);
  EXPECT_EQ(top_view_->cursor(), kCursorHand);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorHand);

    top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view.get());
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorCustom);
  top_view_->GetCursorManager()->ViewBeingDestroyed(child_view.get());
}

TEST_F(CursorManagerTest,
       CustomCursorDisallowedScope_CustomCursorViewFocusRemoved) {
  std::unique_ptr<RenderWidgetHostImpl> widget_host(MakeNewWidgetHost());
  std::unique_ptr<MockRenderWidgetHostViewForCursors> child_view(
      new MockRenderWidgetHostViewForCursors(widget_host.get(), false));

  top_view_->GetCursorManager()->UpdateCursor(top_view_.get(), kCursorHand);
  top_view_->GetCursorManager()->UpdateCursor(child_view.get(), kCursorCustom);
  top_view_->GetCursorManager()->UpdateViewUnderCursor(child_view.get());
  EXPECT_EQ(top_view_->cursor(), kCursorCustom);

  {
    auto disallow_scope =
        top_view_->GetCursorManager()->CreateDisallowCustomCursorScope(
            /*max_dimension_dips=*/0);
    EXPECT_EQ(top_view_->cursor(), kCursorPointer);

    top_view_->GetCursorManager()->UpdateViewUnderCursor(top_view_.get());
    EXPECT_EQ(top_view_->cursor(), kCursorHand);
  }

  EXPECT_EQ(top_view_->cursor(), kCursorHand);
}

}  // namespace content

#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)
