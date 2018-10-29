// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include <string.h>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/aura/types.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura_extra/image_window_delegate.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {
namespace {

const char kUmaStarted[] = "Overscroll.Started3";
const char kUmaCancelled[] = "Overscroll.Cancelled3";
const char kUmaNavigated[] = "Overscroll.Navigated3";

const char kActionCancelledBack[] = "Overscroll_Cancelled.Back";
const char kActionCancelledForward[] = "Overscroll_Cancelled.Forward";
const char kActionNavigatedBack[] = "Overscroll_Navigated.Back";
const char kActionNavigatedForward[] = "Overscroll_Navigated.Forward";

}  // namespace

// Forces web contents to complete web page load as soon as navigation starts.
class ImmediateLoadObserver : WebContentsObserver {
 public:
  explicit ImmediateLoadObserver(TestWebContents* contents)
      : contents_(contents) {
    Observe(contents);
  }
  ~ImmediateLoadObserver() override {}

  void DidStartNavigationToPendingEntry(const GURL& url,
                                        ReloadType reload_type) override {
    // Simulate immediate web page load.
    contents_->TestSetIsLoading(false);
    Observe(nullptr);
  }

 private:
  TestWebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(ImmediateLoadObserver);
};

// A subclass of TestWebContents that offers a fake content window.
class OverscrollTestWebContents : public TestWebContents {
 public:
  explicit OverscrollTestWebContents(
      BrowserContext* browser_context,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window)
      : TestWebContents(browser_context),
        fake_native_view_(std::move(fake_native_view)),
        fake_contents_window_(std::move(fake_contents_window)),
        is_being_destroyed_(false) {}
  ~OverscrollTestWebContents() override {}

  static std::unique_ptr<OverscrollTestWebContents> Create(
      BrowserContext* browser_context,
      scoped_refptr<SiteInstance> instance,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window) {
    std::unique_ptr<OverscrollTestWebContents> web_contents =
        std::make_unique<OverscrollTestWebContents>(
            browser_context, std::move(fake_native_view),
            std::move(fake_contents_window));
    web_contents->Init(
        WebContents::CreateParams(browser_context, std::move(instance)));
    return web_contents;
  }

  void ResetNativeView() { fake_native_view_.reset(); }

  void ResetContentNativeView() { fake_contents_window_.reset(); }

  void set_is_being_destroyed(bool val) { is_being_destroyed_ = val; }

  gfx::NativeView GetNativeView() override { return fake_native_view_.get(); }

  gfx::NativeView GetContentNativeView() override {
    return fake_contents_window_.get();
  }

  bool IsBeingDestroyed() const override { return is_being_destroyed_; }

 private:
  std::unique_ptr<aura::Window> fake_native_view_;
  std::unique_ptr<aura::Window> fake_contents_window_;
  bool is_being_destroyed_;
};

class OverscrollNavigationOverlayTest : public RenderViewHostImplTestHarness {
 public:
  OverscrollNavigationOverlayTest()
      : first_("https://www.google.com"),
        second_("http://www.chromium.org"),
        third_("https://www.kernel.org/"),
        fourth_("https://github.com/") {}

  ~OverscrollNavigationOverlayTest() override {}

