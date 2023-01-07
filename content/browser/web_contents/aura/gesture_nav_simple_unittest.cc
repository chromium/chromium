// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/gesture_nav_simple.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/window.h"

#include <memory>

namespace content {

namespace {

// A subclass of TestWebContents that offers a fake content window.
class GestureNavTestWebContents : public TestWebContents {
 public:
  explicit GestureNavTestWebContents(
      BrowserContext* browser_context,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window)
      : TestWebContents(browser_context),
        fake_native_view_(std::move(fake_native_view)),
        fake_contents_window_(std::move(fake_contents_window)) {}
  ~GestureNavTestWebContents() override {}

  static std::unique_ptr<GestureNavTestWebContents> Create(
      BrowserContext* browser_context,
      scoped_refptr<SiteInstance> instance,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window) {
    std::unique_ptr<GestureNavTestWebContents> web_contents =
        std::make_unique<GestureNavTestWebContents>(
            browser_context, std::move(fake_native_view),
            std::move(fake_contents_window));
    web_contents->Init(
        WebContents::CreateParams(browser_context, std::move(instance)),
        blink::FramePolicy());
    return web_contents;
  }

  // Overriden from TestWebContents:
  gfx::NativeView GetNativeView() override { return fake_native_view_.get(); }

  gfx::NativeView GetContentNativeView() override {
    return fake_contents_window_.get();
  }

 private:
  std::unique_ptr<aura::Window> fake_native_view_;
  std::unique_ptr<aura::Window> fake_contents_window_;
};

}  // namespace

class GestureNavSimpleTest : public RenderViewHostImplTestHarness {
 public:
  GestureNavSimpleTest()
      : first_("https://www.google.com"), second_("http://www.chromium.org") {}

  ~GestureNavSimpleTest() override = default;
  GestureNavSimpleTest(const GestureNavSimpleTest&) = delete;
  GestureNavSimpleTest& operator=(const GestureNavSimpleTest&) = delete;

 protected:
  // RenderViewHostImplTestHarness:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up the fake web contents native view.
    auto fake_native_view = std::make_unique<aura::Window>(nullptr);
    fake_native_view->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_native_view.get());
    fake_native_view->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Set up the fake contents window.
    auto fake_contents_window = std::make_unique<aura::Window>(nullptr);
    fake_contents_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_contents_window.get());
    fake_contents_window->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Replace the default test web contents with our custom class.
    SetContents(GestureNavTestWebContents::Create(
        browser_context(), SiteInstance::Create(browser_context()),
        std::move(fake_native_view), std::move(fake_contents_window)));

    // Add two pages to navigation history.
    contents()->NavigateAndCommit(first_);
    EXPECT_TRUE(controller().GetVisibleEntry());
    EXPECT_FALSE(controller().CanGoBack());

    contents()->NavigateAndCommit(second_);
    EXPECT_TRUE(controller().CanGoBack());
    EXPECT_FALSE(controller().CanGoForward());

    gesture_nav_simple_ = std::make_unique<GestureNavSimple>(contents());
  }

  void TearDown() override {
    gesture_nav_simple_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  const GURL first() const { return first_; }
  const GURL second() const { return second_; }

  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source,
                              cc::OverscrollBehavior behavior) {
    gesture_nav_simple_->OnOverscrollModeChange(old_mode, new_mode, source,
                                                behavior);
  }

  void OnOverscrollComplete(OverscrollMode overscroll_mode) {
    gesture_nav_simple_->OnOverscrollComplete(overscroll_mode);
  }

  OverscrollMode mode() const { return gesture_nav_simple_->mode_; }
  OverscrollSource source() const { return gesture_nav_simple_->source_; }

 private:
  // Test URLs.
  const GURL first_;
  const GURL second_;

  std::unique_ptr<GestureNavSimple> gesture_nav_simple_;
};

// Tests that setting 'overscroll-behavior-x' to 'auto' allows gesture-nav.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorXAutoAllowsGestureNav) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_auto;
  behavior_x_auto.x = cc::OverscrollBehavior::Type::kAuto;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_EAST, mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, source());
}

// Tests that setting 'overscroll-behavior-x' to 'contain' prevents gesture-nav.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorXContainPreventsGestureNav) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_contain;
  behavior_x_contain.x = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that setting 'overscroll-behavior-x' to 'none' prevents gesture-nav.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorXNonePreventsGestureNav) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_none;
  behavior_x_none.x = cc::OverscrollBehavior::Type::kNone;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_none);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that setting 'overscroll-behavior-y' to 'auto' allows pull-to-refresh.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorYAutoAllowsPullToRefresh) {
  cc::OverscrollBehavior behavior_y_auto;
  behavior_y_auto.y = cc::OverscrollBehavior::Type::kAuto;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_SOUTH, mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, source());
}