  void SetDummyScreenshotOnNavEntry(NavigationEntry* entry) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorWHITE);
    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data);
    scoped_refptr<base::RefCountedBytes> png_bytes =
        base::RefCountedBytes::TakeVector(&png_data);
    NavigationEntryImpl* entry_impl =
        NavigationEntryImpl::FromNavigationEntry(entry);
    entry_impl->SetScreenshotPNGData(png_bytes);
  }

  void ReceivePaintUpdate() {
    RenderViewHostTester::SimulateFirstPaint(test_rvh());
  }

  void PerformBackNavigationViaSliderCallbacks(OverscrollSource source) {
    // Sets slide direction to BACK, sets screenshot from NavEntry at
    // offset -1 on layer_delegate_.
    GetOverlay()->owa_->SetOverscrollSourceForTesting(source);
    std::unique_ptr<aura::Window> window(
        GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds()));
    bool window_created = !!window;
    if (window_created) {
      histogram_tester()->ExpectTotalCount(kUmaStarted, 1);
      histogram_tester()->ExpectBucketCount(
          kUmaStarted, source == OverscrollSource::TOUCHPAD ? BACK_TOUCHPAD
                                                            : BACK_TOUCHSCREEN,
          1);
      EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::BACK);
      // Performs BACK navigation, sets image from layer_delegate_ on
      // image_delegate_.
      GetOverlay()->OnOverscrollCompleting();
      window->SetBounds(gfx::Rect(root_window()->bounds().size()));

      histogram_tester()->ExpectTotalCount(kUmaNavigated, 0);
      EXPECT_EQ(0, action_tester()->GetActionCount(kActionNavigatedBack));

      GetOverlay()->OnOverscrollCompleted(std::move(window));
      histogram_tester()->ExpectTotalCount(kUmaNavigated, 1);
      histogram_tester()->ExpectBucketCount(kUmaNavigated,
                                            source == OverscrollSource::TOUCHPAD
                                                ? BACK_TOUCHPAD
                                                : BACK_TOUCHSCREEN,
                                            1);
      EXPECT_EQ(1, action_tester()->GetActionCount(kActionNavigatedBack));
    } else {
      EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::NONE);
      histogram_tester()->ExpectTotalCount(kUmaStarted, 0);
    }
    GetOverlay()->owa_->SetOverscrollSourceForTesting(OverscrollSource::NONE);
    main_test_rfh()->PrepareForCommit();
    if (window_created)
      EXPECT_TRUE(contents()->CrossProcessNavigationPending());
    else
      EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  }

  gfx::Rect GetFrontSlideWindowBounds() {
    gfx::Rect bounds = gfx::Rect(root_window()->bounds().size());
    bounds.Offset(root_window()->bounds().size().width(), 0);
    return bounds;
  }

  gfx::Rect GetBackSlideWindowBounds() {
    return gfx::Rect(root_window()->bounds().size());
  }

  // Const accessors.
  const GURL first() { return first_; }
  const GURL second() { return second_; }
  const GURL third() { return third_; }
  const GURL fourth() { return fourth_; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  base::UserActionTester* action_tester() { return action_tester_.get(); }

 protected:
  // RenderViewHostImplTestHarness:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    histogram_tester_.reset(new base::HistogramTester);
    action_tester_.reset(new base::UserActionTester);

    // Set up the fake web contents native view.
    std::unique_ptr<aura::Window> fake_native_view(new aura::Window(nullptr));
    fake_native_view->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_native_view.get());
    fake_native_view->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Set up the fake contents window.
    std::unique_ptr<aura::Window> fake_contents_window(
        new aura::Window(nullptr));
    fake_contents_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_contents_window.get());
    fake_contents_window->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Replace the default test web contents with our custom class.
    SetContents(OverscrollTestWebContents::Create(
        browser_context(), SiteInstance::Create(browser_context()),
        std::move(fake_native_view), std::move(fake_contents_window)));

    contents()->NavigateAndCommit(first());
    EXPECT_TRUE(controller().GetVisibleEntry());
    EXPECT_FALSE(controller().CanGoBack());

    contents()->NavigateAndCommit(second());
    EXPECT_TRUE(controller().CanGoBack());

    contents()->NavigateAndCommit(third());
    EXPECT_TRUE(controller().CanGoBack());

    contents()->NavigateAndCommit(fourth_);
    EXPECT_TRUE(controller().CanGoBack());
    EXPECT_FALSE(controller().CanGoForward());

    // Receive a paint update. This is necessary to make sure the size is set
    // correctly in RenderWidgetHostImpl.
    viz::LocalSurfaceId local_surface_id(10, 10,
                                         base::UnguessableToken::Create());
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(10, 10);
    metadata.local_surface_id = local_surface_id;
    test_rvh()->GetWidget()->DidUpdateVisualProperties(metadata);

    // Reset pending flags for size/paint.
    test_rvh()->GetWidget()->ResetSentVisualProperties();

    // Create the overlay, and set the contents of the overlay window.
    overlay_.reset(new OverscrollNavigationOverlay(contents(), root_window()));
  }

  void TearDown() override {
    overlay_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  OverscrollNavigationOverlay* GetOverlay() {
    return overlay_.get();
  }

 private:
  // Tests URLs.
  const GURL first_;
  const GURL second_;
  const GURL third_;
  const GURL fourth_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<base::UserActionTester> action_tester_;

  std::unique_ptr<OverscrollNavigationOverlay> overlay_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlayTest);
};

// Tests that if a screenshot is available, it is set in the overlay window
// delegate.
TEST_F(OverscrollNavigationOverlayTest, WithScreenshot) {
  SetDummyScreenshotOnNavEntry(controller().GetEntryAtOffset(-1));
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);
  // Screenshot was set on NavEntry at offset -1.
  EXPECT_TRUE(static_cast<aura_extra::ImageWindowDelegate*>(
                  GetOverlay()->window_->delegate())->has_image());
}

// Tests that if a screenshot is not available, no image is set in the overlay
// window delegate.
TEST_F(OverscrollNavigationOverlayTest, WithoutScreenshot) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHSCREEN);
  // No screenshot was set on NavEntry at offset -1.
  EXPECT_FALSE(static_cast<aura_extra::ImageWindowDelegate*>(
                   GetOverlay()->window_->delegate())->has_image());
}

// Tests that if a navigation is attempted but there is nothing to navigate to,
// we return a null window.
TEST_F(OverscrollNavigationOverlayTest, CannotNavigate) {
  EXPECT_EQ(GetOverlay()->CreateFrontWindow(GetFrontSlideWindowBounds()),
            nullptr);
}

// Tests that if a navigation is cancelled, no navigation is performed and the
// state is restored.
TEST_F(OverscrollNavigationOverlayTest, CancelNavigation) {
  GetOverlay()->owa_->SetOverscrollSourceForTesting(
      OverscrollSource::TOUCHSCREEN);
  std::unique_ptr<aura::Window> window =
      GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::BACK);

  histogram_tester()->ExpectTotalCount(kUmaCancelled, 0);
  EXPECT_EQ(0, action_tester()->GetActionCount(kActionCancelledBack));

  GetOverlay()->OnOverscrollCancelled();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::NONE);
  histogram_tester()->ExpectTotalCount(kUmaCancelled, 1);
  histogram_tester()->ExpectBucketCount(kUmaCancelled, BACK_TOUCHSCREEN, 1);
  EXPECT_EQ(1, action_tester()->GetActionCount(kActionCancelledBack));
}

TEST_F(OverscrollNavigationOverlayTest, ForwardNavigation) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);

  GetOverlay()->owa_->SetOverscrollSourceForTesting(OverscrollSource::TOUCHPAD);
  std::unique_ptr<aura::Window> window =
      GetOverlay()->CreateFrontWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::FORWARD);
  histogram_tester()->ExpectTotalCount(kUmaStarted, 2);
  histogram_tester()->ExpectBucketCount(kUmaStarted, FORWARD_TOUCHPAD, 1);

  GetOverlay()->OnOverscrollCompleting();
  histogram_tester()->ExpectTotalCount(kUmaNavigated, 1);
  EXPECT_EQ(0, action_tester()->GetActionCount(kActionNavigatedForward));

  GetOverlay()->OnOverscrollCompleted(std::move(window));
  histogram_tester()->ExpectTotalCount(kUmaNavigated, 2);
  histogram_tester()->ExpectBucketCount(kUmaNavigated, FORWARD_TOUCHPAD, 1);
  EXPECT_EQ(1, action_tester()->GetActionCount(kActionNavigatedForward));

  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
}

TEST_F(OverscrollNavigationOverlayTest, ForwardNavigationCancelled) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);

  GetOverlay()->owa_->SetOverscrollSourceForTesting(
      OverscrollSource::TOUCHSCREEN);
  std::unique_ptr<aura::Window> window =
      GetOverlay()->CreateFrontWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::FORWARD);
  histogram_tester()->ExpectTotalCount(kUmaStarted, 2);
  histogram_tester()->ExpectBucketCount(kUmaStarted, FORWARD_TOUCHSCREEN, 1);

  histogram_tester()->ExpectTotalCount(kUmaCancelled, 0);
  EXPECT_EQ(0, action_tester()->GetActionCount(kActionCancelledForward));

  GetOverlay()->OnOverscrollCancelled();
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::NONE);
  histogram_tester()->ExpectTotalCount(kUmaCancelled, 1);
  histogram_tester()->ExpectBucketCount(kUmaCancelled, FORWARD_TOUCHSCREEN, 1);
  EXPECT_EQ(1, action_tester()->GetActionCount(kActionCancelledForward));
}