// Tests that setting 'overscroll-behavior-y' to 'contain' prevents
// pull-to-refresh.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorYContainPreventsPullToRefresh) {
  cc::OverscrollBehavior behavior_y_contain;
  behavior_y_contain.y = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that setting 'overscroll-behavior-y' to 'none' prevents
// pull-to-refresh.
TEST_F(GestureNavSimpleTest, OverscrollBehaviorYNonePreventsPullToRefresh) {
  cc::OverscrollBehavior behavior_y_none;
  behavior_y_none.y = cc::OverscrollBehavior::Type::kNone;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_none);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that setting 'overscroll-behavior-x' to a value that prevents
// gesture-nav after it has started does not affect aborting it.
TEST_F(GestureNavSimpleTest, PreventGestureNavBeforeAbort) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_auto;
  behavior_x_auto.x = cc::OverscrollBehavior::Type::kAuto;
  cc::OverscrollBehavior behavior_x_contain;
  behavior_x_contain.x = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_EAST, mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, source());

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_EAST,
                         OverscrollMode::OVERSCROLL_NONE,
                         OverscrollSource::TOUCHSCREEN, behavior_x_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());
}

// Tests that after gesture-nav was prevented due to 'overscroll-behavior-x',
// setting it to 'auto' does not affect aborting overscroll.
TEST_F(GestureNavSimpleTest, AllowGestureNavBeforeAbort) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_contain;
  behavior_x_contain.x = cc::OverscrollBehavior::Type::kContain;
  cc::OverscrollBehavior behavior_x_auto;
  behavior_x_auto.x = cc::OverscrollBehavior::Type::kAuto;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_EAST,
                         OverscrollMode::OVERSCROLL_NONE,
                         OverscrollSource::TOUCHSCREEN, behavior_x_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());
}

// Tests that preventing gesture-nav using 'overscroll-behavior-x' does not
// affect completing overscroll.
TEST_F(GestureNavSimpleTest, CompletePreventedGestureNav) {
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());

  cc::OverscrollBehavior behavior_x_contain;
  behavior_x_contain.x = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_EAST,
                         OverscrollSource::TOUCHSCREEN, behavior_x_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());

  OnOverscrollComplete(OverscrollMode::OVERSCROLL_EAST);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
  EXPECT_EQ(second(), contents()->GetLastCommittedURL());
}

// Tests that setting 'overscroll-behavior-y' to a value that prevents
// pull-to-refresh after it has started does not affect aborting it.
TEST_F(GestureNavSimpleTest, PreventPullToRefreshBeforeAbort) {
  cc::OverscrollBehavior behavior_y_auto;
  behavior_y_auto.y = cc::OverscrollBehavior::Type::kAuto;
  cc::OverscrollBehavior behavior_y_contain;
  behavior_y_contain.y = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_SOUTH, mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, source());

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollMode::OVERSCROLL_NONE,
                         OverscrollSource::TOUCHSCREEN, behavior_y_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that after pull-to-refresh was prevented due to
// 'overscroll-behavior-y', setting it to 'auto' does not affect aborting
// overscroll.
TEST_F(GestureNavSimpleTest, AllowPullToRefreshBeforeAbort) {
  cc::OverscrollBehavior behavior_y_contain;
  behavior_y_contain.y = cc::OverscrollBehavior::Type::kContain;
  cc::OverscrollBehavior behavior_y_auto;
  behavior_y_auto.y = cc::OverscrollBehavior::Type::kAuto;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollMode::OVERSCROLL_NONE,
                         OverscrollSource::TOUCHSCREEN, behavior_y_auto);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

// Tests that preventing pull-to-refresh using 'overscroll-behavior-y' does not
// affect completing overscroll.
TEST_F(GestureNavSimpleTest, CompletePreventedPullToRefresh) {
  cc::OverscrollBehavior behavior_y_contain;
  behavior_y_contain.y = cc::OverscrollBehavior::Type::kContain;

  OnOverscrollModeChange(OverscrollMode::OVERSCROLL_NONE,
                         OverscrollMode::OVERSCROLL_SOUTH,
                         OverscrollSource::TOUCHSCREEN, behavior_y_contain);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());

  OnOverscrollComplete(OverscrollMode::OVERSCROLL_SOUTH);

  EXPECT_EQ(OverscrollMode::OVERSCROLL_NONE, mode());
  EXPECT_EQ(OverscrollSource::NONE, source());
}

}  // namespace content