// Performs two navigations. The second navigation is cancelled, tests that the
// first one worked correctly.
TEST_F(OverscrollNavigationOverlayTest, CancelAfterSuccessfulNavigation) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);
  GetOverlay()->owa_->SetOverscrollSourceForTesting(OverscrollSource::TOUCHPAD);
  std::unique_ptr<aura::Window> wrapper =
      GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::BACK);
  histogram_tester()->ExpectTotalCount(kUmaStarted, 2);
  histogram_tester()->ExpectBucketCount(kUmaStarted, BACK_TOUCHPAD, 2);

  GetOverlay()->OnOverscrollCancelled();
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::NONE);
  histogram_tester()->ExpectTotalCount(kUmaCancelled, 1);
  histogram_tester()->ExpectBucketCount(kUmaCancelled, BACK_TOUCHPAD, 1);
  EXPECT_EQ(1, action_tester()->GetActionCount(kActionCancelledBack));
  // Navigation metrics shouldn't change.
  histogram_tester()->ExpectTotalCount(kUmaNavigated, 1);
  EXPECT_EQ(1, action_tester()->GetActionCount(kActionNavigatedBack));

  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      pending->GetUniqueID(), false, pending->GetURL(),
      pending->GetTransitionType());
  EXPECT_EQ(contents()->GetURL(), third());
}

// Tests that an overscroll navigation that receives a paint update actually
// stops observing.
TEST_F(OverscrollNavigationOverlayTest, Navigation_PaintUpdate) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHSCREEN);
  ReceivePaintUpdate();

  // Paint updates until the navigation is committed typically represent updates
  // for the previous page, so we should still be observing.
  EXPECT_TRUE(GetOverlay()->web_contents());

  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      pending->GetUniqueID(), false, pending->GetURL(),
      pending->GetTransitionType());
  ReceivePaintUpdate();

  // Navigation was committed and the paint update was received - we should no
  // longer be observing.
  EXPECT_FALSE(GetOverlay()->web_contents());
  EXPECT_EQ(contents()->GetURL(), third());
}

// Tests that an overscroll navigation that receives a loading update actually
// stops observing.
TEST_F(OverscrollNavigationOverlayTest, Navigation_LoadingUpdate) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);
  EXPECT_TRUE(GetOverlay()->web_contents());
  // DidStopLoading for any navigation should always reset the load flag and
  // dismiss the overlay even if the pending navigation wasn't committed -
  // this is a "safety net" in case we mis-identify the destination webpage
  // (which can happen if a new navigation is performed while while a GestureNav
  // navigation is in progress).
  contents()->TestSetIsLoading(false);
  EXPECT_FALSE(GetOverlay()->web_contents());
  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigate(
      pending->GetUniqueID(), false, pending->GetURL());
  EXPECT_EQ(contents()->GetURL(), third());
}

TEST_F(OverscrollNavigationOverlayTest, CloseDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode normal_duration_(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetOverlay()->owa_->OnOverscrollModeChange(OVERSCROLL_NONE, OVERSCROLL_EAST,
                                             OverscrollSource::TOUCHSCREEN,
                                             cc::OverscrollBehavior());
  GetOverlay()->owa_->OnOverscrollComplete(OVERSCROLL_EAST);
  EXPECT_EQ(GetOverlay()->direction_, NavigationDirection::BACK);
  OverscrollTestWebContents* test_web_contents =
      static_cast<OverscrollTestWebContents*>(web_contents());
  test_web_contents->set_is_being_destroyed(true);
  test_web_contents->ResetContentNativeView();
  test_web_contents->ResetNativeView();
  // Ensure a clean close.
}

// Tests that we can handle the case when the load completes as soon as the
// navigation is started.
TEST_F(OverscrollNavigationOverlayTest, ImmediateLoadOnNavigate) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);
  // This observer will force the page load to complete as soon as the
  // navigation starts.
  ImmediateLoadObserver immediate_nav(contents());
  GetOverlay()->owa_->OnOverscrollModeChange(OVERSCROLL_NONE, OVERSCROLL_EAST,
                                             OverscrollSource::TOUCHPAD,
                                             cc::OverscrollBehavior());
  // This will start and immediately complete the navigation.
  GetOverlay()->owa_->OnOverscrollComplete(OVERSCROLL_EAST);
  EXPECT_FALSE(GetOverlay()->window_.get());
  histogram_tester()->ExpectTotalCount(kUmaNavigated, 2);
  histogram_tester()->ExpectBucketCount(kUmaNavigated, BACK_TOUCHPAD, 2);
  EXPECT_EQ(2, action_tester()->GetActionCount(kActionNavigatedBack));
}

// Tests that swapping the overlay window at the end of a gesture caused by the
// start of a new overscroll does not crash and the events still reach the new
// overlay window.
TEST_F(OverscrollNavigationOverlayTest, OverlayWindowSwap) {
  PerformBackNavigationViaSliderCallbacks(OverscrollSource::TOUCHPAD);
  aura::Window* first_overlay_window = GetOverlay()->window_.get();
  EXPECT_TRUE(GetOverlay()->web_contents());
  EXPECT_TRUE(first_overlay_window);

  // At this stage, the overlay window is covering the web contents. Configure
  // the animator of the overlay window for the test.
  ui::ScopedAnimationDurationScaleMode normal_duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimator* animator = GetOverlay()->window_->layer()->GetAnimator();
  animator->set_disable_timer_for_test(true);
  ui::LayerAnimatorTestController test_controller(animator);

  int overscroll_complete_distance =
      root_window()->bounds().size().width() *
          OverscrollConfig::GetThreshold(
              OverscrollConfig::Threshold::kCompleteTouchscreen) +
      ui::GestureConfiguration::GetInstance()
          ->max_touch_move_in_pixels_for_click() +
      1;

  // Start and complete a back navigation via a gesture.
  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(0, 0),
                                  gfx::Point(overscroll_complete_distance, 0),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);

  ui::ScopedLayerAnimationSettings settings(animator);
  test_controller.StartThreadedAnimationsIfNeeded();

  // The overlay window should now be being animated to the edge of the screen.
  // |first()overlay_window| is the back window.
  // This is what the screen should look like. The X indicates where the next
  // gesture starts for the test.
  // +---------root_window--------+
  // |+-back window--+--front window--+
  // ||              |            |   |
  // ||        1     |X       2   |   |
  // ||              |            |   |
  // |+--------------+------------|---+
  // +----------------------------+
  // |  overscroll   ||
  // |   complete    ||
  // |   distance    ||
  // |<------------->||
  // |     second     |
  // |   overscroll   |
  // | start distance |
  // |<-------------->|
  EXPECT_EQ(GetOverlay()->window_.get(), first_overlay_window);

  // The overlay window is halfway through, start another animation that will
  // cancel the first one. The event that cancels the animation will go to
  // the slide window, which will be used as the overlay window when the new
  // overscroll starts.
  int second_overscroll_start_distance = overscroll_complete_distance + 1;
  generator.GestureScrollSequence(
      gfx::Point(second_overscroll_start_distance, 0),
      gfx::Point(
          second_overscroll_start_distance + overscroll_complete_distance, 0),
      base::TimeDelta::FromMilliseconds(10), 10);
  EXPECT_TRUE(GetOverlay()->window_.get());
  // The overlay window should be a new window.
  EXPECT_NE(GetOverlay()->window_.get(), first_overlay_window);

  // Complete the animation.
  GetOverlay()->window_->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(GetOverlay()->window_.get());

  // Load the page.
  contents()->CommitPendingNavigation();
  ReceivePaintUpdate();
  EXPECT_FALSE(GetOverlay()->window_.get());
  EXPECT_EQ(contents()->GetURL(), first());
}

}  // namespace content
