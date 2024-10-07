// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include <sstream>
#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "base/numerics/ranges.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_impl.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "cc/test/pixel_test_utils.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/navigation_transitions/physics_model.h"
#include "content/browser/navigation_transitions/progress_bar.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/navigation_transition_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "ui/android/fake_modal_dialog_manager_bridge.h"
#include "ui/android/progress_bar_config.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/switches.h"
#include "ui/snapshot/snapshot.h"

namespace content {

namespace {

using SwipeEdge = ui::BackGestureEventSwipeEdge;
using AnimationStage = BackForwardTransitionAnimationManager::AnimationStage;
using NavType = BackForwardTransitionAnimationManager::NavigationDirection;
using base::test::TestFuture;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Combine;
using testing::TestParamInfo;
using testing::Values;
using testing::WithParamInterface;

using AnimatorState = BackForwardTransitionAnimator::State;
using enum BackForwardTransitionAnimator::State;

// The tolerance for two float to be considered equal.
static constexpr float kFloatTolerance = 0.001f;

#define EXPECT_X_TRANSLATION(expected, actual)                    \
  EXPECT_TRANSFORM_NEAR(ViewportTranslationX(expected), (actual), \
                        kFloatTolerance)

#define EXPECT_STATE_EQ(expected, actual)                                      \
  EXPECT_EQ(expected, actual)                                                  \
      << "Expected: "                                                          \
      << BackForwardTransitionAnimator::StateToString(expected) << " but got " \
      << BackForwardTransitionAnimator::StateToString(actual);

// TODO(liuwilliam): 99 seconds seems arbitrary. Pick a meaningful constant
// instead.
// If the duration is long enough, the spring will return the final (rest /
// equilibrium) position right away. This means each spring model will just
// produce one frame: the frame for the final position.
constexpr base::TimeDelta kLongDurationBetweenFrames = base::Seconds(99);

// Test parameter to run tests either with default device-scale-factor==1 or a
// fractional (1.333) device scale factor.
enum class DSFMode { kOne, kFractional };

// Test parameter to run tests with UI laid out both left to right and right
// to left, the latter forces the back-edge to be flipped (i.e. right edge
// uses an animated gesture).
enum class UILayoutDirection { kLTR, kRTL };

static constexpr gfx::Transform kIdentityTransform;

AssertionResult ColorsNear(const SkColor4f& e, const SkColor4f& a) {
  if (base::IsApproximatelyEqual(e.fA, a.fA, kFloatTolerance) &&
      base::IsApproximatelyEqual(e.fB, a.fB, kFloatTolerance) &&
      base::IsApproximatelyEqual(e.fG, a.fG, kFloatTolerance) &&
      base::IsApproximatelyEqual(e.fR, a.fR, kFloatTolerance)) {
    return AssertionSuccess();
  }

  return AssertionFailure()
         << "Expected color [" << e.fR << "," << e.fG << "," << e.fB << ","
         << e.fA << "] but got actual [" << a.fR << "," << a.fG << "," << a.fB
         << "," << a.fA << "].";
}

// For light mode.
static constexpr SkColor4f kScrimColorAtStart = {0, 0, 0, 0.1f};
static constexpr SkColor4f kScrimColorAt30 = {0, 0, 0, 0.0745f};
static constexpr SkColor4f kScrimColorAt60 = {0, 0, 0, 0.049f};
static constexpr SkColor4f kScrimColorAt90 = {0, 0, 0, 0.0235f};

int64_t GetItemSequenceNumberForNavigation(
    NavigationHandle* navigation_handle) {
  auto* request = static_cast<NavigationRequest*>(navigation_handle);
  EXPECT_TRUE(request->GetNavigationEntry());
  EXPECT_TRUE(request->GetRenderFrameHost());
  return static_cast<NavigationEntryImpl*>(request->GetNavigationEntry())
      ->GetFrameEntry(request->GetRenderFrameHost()->frame_tree_node())
      ->item_sequence_number();
}

class AnimatorForTesting : public BackForwardTransitionAnimator {
 public:
  explicit AnimatorForTesting(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_type,
      ui::BackGestureEventSwipeEdge initiating_edge,
      NavigationEntryImpl* destination_entry,
      const SkBitmap& embedder_bitmap,
      BackForwardTransitionAnimationManagerAndroid* animation_manager)
      : BackForwardTransitionAnimator(web_contents_view_android,
                                      controller,
                                      gesture,
                                      nav_type,
                                      initiating_edge,
                                      destination_entry,
                                      embedder_bitmap,
                                      animation_manager),
        wcva_(web_contents_view_android) {}

  ~AnimatorForTesting() override {
    if (on_impl_destroyed_) {
      std::move(on_impl_destroyed_).Run(state());
    }
  }

  // `BackForwardTransitionAnimator`:
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override {
    if (intercept_render_frame_metadata_changed_) {
      return;
    }
    if (state() == State::kWaitingForNewRendererToDraw &&
        waited_for_renderer_new_frame_) {
      std::move(waited_for_renderer_new_frame_).Run();
    }

    BackForwardTransitionAnimator::OnRenderFrameMetadataChangedAfterActivation(
        activation_time);
  }
  // TODO(bokan): Caution: this override ignores the passed in frame_begin_time
  // and instead sets the time by incrementing by `duration_between_frames_`.
  // It would be clearer if tests simulated an animation frame from the test
  // body with a supplied frame time. See DirectlyCallOnAnimate below this one.
  void OnAnimate(base::TimeTicks frame_begin_time) override {
    if (pause_on_animate_at_state_.has_value() &&
        pause_on_animate_at_state_.value() == state()) {
      return;
    }
    if (next_on_animate_callback_) {
      std::move(next_on_animate_callback_).Run();
    }
    static base::TimeTicks tick = base::TimeTicks();
    tick += duration_between_frames_;
    BackForwardTransitionAnimator::OnAnimate(tick);
  }
  void DirectlyCallOnAnimate(base::TimeTicks frame_time) {
    BackForwardTransitionAnimator::OnAnimate(frame_time);
  }
  void OnCancelAnimationDisplayed() override {
    if (on_cancel_animation_displayed_) {
      std::move(on_cancel_animation_displayed_).Run();
    }
    BackForwardTransitionAnimator::OnCancelAnimationDisplayed();
  }
  void OnInvokeAnimationDisplayed() override {
    if (on_invoke_animation_displayed_) {
      std::move(on_invoke_animation_displayed_).Run();
    }
    BackForwardTransitionAnimator::OnInvokeAnimationDisplayed();
  }
  void OnCrossFadeAnimationDisplayed() override {
    if (on_cross_fade_animation_displayed_) {
      std::move(on_cross_fade_animation_displayed_).Run();
    }

    BackForwardTransitionAnimator::OnCrossFadeAnimationDisplayed();
  }
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    last_navigation_request_ =
        NavigationRequest::From(navigation_handle)->GetWeakPtr();
    BackForwardTransitionAnimator::DidStartNavigation(navigation_handle);
  }
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    BackForwardTransitionAnimator::ReadyToCommitNavigation(navigation_handle);
    if (post_ready_to_commit_callback_) {
      std::move(post_ready_to_commit_callback_).Run();
    }
  }
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (did_finish_navigation_callback_) {
      auto* request = static_cast<NavigationRequest*>(navigation_handle);
      std::move(did_finish_navigation_callback_)
          .Run(request->GetRenderFrameHost()
                   ->GetView()
                   ->host()
                   ->IsContentRenderingTimeoutRunning());
    }
    BackForwardTransitionAnimator::DidFinishNavigation(navigation_handle);
  }

  NavigationRequest* LastNavigationRequest() {
    CHECK(last_navigation_request_);
    return last_navigation_request_.get();
  }
  void PauseAnimationAtDisplayingCancelAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingCancelAnimation;
  }

  void PauseAnimationAtDisplayingInvokeAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingInvokeAnimation;
  }

  void PauseAnimationAtDisplayingCrossFadeAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingCrossFadeAnimation;
  }

  void UnpauseAnimation() {
    pause_on_animate_at_state_ = std::nullopt;
    OnAnimate(base::TimeTicks{});
  }

  void set_intercept_render_frame_metadata_changed(bool intercept) {
    intercept_render_frame_metadata_changed_ = intercept;
  }
  void set_on_cancel_animation_displayed(base::OnceClosure callback) {
    CHECK(!on_cancel_animation_displayed_);
    on_cancel_animation_displayed_ = std::move(callback);
  }
  void set_on_invoke_animation_displayed(base::OnceClosure callback) {
    CHECK(!on_invoke_animation_displayed_);
    on_invoke_animation_displayed_ = std::move(callback);
  }
  void set_on_cross_fade_animation_displayed(base::OnceClosure callback) {
    CHECK(!on_cross_fade_animation_displayed_);
    on_cross_fade_animation_displayed_ = std::move(callback);
  }
  void set_waited_for_renderer_new_frame(base::OnceClosure callback) {
    CHECK(!waited_for_renderer_new_frame_);
    waited_for_renderer_new_frame_ = std::move(callback);
  }
  void set_next_on_animate_callback(base::OnceClosure callback) {
    CHECK(!next_on_animate_callback_);
    next_on_animate_callback_ = std::move(callback);
  }
  void set_post_ready_to_commit_callback(base::OnceClosure callback) {
    CHECK(!post_ready_to_commit_callback_);
    post_ready_to_commit_callback_ = std::move(callback);
  }
  void set_did_finish_navigation_callback(
      base::OnceCallback<void(bool)> callback) {
    CHECK(!did_finish_navigation_callback_);
    did_finish_navigation_callback_ = std::move(callback);
  }
  void set_on_impl_destroyed(base::OnceCallback<void(State)> callback) {
    CHECK(!on_impl_destroyed_);
    on_impl_destroyed_ = std::move(callback);
  }
  void set_duration_between_frames(base::TimeDelta duration) {
    duration_between_frames_ = duration;
  }
  void set_subframe_navigation(bool subframe_navigation) {
    subframe_navigation_ = subframe_navigation;
  }

  State state() const { return state_; }

  int64_t primary_main_frame_navigation_entry_item_sequence_number() const {
    return primary_main_frame_navigation_entry_item_sequence_number_;
  }

 private:
  const raw_ptr<WebContentsViewAndroid> wcva_;

  base::TimeDelta duration_between_frames_ = kLongDurationBetweenFrames;

  bool intercept_render_frame_metadata_changed_ = false;

  bool subframe_navigation_ = false;

  std::optional<State> pause_on_animate_at_state_;

  base::WeakPtr<NavigationRequest> last_navigation_request_;

  base::OnceClosure on_cancel_animation_displayed_;
  base::OnceClosure on_invoke_animation_displayed_;
  base::OnceClosure on_cross_fade_animation_displayed_;
  base::OnceClosure waited_for_renderer_new_frame_;
  base::OnceClosure next_on_animate_callback_;
  base::OnceClosure post_ready_to_commit_callback_;
  // The return value indicates if the paint-holding timer is running on the new
  // RenderWidgetHost when the animated history navigation commits.
  base::OnceCallback<void(bool)> did_finish_navigation_callback_;
  base::OnceCallback<void(State)> on_impl_destroyed_;
};

class FactoryForTesting : public BackForwardTransitionAnimator::Factory {
 public:
  explicit FactoryForTesting(const SkBitmap& override_bitmap)
      : override_bitmap_(override_bitmap) {}
  ~FactoryForTesting() override = default;

  std::unique_ptr<BackForwardTransitionAnimator> Create(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_type,
      ui::BackGestureEventSwipeEdge initiating_edge,
      NavigationEntryImpl* destination_entry,
      SkBitmap embedder_content,
      BackForwardTransitionAnimationManagerAndroid* animation_manager)
      override {
    return std::make_unique<AnimatorForTesting>(
        web_contents_view_android, controller, gesture, nav_type,
        initiating_edge, destination_entry,
        override_bitmap_.empty() ? embedder_content : override_bitmap_,
        animation_manager);
  }

 private:
  SkBitmap override_bitmap_;
};
}  // namespace

// TODO(https://crbug.com/325329998): Enable the pixel comparison so the tests
// are truly end-to-end.
class BackForwardTransitionAnimationManagerBrowserTest
    : public ContentBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}},
        {blink::features::kIncrementLocalSurfaceIdForMainframeSameDocNavigation,
         {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBrowserTest() override = default;

  void SetUp() override {
    if (base::SysInfo::GetAndroidHardwareEGL() == "emulation") {
      // crbug.com/337886037 and crrev.com/c/5504854/comment/b81b8fb6_95fb1381/:
      // The CopyOutputRequests crash the GPU process. ANGLE is exporting the
      // native fence support on Android emulators but it doesn't work properly.
      GTEST_SKIP();
    }
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForcePrefersNoReducedMotion);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(
        base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions));

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Manually load a "red" document because we are still at the initial
    // entry.
    ASSERT_TRUE(NavigateToURL(web_contents(), RedURL()));
    WaitForCopyableViewInWebContents(web_contents());

    auto* manager =
        BrowserContextImpl::From(web_contents()->GetBrowserContext())
            ->GetNavigationEntryScreenshotManager();
    ASSERT_TRUE(manager);
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());
    // 10 Screenshots, with 4 bytes per screenshot.
    manager->SetMemoryBudgetForTesting(4 * GetViewportSize().Area64() * 10);

    // Set up for a backward navigation: [red&, green*].
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), GreenURL()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());

    GetAnimationManager()->set_animator_factory_for_testing(
        std::make_unique<FactoryForTesting>(EmbedderBitmap()));
  }

  virtual SkBitmap EmbedderBitmap() { return SkBitmap(); }

  gfx::Size GetViewportSize() {
    return web_contents()->GetNativeView()->GetPhysicalBackingSize();
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  BackForwardTransitionAnimationManagerAndroid* GetAnimationManager() {
    return static_cast<BackForwardTransitionAnimationManagerAndroid*>(
        web_contents()->GetBackForwardTransitionAnimationManager());
  }

  GURL RedURL() const { return embedded_test_server()->GetURL("/red.html"); }

  GURL GreenURL() const {
    return embedded_test_server()->GetURL("/green.html");
  }

  GURL BlueURL() const { return embedded_test_server()->GetURL("/blue.html"); }

  gfx::Transform ViewportTranslationX(float translation_x) {
    return gfx::Transform::MakeTranslation(
        translation_x * GetViewportSize().width(), 0.f);
  }

  cc::slim::Layer* GetViewLayer() {
    return static_cast<WebContentsViewAndroid*>(web_contents()->GetView())
        ->GetNativeView()
        ->GetLayer();
  }

  cc::slim::Layer* GetScreenshotLayer() {
    if (!GetAnimator()) {
      return nullptr;
    }
    return GetAnimator()->screenshot_layer_for_testing();
  }

  cc::slim::Layer* GetScrimLayer() {
    if (!GetAnimator()) {
      return nullptr;
    }
    return GetAnimator()->scrim_layer_for_testing();
  }

  cc::slim::Layer* GetCloneLayer() {
    if (!GetAnimator()) {
      return nullptr;
    }
    return GetAnimator()->clone_layer_for_testing();
  }

  cc::slim::Layer* GetEmbedderLayer() {
    if (!GetAnimator()) {
      return nullptr;
    }
    return GetAnimator()->embedder_live_content_clone_for_testing();
  }

  cc::slim::Layer* GetLivePageLayer() {
    return GetAnimationManager()
        ->web_contents_view_android()
        ->parent_for_web_page_widgets();
  }

  const cc::slim::Layer* GetProgressBarLayer() {
    if (!GetAnimator() || !GetAnimator()->progress_bar_for_testing()) {
      return nullptr;
    }
    return GetAnimator()->progress_bar_for_testing()->GetLayer().get();
  }

  const cc::slim::Layer* GetRRectLayer() {
    if (!GetAnimator()) {
      return nullptr;
    }
    return GetAnimator()->rrect_layer_for_testing();
  }

  const cc::slim::Layer* GetFaviconLayer() {
    if (!GetRRectLayer()) {
      return nullptr;
    }
    EXPECT_EQ(GetRRectLayer()->children().size(), 1u);
    return GetRRectLayer()->children().at(0).get();
  }

  // Prints known children of the given layer, in increasing z-order.
  std::string ChildrenInOrder(const cc::slim::Layer& layer) {
    std::stringstream output;
    output << "[";

    bool list_non_empty = false;

    for (const auto& child : layer.children()) {
      std::string layer_name;
      if (child.get() == GetLivePageLayer()) {
        layer_name = "LivePage";
      } else if (child.get() == GetScreenshotLayer()) {
        layer_name = "Screenshot";
      } else if (child.get() == GetScrimLayer()) {
        layer_name = "Scrim";
      } else if (child.get() == GetCloneLayer()) {
        layer_name = "OldSurfaceClone";
      } else if (child.get() == GetProgressBarLayer()) {
        layer_name = "ProgressBar";
      } else if (child.get() == GetEmbedderLayer()) {
        layer_name = "EmbedderContentLayer";
      } else if (child.get() == GetRRectLayer()) {
        layer_name = "RRect";
      } else if (child.get() == GetFaviconLayer()) {
        layer_name = "Favicon";
      }

      if (!layer_name.empty()) {
        if (list_non_empty) {
          output << ",";
        }
        list_non_empty = true;

        output << layer_name;
        if (child.get() == GetScreenshotLayer() ||
            child.get() == GetRRectLayer()) {
          output << ChildrenInOrder(*child.get());
        }
      }
    }

    output << "]";
    return output.str();
  }

  // This will wait (or return immediately) for the first frame to be submitted
  // after navigating the primary main frame such that the cross fade can begin.
  // Must be called after the navigation has been committed and a the new
  // RenderFrameHost is swapped in.
  [[nodiscard]] AssertionResult WaitForFirstFrameInPrimaryMainFrame() {
    CHECK(GetAnimator());

    int64_t sequence_num =
        GetAnimator()
            ->primary_main_frame_navigation_entry_item_sequence_number();
    if (sequence_num == cc::RenderFrameMetadata::kInvalidItemSequenceNumber) {
      return AssertionFailure() << "Animator not waiting for a new frame.";
    }

    RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
    RenderWidgetHostImpl* new_widget_host = rfh->GetRenderWidgetHost();
    while (new_widget_host->render_frame_metadata_provider()
               ->LastRenderFrameMetadata()
               .primary_main_frame_item_sequence_number != sequence_num) {
      RenderFrameSubmissionObserver frame_observer(rfh);
      frame_observer.WaitForAnyFrameSubmission();
    }

    return AssertionSuccess();
  }

  AnimatorForTesting* GetAnimator() {
    return static_cast<AnimatorForTesting*>(
        GetAnimationManager()->animator_.get());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Basic tests which will be run both with a swipe from the left edge as well as
// a swipe from the right edge with an RTL UI direction. Tests from the right
// edge also force the UI to use an RTL direction.
class BackForwardTransitionAnimationManagerBothEdgeBrowserTest
    : public BackForwardTransitionAnimationManagerBrowserTest,
      public WithParamInterface<std::tuple<UILayoutDirection, DSFMode>> {
 public:
  BackForwardTransitionAnimationManagerBothEdgeBrowserTest() {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}},
        {ui::kMirrorBackForwardGesturesInRTL, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBothEdgeBrowserTest() override =
      default;

  void SetUp() override {
    if (GetUILayoutDirection() == UILayoutDirection::kRTL) {
      l10n_util::SetRtlForTesting(true);
    }

    BackForwardTransitionAnimationManagerBrowserTest::SetUp();

    if (GetDSFMode() == DSFMode::kFractional) {
      EnablePixelOutput(/*force_device_scale_factor=*/1.333f);
    }
  }

  UILayoutDirection GetUILayoutDirection() const {
    return std::get<0>(GetParam());
  }
  DSFMode GetDSFMode() const { return std::get<1>(GetParam()); }

  SwipeEdge BackEdge() const {
    if (GetUILayoutDirection() == UILayoutDirection::kRTL) {
      return SwipeEdge::RIGHT;
    }

    return SwipeEdge::LEFT;
  }

  SwipeEdge ForwardEdge() const {
    if (GetUILayoutDirection() == UILayoutDirection::kRTL) {
      return SwipeEdge::LEFT;
    }

    return SwipeEdge::RIGHT;
  }
};

// Simulates the gesture sequence: start, 30%, 60%, 90%, 60%, 30%, 60%, 90% and
// finally invoke.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       BasicInvokeBack) {
  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);
  ASSERT_TRUE(GetAnimator());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  TestFrameNavigationObserver back_to_red(web_contents());
  {
    TestFuture<void> did_cross_fade;
    TestFuture<void> did_invoke;
    TestFuture<AnimatorState> destroyed;

    GetAnimator()->set_on_cross_fade_animation_displayed(
        did_cross_fade.GetCallback());
    GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

    GetAnimationManager()->OnGestureInvoked();

    ASSERT_TRUE(destroyed.Wait());
    EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());
    EXPECT_TRUE(did_invoke.IsReady());
    EXPECT_TRUE(did_cross_fade.IsReady());
  }
  back_to_red.Wait();

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Simulates the gesture sequence: start, 30%, 60%, 90%, 60%, 30% and finally
// cancels.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       BasicCancelBack) {
  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);
  ASSERT_TRUE(GetAnimator());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  {
    TestFuture<AnimatorState> destroyed;
    TestFuture<void> did_cancel;
    GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

    GetAnimationManager()->OnGestureCancelled();

    ASSERT_TRUE(destroyed.Wait());
    EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());
    EXPECT_TRUE(did_cancel.IsReady());
  }

  ASSERT_EQ(web_contents()->GetController().GetActiveEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_TRUE(web_contents()->GetController().GetEntryAtIndex(0)->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Tests the translation applied to the screenshot and fade applied to the scrim
// as the gesture is progressed in both directions in a back navigation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestScreenshotTransformAndScrimColorBackNavigation) {
  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);

  // The gesture should have created and attached a screenshot layer with a
  // child scrim layer, under the live page.
  ASSERT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  // In a back navigation, the screenshot starts off-screen in the direction the
  // swipe is coming from and moves to the viewport origin. Therefore we expect
  // it to be at `(1-progress) * initial_position` at all times.
  float initial_position = BackEdge() == SwipeEdge::LEFT
                               ? PhysicsModel::kScreenshotInitialPositionRatio
                               : -PhysicsModel::kScreenshotInitialPositionRatio;

  EXPECT_TRUE(
      ColorsNear(kScrimColorAtStart, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position, GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(ColorsNear(kScrimColorAt30, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.7,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.4,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_TRUE(ColorsNear(kScrimColorAt90, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.1,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.4,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(ColorsNear(kScrimColorAt30, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.7,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.4,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_TRUE(ColorsNear(kScrimColorAt90, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(initial_position * 0.1,
                       GetScreenshotLayer()->transform());
}

// Tests the translation of the screenshot and the fade of the scrim as the
// gesture is progressed in both directions in a forward navigation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestScreenshotTransformScrimColorForwardNavigation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // The test starts off with the session history: [red&, green*]. Add an entry
  // and navigate back so we have entries in both directions:
  // [red&, green*, blue&].
  {
    ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
    WaitForCopyableViewInWebContents(web_contents());

    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
  }

  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          ForwardEdge(), NavType::kForward);

  // In a forward navigation, the screenshot starts off-screen at the viewport
  // edge where the gesture is initiated. It in the direction of the swipe over
  // a distance defined by the commit pending ratio.
  float start_position = ForwardEdge() == SwipeEdge::RIGHT ? 1 : -1;
  float total_distance = ForwardEdge() == SwipeEdge::RIGHT
                             ? -PhysicsModel::kTargetCommitPendingRatio
                             : PhysicsModel::kTargetCommitPendingRatio;

  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(ColorsNear(kScrimColorAt30, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.3,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.6,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_TRUE(ColorsNear(kScrimColorAt90, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.9,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.6,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(ColorsNear(kScrimColorAt30, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.3,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(ColorsNear(kScrimColorAt60, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.6,
                       GetScreenshotLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_TRUE(ColorsNear(kScrimColorAt90, GetScrimLayer()->background_color()));
  EXPECT_X_TRANSLATION(start_position + total_distance * 0.9,
                       GetScreenshotLayer()->transform());
}

IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       DarkModeScrim) {
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  blink::web_pref::WebPreferences prefs =
      web_contents()->GetOrCreateWebPreferences();
  prefs.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  web_contents()->SetWebPreferences(prefs);

  // Dark mode has twice the scrim from the light mode.
  const SkColor4f kDMScrimColorAtStart = {0, 0, 0, kScrimColorAtStart.fA * 2};
  const SkColor4f kDMScrimColorAt30 = {0, 0, 0, kScrimColorAt30.fA * 2};
  const SkColor4f kDMScrimColorAt60 = {0, 0, 0, kScrimColorAt60.fA * 2};
  const SkColor4f kDMScrimColorAt90 = {0, 0, 0, kScrimColorAt90.fA * 2};

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);
  ASSERT_TRUE(GetScrimLayer());
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAtStart, GetScrimLayer()->background_color()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAt30, GetScrimLayer()->background_color()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAt60, GetScrimLayer()->background_color()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAt90, GetScrimLayer()->background_color()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAt30, GetScrimLayer()->background_color()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_TRUE(
      ColorsNear(kDMScrimColorAt60, GetScrimLayer()->background_color()));
}

// Tests the translation of the live page as the gesture is progressed in both
// directions in a back navigation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLivePageTransformBackNavigation) {
  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);

  // In a back navigation, the live page starts off at the viewport origin and
  // moves in the direction of the swipe to a maximum defined by the "commit
  // pending ratio" of the viewport width.
  float final_position = BackEdge() == SwipeEdge::LEFT
                             ? PhysicsModel::kTargetCommitPendingRatio
                             : -PhysicsModel::kTargetCommitPendingRatio;

  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_X_TRANSLATION(final_position * 0.3, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_X_TRANSLATION(final_position * 0.9, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_X_TRANSLATION(final_position * 0.3, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_X_TRANSLATION(final_position * 0.9, GetLivePageLayer()->transform());
}

// Tests the translation of the live page as the gesture is progressed in both
// directions in a back navigation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLivePageTransformForwardNavigation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // The test starts off with the session history: [red&, green*]. Add an entry
  // and navigate back so we have entries in both directions:
  // [red&, green*, blue&].
  {
    ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
    WaitForCopyableViewInWebContents(web_contents());

    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
  }

  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          ForwardEdge(), NavType::kForward);

  // In a forward navigation, the live page starts off at the viewport origin
  // and moves in the direction of the swipe the same amount as the screenshot
  // in a back navigation.
  float final_position = ForwardEdge() == SwipeEdge::RIGHT
                             ? PhysicsModel::kScreenshotInitialPositionRatio
                             : -PhysicsModel::kScreenshotInitialPositionRatio;

  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_X_TRANSLATION(final_position * 0.3, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_X_TRANSLATION(final_position * 0.9, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_X_TRANSLATION(final_position * 0.3, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_X_TRANSLATION(final_position * 0.6, GetLivePageLayer()->transform());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));
  EXPECT_X_TRANSLATION(final_position * 0.9, GetLivePageLayer()->transform());
}

// Tests a forward navigation creates the expected layers and puts them in the
// correct z-order.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLayerOrderForwardNavigation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // The test starts off with the session history: [red&, green*]. Add an entry
  // and navigate back so we have entries in both directions:
  // [red&, green*, blue&].
  {
    ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
    WaitForCopyableViewInWebContents(web_contents());

    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
  }

  ASSERT_FALSE(GetAnimator());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          ForwardEdge(), NavType::kForward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  // A screenshot layer with a scrim should have been added. In a forward
  // navigation, the screenshot must appear on top of the live page.
  ASSERT_EQ("[LivePage,Screenshot[Scrim]]", ChildrenInOrder(*GetViewLayer()));

  // Prevent completing the invoke animation but trigger and finish the
  // navigation. This simulates the case where the new renderer commits while
  // the invoke animation is playing.
  {
    GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();
    TestNavigationManager forward_to_blue(web_contents(), BlueURL());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(forward_to_blue.WaitForNavigationFinished());
    EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());

    // A clone of the old page should be inserted under the screenshot but
    // above the current live page.
    ASSERT_EQ("[LivePage,OldSurfaceClone,Screenshot[Scrim]]",
              ChildrenInOrder(*GetViewLayer()));
  }

  // Make sure the new renderer has submitted a new frame and let the invoke
  // animation finish.
  {
    TestFuture<void> did_invoke;
    GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
    GetAnimator()->UnpauseAnimation();
    ASSERT_TRUE(did_invoke.Wait());

    // A clone of the old page should be removed. The screenshot should remain
    // on top of the live page.
    ASSERT_EQ("[LivePage,Screenshot[Scrim]]", ChildrenInOrder(*GetViewLayer()));
  }

  // Let the transition play to completion. The screenshot layers should all be
  // removed and the animator complete successfully.
  {
    TestFuture<AnimatorState> destroyed;
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
    ASSERT_TRUE(destroyed.Wait());
    EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());

    ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  }
}

// Verify transforms of screenshot and live layers at the end of a cancel
// animation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLayerTransformsAfterCancelBack) {
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);

  // Only a screenshot layer with a scrim should have been added, under the live
  // page.
  ASSERT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));
  ASSERT_EQ(GetScrimLayer()->background_color().fA, 0.1f);

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  TestFuture<void> did_cancel;
  TestFuture<AnimatorState> destroyed;

  // Extract the state during the cancel animation finished callback since the
  // layers will be removed synchronously right after that's called.
  gfx::Transform actual_screenshot_transform;
  gfx::Transform actual_live_transform;
  std::string actual_child_layers;
  GetAnimator()->set_on_cancel_animation_displayed(
      base::BindLambdaForTesting([&]() {
        actual_screenshot_transform = GetScreenshotLayer()->transform();
        actual_live_transform = GetLivePageLayer()->transform();
        actual_child_layers = ChildrenInOrder(*GetViewLayer());
        std::move(did_cancel.GetCallback()).Run();
      }));
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  GetAnimationManager()->OnGestureCancelled();
  ASSERT_TRUE(did_cancel.Wait());

  float expected_screenshot_ratio =
      BackEdge() == SwipeEdge::LEFT
          ? PhysicsModel::kScreenshotInitialPositionRatio
          : -PhysicsModel::kScreenshotInitialPositionRatio;
  EXPECT_X_TRANSLATION(expected_screenshot_ratio, actual_screenshot_transform);
  EXPECT_X_TRANSLATION(0, actual_live_transform);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", actual_child_layers);

  // When the cancel animation finishes it synchronously destroys the animator.
  EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());

  EXPECT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
}

// Verify transforms of screenshot and live layers at the end of the invoke
// animation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLayerTransformsAfterInvokeAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);

  // A screenshot layer should have been added, with the live page on top.
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  TestFuture<void> did_invoke;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(did_invoke.Wait());

  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  // The live page should be fully offscreen in the direction of the swipe. The
  // screenshot should be at the origin.
  float live_page_offset = BackEdge() == SwipeEdge::LEFT ? 1.f : -1.f;
  EXPECT_X_TRANSLATION(live_page_offset, GetLivePageLayer()->transform());
  EXPECT_X_TRANSLATION(0, GetScreenshotLayer()->transform());

  // Scrim should be at zero when the invoke animation is finished.
  EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.0f);
}

// Verify transforms of screenshot and live layers during the crossfade
// animation.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       TestLayerTransformsDuringCrossFade) {
  // TODO(bokan): Should this just be added to the harness?
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0), BackEdge(),
                                          NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  // Avoid processing any frames from the new renderer after invoking the
  // navigation to simulate the invoke animation finishing before the first
  // frame from the new page.
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(did_invoke.Wait());

  // Only a screenshot layer should have been added.
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  EXPECT_STATE_EQ(kWaitingForNewRendererToDraw, GetAnimator()->state());

  // Make sure the new renderer has submitted a new frame and manually call
  // OnRenderFrameMetadataChangedAfterActivation to move into cross fading.
  ASSERT_TRUE(WaitForFirstFrameInPrimaryMainFrame());
  GetAnimator()->set_intercept_render_frame_metadata_changed(false);
  base::TimeTicks now = base::TimeTicks();
  GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(now);

  EXPECT_STATE_EQ(kDisplayingCrossFadeAnimation, GetAnimator()->state());

  // The screenshot should be drawn on top of the live page now.
  EXPECT_EQ("[LivePage,Screenshot[Scrim]]", ChildrenInOrder(*GetViewLayer()));

  // Screenshot should still have the scrim and it should be at the end of its
  // timeline and fully opaque to start the cross fade.
  EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.f);
  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());
  EXPECT_X_TRANSLATION(0, GetScreenshotLayer()->transform());
  EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
  EXPECT_EQ(GetScreenshotLayer()->opacity(), 1.f);

  // First tick should setup the animation.
  now += base::Milliseconds(16);
  GetAnimator()->DirectlyCallOnAnimate(now);
  EXPECT_STATE_EQ(kDisplayingCrossFadeAnimation, GetAnimator()->state());
  EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.f);
  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());
  EXPECT_X_TRANSLATION(0, GetScreenshotLayer()->transform());
  EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
  EXPECT_EQ(GetScreenshotLayer()->opacity(), 1.f);

  // Next tick should animate screenshot opacity.
  now += base::Milliseconds(16);
  GetAnimator()->DirectlyCallOnAnimate(now);
  EXPECT_STATE_EQ(kDisplayingCrossFadeAnimation, GetAnimator()->state());
  EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
  EXPECT_LT(GetScreenshotLayer()->opacity(), 1.f);

  // Animate to finish.
  now += base::Seconds(99);
  GetAnimator()->DirectlyCallOnAnimate(now);

  ASSERT_TRUE(did_cross_fade.IsReady());
  EXPECT_EQ("[LivePage,Screenshot[Scrim]]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_X_TRANSLATION(0, GetLivePageLayer()->transform());
  EXPECT_X_TRANSLATION(0, GetScreenshotLayer()->transform());

  // The cross fade should have completed by now.
  EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
  EXPECT_EQ(GetScreenshotLayer()->opacity(), 0.f);

  // The scrim should remain completely transparent.
  EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.f);

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
    Combine(Values(UILayoutDirection::kLTR, UILayoutDirection::kRTL),
            Values(DSFMode::kOne, DSFMode::kFractional)),
    [](const TestParamInfo<
        BackForwardTransitionAnimationManagerBothEdgeBrowserTest::ParamType>&
           info) {
      return base::StrCat(
          {std::get<0>(info.param) == UILayoutDirection::kLTR ? "LTR" : "RTL",
           std::get<1>(info.param) == DSFMode::kOne ? "" : "FractionalDSF"});
    });

// Runs a transition in a ViewTransition enabled page. Ensures view transition
// does not run.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DefaultTransitionSupersedesViewTransition) {
  GURL first_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), first_url));
  WaitForCopyableViewInWebContents(web_contents());

  GURL second_url(embedded_test_server()->GetURL(
      "/view_transitions/basic-vt-opt-in.html?next"));
  ASSERT_TRUE(NavigateToURL(web_contents(), second_url));
  WaitForCopyableViewInWebContents(web_contents());

  // Back nav from the green page to the red page. The live page (green) is on
  // top and slides towards right. The red page (screenshot) is on the bottom
  // and appears on the left of screen.
  ASSERT_FALSE(GetAnimator());
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  ASSERT_TRUE(GetAnimator());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.9));

  TestFrameNavigationObserver back_navigation(web_contents());

  // Invoke the back navigation and wait for it to complete.
  {
    TestFuture<AnimatorState> destroyed;
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(destroyed.Wait());
    back_navigation.Wait();
  }

  // Ensure the new Document has produced a frame, otherwise `pagereveal` which
  // sets had_incoming_transition might not have been fired yet.
  WaitForCopyableViewInWebContents(web_contents());

  ASSERT_EQ(back_navigation.last_committed_url(), first_url);
  EXPECT_EQ(false, EvalJs(web_contents(), "had_incoming_transition"));
}

// If the destination has no screenshot, we will compose a fallback screenshot
// for transition. The destination page has no favicon so we don't draw
// the rounded rectangle.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DestinationHasNoScreenshot_NoFavicon) {
  std::optional<int> index =
      web_contents()->GetController().GetIndexForGoBack();
  ASSERT_TRUE(index);
  NavigationEntryImpl* red_entry =
      web_contents()->GetController().GetEntryAtIndex(*index);
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetNavigationEntryScreenshotCache()
                  ->RemoveScreenshot(red_entry));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);

  // live page layer with screenshot underneath. No rrect.
  ASSERT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.2));
  auto expected_bg_color = web_contents()
                               ->GetDelegate()
                               ->GetBackForwardTransitionFallbackUXConfig()
                               .background_color;
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.5));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.8));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  TestFrameNavigationObserver back_navigation(web_contents());

  // Trigger and complete the back navigation.
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(destroyed.Wait());
  back_navigation.Wait();

  ASSERT_EQ(back_navigation.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// If the destination has no screenshot, we will compose a fallback screenshot
// for transition. The destination page has a favicon so we  draw
// the rounded rectangle, and the rrect embeds the favicon.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DestinationHasNoScreenshot_HasFavicon) {
  SkBitmap stub_favicon;
  stub_favicon.allocN32Pixels(20, 20);
  stub_favicon.eraseColor(SkColors::kMagenta);
  stub_favicon.setImmutable();
  std::optional<int> index =
      web_contents()->GetController().GetIndexForGoBack();
  ASSERT_TRUE(index);
  NavigationEntryImpl* red_entry =
      web_contents()->GetController().GetEntryAtIndex(*index);
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetNavigationEntryScreenshotCache()
                  ->RemoveScreenshot(red_entry));
  red_entry->navigation_transition_data().set_favicon(stub_favicon);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);

  // live page layer with screenshot underneath, and the rounded rectangle is
  // above the scrim.
  ASSERT_EQ("[Screenshot[Scrim,RRect[Favicon]],LivePage]",
            ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.2));
  auto expected_bg_color = web_contents()
                               ->GetDelegate()
                               ->GetBackForwardTransitionFallbackUXConfig()
                               .background_color;
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_TRUE(base::IsApproximatelyEqual(GetRRectLayer()->opacity(), 0.f,
                                         kFloatTolerance));
  // Opacity value isn't propagated into the subtree.
  EXPECT_TRUE(base::IsApproximatelyEqual(GetFaviconLayer()->opacity(), 1.f,
                                         kFloatTolerance));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.5));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_TRUE(base::IsApproximatelyEqual(GetRRectLayer()->opacity(), 0.7f,
                                         kFloatTolerance));
  EXPECT_TRUE(base::IsApproximatelyEqual(GetFaviconLayer()->opacity(), 1.f,
                                         kFloatTolerance));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.8));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_TRUE(base::IsApproximatelyEqual(GetRRectLayer()->opacity(), 1.f,
                                         kFloatTolerance));
  EXPECT_TRUE(base::IsApproximatelyEqual(GetFaviconLayer()->opacity(), 1.f,
                                         kFloatTolerance));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  EXPECT_EQ(GetScreenshotLayer()->background_color(), expected_bg_color);
  EXPECT_TRUE(base::IsApproximatelyEqual(GetRRectLayer()->opacity(), 0.02f,
                                         kFloatTolerance));
  EXPECT_TRUE(base::IsApproximatelyEqual(GetFaviconLayer()->opacity(), 1.f,
                                         kFloatTolerance));

  TestFrameNavigationObserver back_navigation(web_contents());

  // Trigger and complete the back navigation.
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(destroyed.Wait());
  back_navigation.Wait();

  ASSERT_EQ(back_navigation.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ChainedBackGesture) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Navigate to a third page to enable two consecutive back navigations.
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  WaitForCopyableViewInWebContents(web_contents());

  TestFuture<AnimatorState> destroyed_first;

  // First back gesture - start and progress partially
  {
    GetAnimationManager()->OnGestureStarted(
        ui::BackGestureEvent(0), SwipeEdge::LEFT, NavType::kBackward);

    TestFuture<void> did_invoke_first;
    GetAnimator()->set_on_invoke_animation_displayed(
        did_invoke_first.GetCallback());
    GetAnimator()->set_on_impl_destroyed(destroyed_first.GetCallback());

    GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
    GetAnimationManager()->OnGestureInvoked();
    EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
    ASSERT_TRUE(did_invoke_first.Wait());
  }

  // Second back gesture - start before the first one is completed.
  // The second gesture should immediately take over and progress.
  {
    GetAnimationManager()->OnGestureStarted(
        ui::BackGestureEvent(0), SwipeEdge::LEFT, NavType::kBackward);
    ASSERT_TRUE(GetAnimator());
    ASSERT_TRUE(destroyed_first.IsReady());
    EXPECT_STATE_EQ(kAnimationAborted, destroyed_first.Get());
    EXPECT_STATE_EQ(kStarted, GetAnimator()->state());

    // The navigation should go back to the red page (two back navigations).
    TestFrameNavigationObserver back_to_red(web_contents());

    TestFuture<void> did_cross_fade;
    TestFuture<AnimatorState> destroyed;
    GetAnimator()->set_on_cross_fade_animation_displayed(
        did_cross_fade.GetCallback());
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

    GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(did_cross_fade.Wait());

    ASSERT_TRUE(destroyed.Wait());
    EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());

    back_to_red.Wait();
    ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  }

  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Assert that if the user does not start the navigation, we don't put the
// fallback screenshot back.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       Cancel_DestinationNoScreenshot) {
  std::optional<int> index =
      web_contents()->GetController().GetIndexForGoBack();
  ASSERT_TRUE(index);
  auto* red_entry = web_contents()->GetController().GetEntryAtIndex(*index);
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetNavigationEntryScreenshotCache()
                  ->RemoveScreenshot(red_entry));

  {
    TestFuture<void> did_cancel;

    GetAnimationManager()->OnGestureStarted(
        ui::BackGestureEvent(0), SwipeEdge::LEFT, NavType::kBackward);
    GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
    GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
    GetAnimationManager()->OnGestureCancelled();

    ASSERT_TRUE(did_cancel.Wait());
  }

  ASSERT_EQ(web_contents()->GetController().GetActiveEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Simulating the user click the X button to cancel the navigation while the
// animation is at commit-pending.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationAborted) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // We haven't started the navigation at this point.
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  // The user has lifted the finger - signaling the start of the navigation.
  TestNavigationManager back_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  // The user clicks the X (stop) button in the UI.
  web_contents()->Stop();
  ASSERT_TRUE(did_cancel.Wait());
  ASSERT_FALSE(back_to_red.was_committed());
  ASSERT_TRUE(destroyed.Wait());

  // Screenshot layer should be removed and the page should be back at the
  // origin.
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  // [red, green*].
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_TRUE(web_contents()->GetController().GetEntryAtIndex(0)->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// The invoke animation is displaying and the gesture navigation is <
// READY_TO_COMMIT. A secondary navigation cancels our gesture navigation as the
// gesture navigation has not told the renderer to commit. The cancel animation
// will be placed to bring the active page back.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       GestureNavigationBeingReplaced) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  {
    // The pause here prevents the manager from finishing the invoke animation.
    // When the navigation to blue starts, blue's navigation request will cancel
    // the red's navigation request, and the manager will get a
    // DidFinishNavigation to advance itself from `kDisplayingInvokeAnimation`
    // to `kDisplayingCancelAnimation`.
    GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();

    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(back_nav_to_red.WaitForRequestStart());

    EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());
  }

  // We can't use NavigateToURL() here. NavigateToURL will wait for the
  // current WebContents to stop loading. We have an on-going navigation here
  // so the wait will timeout.
  {
    TestNavigationManager nav_to_blue(web_contents(), BlueURL());
    web_contents()->GetController().LoadURL(
        BlueURL(), Referrer{},
        ui::PageTransitionFromInt(
            ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
            ui::PageTransition::PAGE_TRANSITION_TYPED),
        std::string{});
    ASSERT_TRUE(nav_to_blue.WaitForRequestStart());
    // The start of blue will advance the manager to
    // kDisplayingCancelAnimation.
    EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
    // Force the cancel animation to finish playing, by unpausing it and
    // calling OnAnimate on it.
    GetAnimator()->UnpauseAnimation();

    ASSERT_TRUE(did_cancel.Wait());
    ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  }

  ASSERT_TRUE(destroyed.Wait());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  ASSERT_FALSE(back_nav_to_red.was_committed());
}

// The user swipes across the screen while a cross-doc navigation commits. We
// destroy the animation manager synchronously.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileOnGestureProgressed) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// The cancel animation is displaying while a cross-doc navigation commits. We
// destroy the animation manager synchronously.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileDisplayingCancelAnimation) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager()->OnGestureCancelled();
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileWaitingForRendererNewFrame) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  // The user has lifted the finger - signaling the start of the navigation.
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Intercept all the `OnRenderFrameMetadataChangedAfterActivation()`s.
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(did_invoke.Wait());

  EXPECT_STATE_EQ(kWaitingForNewRendererToDraw, GetAnimator()->state());

  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Test `BackForwardTransitionAnimator::StartNavigationAndTrackRequest()`
// returns false:
// - at OnGestureStarted() there is a destination entry;
// - at OnGestureInvoked() the entry cannot be found.
// - Upon the user lifts the finger, the cancel animation should be played, and
//   no navigation committed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NotAbleToStartNavigationOnInvoke) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  // Only have the active green entry after this call.
  // `StartNavigationAndTrackRequest()` will fail.
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  web_contents()->GetController().PruneAllButLastCommitted();
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_cancel.IsReady());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
}

// Test that the animation manager is blocked by the renderer's impl thread
// submitting a new compostior frame.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimationStaysBeforeFrameActivation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  // The user has lifted the finger - signaling the start of the navigation.
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Intercept all the `OnRenderFrameMetadataChangedAfterActivation()`s.
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(did_invoke.Wait());
  EXPECT_STATE_EQ(kWaitingForNewRendererToDraw, GetAnimator()->state());

  GetAnimator()->set_intercept_render_frame_metadata_changed(false);
  GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(base::TimeTicks());

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_cross_fade.IsReady());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Test that the animation manager is destroyed when the visibility changes for
// that tab.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnVisibilityChange) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();

  {
    TestNavigationManager back_nav_to_red(web_contents(), RedURL());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  }

  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  // The first two args don't matter in tests.
  window->OnVisibilityChanged(
      /*env=*/nullptr,
      /*obj=*/base::android::JavaParamRef<jobject>(nullptr),
      /*visible=*/false);
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Test that the animation manager is destroyed when the browser compositor is
// detached.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnDetachCompositor) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();

  {
    TestNavigationManager back_nav_to_red(web_contents(), RedURL());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  }

  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  window->DetachCompositor();

  ASSERT_TRUE(destroyed.IsReady());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Assert that non primary main frame navigations won't cancel the ongoing
// animation.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       IgnoreNonPrimaryMainFrameNavigations) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Add an iframe to the green page while the gesture is in-progress. This will
  // trigger a renderer-initiated navigation in the subframe.
  constexpr char kAddIframeScript[] = R"({
    (()=>{
        return new Promise((resolve) => {
          const frame = document.createElement('iframe');
          frame.addEventListener('load', () => {resolve();});
          frame.src = $1;
          document.body.appendChild(frame);
        });
    })();
  })";
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace(kAddIframeScript, BlueURL())));

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  EXPECT_TRUE(did_cross_fade.IsReady());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            RedURL());
}

// Assert that during OnAnimate, if the current animation hasn't finish, we
// should expect a follow up OnAnimate call.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnAnimateIsCalled) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  GetAnimator()->set_duration_between_frames(base::Milliseconds(1));
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());
  {
    SCOPED_TRACE("first on animate call");
    base::RunLoop first_on_animate_call;
    GetAnimator()->set_next_on_animate_callback(
        first_on_animate_call.QuitClosure());
    first_on_animate_call.Run();
    EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());
  }
  GetAnimator()->set_duration_between_frames(kLongDurationBetweenFrames);
  {
    SCOPED_TRACE("second on animate call");
    base::RunLoop second_on_animate_call;
    GetAnimator()->set_next_on_animate_callback(
        second_on_animate_call.QuitClosure());
    second_on_animate_call.Run();
  }

  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  EXPECT_TRUE(did_cross_fade.IsReady());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Test that, when the browser receives the DidCommit message, Viz has already
// activated a render frame, we will also skip `kWaitingForNewRendererToDraw`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       RenderFrameActivatedBeforeDidCommit) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> did_cross_fade;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  bool received_frame_while_waiting = false;
  GetAnimator()->set_waited_for_renderer_new_frame(base::BindLambdaForTesting(
      [&]() { received_frame_while_waiting = true; }));

  TestNavigationManager back_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Manually set the new frame metadata before the DidCommit message and call
  // `OnRenderFrameMetadataChangedAfterActivation()` to simulate a frame
  // activation.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimator()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(back_to_red.GetNavigationHandle());
    GetAnimator()->set_post_ready_to_commit_callback(
        base::BindLambdaForTesting([&]() {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_cross_fade.IsReady());

  ASSERT_FALSE(received_frame_while_waiting);

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// Test that, when the invoke animation finishes (when the active page is
// completely out of the view port), if Viz has already activated a new frame
// submitted by the new renderer, we skip `kWaitingForNewRendererToDraw`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       RenderFrameActivatedDuringInvokeAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  bool received_frame_while_waiting = false;
  GetAnimator()->set_waited_for_renderer_new_frame(base::BindLambdaForTesting(
      [&]() { received_frame_while_waiting = true; }));

  TestNavigationManager back_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Manually set the new frame metadata before the DidCommit message and call
  // `OnRenderFrameMetadataChangedAfterActivation()` to simulate a frame
  // activation. Do this at the end of the "DidCommit" stack to simulate the
  // viz activates the first frame while the invoke animation is still playing.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimator()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(back_to_red.GetNavigationHandle());
    GetAnimator()->set_did_finish_navigation_callback(
        base::BindLambdaForTesting([&](bool) {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  ASSERT_FALSE(received_frame_while_waiting);

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

// E.g., google.com --back nav--> bank.com. Bank.com commits, but before the
// invoke animation has finished, bank.com's document redirects the user to
// bank.com/login.html.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirectWhileDisplayingInvokeAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<bool> did_finish_navigation;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_did_finish_navigation_callback(
      did_finish_navigation.GetCallback());

  GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();

  {
    TestNavigationManager back_to_red(web_contents(), RedURL());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  }

  ASSERT_TRUE(did_finish_navigation.Wait());

  // Navigate to the blue page while the animator is still displaying the
  // invoke animation.
  EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());

  {
    TestNavigationManager nav_to_blue(web_contents(), BlueURL());
    // Simulate a client redirect, from red's document.
    ASSERT_TRUE(ExecJs(web_contents(), "window.location.href = 'blue.html'"));
    ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  }

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirectWhileWaitingForNewFrame) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<bool> did_finish_navigation;
  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_did_finish_navigation_callback(
      did_finish_navigation.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  {
    TestNavigationManager back_to_red(web_contents(), RedURL());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(back_to_red.WaitForResponse());
    GetAnimator()->set_intercept_render_frame_metadata_changed(true);
    ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  }

  ASSERT_TRUE(did_finish_navigation.Wait());
  ASSERT_TRUE(did_invoke.Wait());

  EXPECT_STATE_EQ(kWaitingForNewRendererToDraw, GetAnimator()->state());

  {
    TestNavigationManager nav_to_blue(web_contents(), BlueURL());
    // Simulate a client redirect, from red's document.
    ASSERT_TRUE(ExecJs(web_contents(), "window.location.href = 'blue.html'"));
    ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  }

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
  EXPECT_FALSE(did_cross_fade.IsReady());

  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  // [red, blue]. The green entry is pruned because of the client redirect.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            BlueURL());
}

// Assert that navigating from a crashed page should have no impact on the
// animations.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigatingFromACrashedPage) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Crash the green page.
  RenderFrameHostWrapper crashed(web_contents()->GetPrimaryMainFrame());
  RenderProcessHostWatcher crashed_obs(
      crashed->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  crashed->GetProcess()->Shutdown(content::RESULT_CODE_KILLED);
  crashed_obs.Wait();
  ASSERT_TRUE(crashed.WaitUntilRenderFrameDeleted());
  // The crashed RFH is still owned by the RFHManager.
  ASSERT_FALSE(crashed.IsDestroyed());
  ASSERT_FALSE(crashed->IsRenderFrameLive());
  ASSERT_FALSE(crashed->GetView());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestFrameNavigationObserver back_to_red(web_contents());

  // Ignore frames from the new RenderFrameHost until we're ready.
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);

  GetAnimationManager()->OnGestureInvoked();
  back_to_red.Wait();

  // Wait for the invoke animation to finish.
  {
    ASSERT_TRUE(did_invoke.Wait());
    EXPECT_STATE_EQ(kWaitingForNewRendererToDraw, GetAnimator()->state());

    // A screenshot layer should have been added, with the live page on top.
    EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

    // The live page should be fully offscreen in the direction of the swipe.
    // The screenshot should be at the origin.
    EXPECT_X_TRANSLATION(1.f, GetLivePageLayer()->transform());
    EXPECT_X_TRANSLATION(0.f, GetScreenshotLayer()->transform());

    // Scrim should be at zero when the invoke animation is finished.
    EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.f);
  }

  base::TimeTicks now = base::TimeTicks();

  // Un-block waiting on the frame activation to start the cross fade animation.
  {
    // If the new frame hasn't yet submitted a new frame, wait for it so that
    // calling OnRenderFrameMetadataChangedAfterActivation moves the animator
    // into a cross fade.
    ASSERT_TRUE(WaitForFirstFrameInPrimaryMainFrame());
    GetAnimator()->set_intercept_render_frame_metadata_changed(false);
    GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(now);
    EXPECT_STATE_EQ(kDisplayingCrossFadeAnimation, GetAnimator()->state());

    // Screenshot should still have the scrim and it should be at the end of
    // its timeline. Both screenshot and live page should be fully opaque to
    // start the cross fade.
    EXPECT_EQ(GetScrimLayer()->background_color().fA, 0.f);
    EXPECT_X_TRANSLATION(0.f, GetLivePageLayer()->transform());
    EXPECT_X_TRANSLATION(0.f, GetScreenshotLayer()->transform());
    EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
    EXPECT_EQ(GetScreenshotLayer()->opacity(), 1.f);
  }

  // Wait for the crossfade animation to complete.
  {
    ASSERT_TRUE(did_cross_fade.Wait());
    ASSERT_TRUE(destroyed.IsReady());
    EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());

    EXPECT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
    EXPECT_X_TRANSLATION(0.f, GetLivePageLayer()->transform());

    // The cross fade should have completed.
    EXPECT_EQ(GetLivePageLayer()->opacity(), 1.f);
  }

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Regression test for https://crbug.com/326516254: If the destination page is
// skipped for a back/forward navigation due to the lack of user activation, the
// animator should also skip that entry.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       SkipPageWithNoUserActivation) {
  auto& nav_controller = web_contents()->GetController();

  // [red&, green&, blue*]
  {
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
    ASSERT_EQ(nav_controller.GetEntryCount(), 3);
    ASSERT_EQ(nav_controller.GetCurrentEntryIndex(), 2);
  }

  // Mark green as skipped.
  nav_controller.GetEntryAtIndex(1)->set_should_skip_on_back_forward_ui(true);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cross_fade;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());

  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_cross_fade.IsReady());
  back_to_red.Wait();

  // TODO(https://crbug.com/325329998): We should also test that the transition
  // is from blue to red via pixel comparison.

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_EQ(nav_controller.GetEntryCount(), 3);
  ASSERT_EQ(nav_controller.GetCurrentEntryIndex(), 0);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DifferentScreenAndScreenshotOrientation) {
  // Resize the screen.
  auto* native_view = web_contents()->GetNativeView();
  ASSERT_TRUE(native_view);
  native_view->OnPhysicalBackingSizeChanged(
      gfx::ScaleToCeiledSize(native_view->GetPhysicalBackingSize(), 2, 0.5f));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  ASSERT_TRUE(GetAnimator());

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.5));
  EXPECT_EQ(GetScreenshotLayer()->background_color(),
            web_contents()
                ->GetDelegate()
                ->GetBackForwardTransitionFallbackUXConfig()
                .background_color);
  EXPECT_FALSE(GetRRectLayer());
  EXPECT_FALSE(GetFaviconLayer());

  const auto& children =
      static_cast<WebContentsViewAndroid*>(web_contents()->GetView())
          ->parent_for_web_page_widgets()
          ->parent()
          ->children();
  // `parent_for_web_page_widgets()` and the screenshot.
  ASSERT_EQ(children.size(), 2U);
  auto* fallback_screenshot = GetScreenshotLayer();
  auto expected_bg_color = web_contents()
                               ->GetDelegate()
                               ->GetBackForwardTransitionFallbackUXConfig()
                               .background_color;
  ASSERT_EQ(fallback_screenshot->background_color(), expected_bg_color);

  TestFrameNavigationObserver back_to_red(web_contents());
  base::test::TestFuture<void> cross_fade_displayed;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.GetCallback());
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(cross_fade_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());
  back_to_red.Wait();

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

namespace {

// Wait for the main frame to receive a UpdateUserActivationState from the
// renderer with the expected new state.
class BrowserUserActivationWaiter
    : public UpdateUserActivationStateInterceptor {
 public:
  BrowserUserActivationWaiter(
      RenderFrameHost* rfh,
      blink::mojom::UserActivationNotificationType expected_type)
      : UpdateUserActivationStateInterceptor(rfh),
        expected_type_(expected_type) {}
  ~BrowserUserActivationWaiter() override = default;

  // Blocks until the renderer sends the expected user activation via
  // `UpdateUserActivationState()`.
  void Wait() { run_loop_.Run(); }

  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override {
    if (notification_type == expected_type_) {
      run_loop_.Quit();
    }
    UpdateUserActivationStateInterceptor::UpdateUserActivationState(
        update_type, notification_type);
  }

 private:
  const blink::mojom::UserActivationNotificationType expected_type_;
  base::RunLoop run_loop_;
};

void InjectBeforeUnload(ToRenderFrameHost adapter) {
  static constexpr std::string_view kScript = R"(
    window.onbeforeunload = (event) => {
      // Recommended
      event.preventDefault();

      // Included for legacy support, e.g. Chrome/Edge < 119
      event.returnValue = true;
    };
  )";
  ASSERT_TRUE(ExecJs(adapter, kScript, EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void AddUserActivationForBeforeUnload(RenderFrameHostImpl* frame) {
  // Set the sticky user activation and let the bit propagate from renderer to
  // the browser.
  BrowserUserActivationWaiter wait_for_expected_user_activation(
      frame, blink::mojom::UserActivationNotificationType::kTest);
  frame->GetAssociatedLocalFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  wait_for_expected_user_activation.Wait();
  ASSERT_TRUE(frame->ShouldDispatchBeforeUnload(
      /*check_subframes_only=*/false));
  ASSERT_TRUE(frame->HasStickyUserActivation());
}

// Inject a BeforeUnload handler into `adapter`, and maybe set the stick user
// activation.
void InjectBeforeUnloadAndSetStickyUserActivation(
    ToRenderFrameHost adapter,
    bool set_sticky_user_activation = true) {
  InjectBeforeUnload(adapter);

  auto* frame = static_cast<RenderFrameHostImpl*>(adapter.render_frame_host());

  if (set_sticky_user_activation) {
    AddUserActivationForBeforeUnload(frame);
  } else {
    ASSERT_TRUE(
        frame->ShouldDispatchBeforeUnload(/*check_subframes_only=*/false));
    ASSERT_FALSE(frame->HasStickyUserActivation());
  }
}

// Intercept the BeforeUnload dialog. Used to block the execution until the
// confirmation dialog shows up, and to interact with the dialog to either
// cancel or start the navigation.
class BeforeUnloadDialogObserver
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit BeforeUnloadDialogObserver(RenderFrameHostImpl* frame)
      : frame_(frame), impl_(receiver().SwapImplForTesting(this)) {}
  ~BeforeUnloadDialogObserver() override = default;

  // `blink::mojom::LocalFrameHostInterceptorForTesting`:
  LocalFrameHost* GetForwardingInterface() override { return impl_; }
  void RunBeforeUnloadConfirm(
      bool is_reload,
      RunBeforeUnloadConfirmCallback callback) override {
    CHECK(!is_reload);
    ack_ = std::move(callback);
    run_loop_.Quit();
    // Reset immediately. `frame_` and `impl_` will be destroyed once
    // `ack_` is executed with "proceed".
    std::ignore = receiver().SwapImplForTesting(impl_);
    frame_ = nullptr;
    impl_ = nullptr;
  }

  void WaitForDialog() { run_loop_.Run(); }

  void RespondToDialogue(bool proceed) { std::move(ack_).Run(proceed); }

  [[nodiscard]] bool shown() const { return !frame_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>& receiver() {
    return frame_->local_frame_host_receiver_for_testing();
  }

  raw_ptr<RenderFrameHostImpl> frame_;
  raw_ptr<blink::mojom::LocalFrameHost> impl_;
  base::RunLoop run_loop_;
  RunBeforeUnloadConfirmCallback ack_;
};

}  // namespace

// Test the case where the renderer acks the BeforeUnload message without
// showing a prompt.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Proceed_NoPrompt) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadAndSetStickyUserActivation(
      web_contents(), /*set_sticky_user_activation=*/false);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<bool> did_finish_nav;
  TestFuture<void> did_cross_fade;
  TestFuture<void> did_cancel;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_did_finish_navigation_callback(
      did_finish_nav.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  EXPECT_TRUE(did_cross_fade.IsReady());
  EXPECT_TRUE(did_finish_nav.IsReady());
  back_to_red.Wait();
  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());

  ASSERT_FALSE(dialog_observer.shown());
  ASSERT_FALSE(did_cancel.IsReady());
}

// Test the case where the renderer shows a prompt for the BeforeUnload message,
// and the user decides to proceed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Proceed_WithPrompt) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
  InjectBeforeUnloadAndSetStickyUserActivation(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<bool> did_finish_nav;
  TestFuture<void> did_cancel;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_did_finish_navigation_callback(
      did_finish_nav.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(did_cancel.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/true);

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  EXPECT_TRUE(did_finish_nav.IsReady());

  back_to_red.Wait();
  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());

  ASSERT_TRUE(dialog_observer.shown());
}

// Test the case where the user cancels the navigation via the prompt, after
// the cancel animation finishes.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Cancel_AfterCancelAnimationFinishes) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadAndSetStickyUserActivation(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(did_cancel.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/false);

  ASSERT_TRUE(destroyed.Wait());
  ASSERT_FALSE(back_to_red.last_navigation_succeeded());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_TRUE(dialog_observer.shown());
}

// Test the case where the user cancels the navigation via the prompt, before
// the cancel animation finishes.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Cancel_BeforeCancelAnimationFinishes) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadAndSetStickyUserActivation(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager()->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimator()->UnpauseAnimation();

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_cancel.IsReady());
  ASSERT_FALSE(back_to_red.last_navigation_succeeded());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_TRUE(dialog_observer.shown());
}

// Test that when the user has decided not leave the current page by interacting
// with the prompt and the cancel animation is still playing, another navigation
// commits in the main frame. We should destroy the animator when the other
// navigation commits.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_RequestCancelledBeforeStart) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadAndSetStickyUserActivation(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimator()->set_duration_between_frames(base::Microseconds(1));
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager()->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
  // Expectation the animator will be destroyed while playing the cancel
  // animation.
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimator()->UnpauseAnimation();

  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_FALSE(did_cancel.IsReady());
  ASSERT_TRUE(dialog_observer.shown());

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 3);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 2);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       HasUaVisualTransitionSameDocument) {
  GURL url1 = embedded_test_server()->GetURL(
      "a.com", "/has-ua-visual-transition.html#frag1");
  GURL url2 = embedded_test_server()->GetURL(
      "a.com", "/has-ua-visual-transition.html#frag2");
  NavigationHandleCommitObserver navigation_0(web_contents(), url1);
  NavigationHandleCommitObserver navigation_1(web_contents(), url2);

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  // The NavigationEntry changes on a same-document navigation.
  EXPECT_NE(web_contents()->GetController().GetLastCommittedEntry(), entry);
  EXPECT_FALSE(
      EvalJs(web_contents(), "hasUAVisualTransitionValue").ExtractBool());

  EXPECT_TRUE(navigation_0.has_committed());
  EXPECT_TRUE(navigation_1.has_committed());
  EXPECT_FALSE(navigation_0.was_same_document());
  EXPECT_TRUE(navigation_1.was_same_document());

  TestNavigationManager manager(web_contents(), url1);
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(manager.WaitForNavigationFinished());
  ASSERT_TRUE(
      EvalJs(web_contents(), "hasUAVisualTransitionValue").ExtractBool());
}

// Test the case where script commits a same-document navigation in beforeunload
// while the cancel animation is playing.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTest,
    BeforeUnload_SameDocumentNavigation_DuringCancelAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("/before_unload_same_doc_nav.html")));
  AddUserActivationForBeforeUnload(web_contents()->GetPrimaryMainFrame());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();

  GetAnimationManager()->OnGestureInvoked();
  EXPECT_TRUE(web_contents()->HasUncommittedNavigationInPrimaryMainFrame());

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
      embedded_test_server()->GetURL("/before_unload_same_doc_nav.html#foo"));
}

namespace {
class FailBeginNavigationImpl : public ContentBrowserTestContentBrowserClient {
 public:
  FailBeginNavigationImpl() = default;
  ~FailBeginNavigationImpl() override = default;

  // `ContentBrowserTestContentBrowserClient`:
  bool ShouldOverrideUrlLoading(FrameTreeNodeId frame_tree_node_id,
                                bool browser_initiated,
                                const GURL& gurl,
                                const std::string& request_method,
                                bool has_user_gesture,
                                bool is_redirect,
                                bool is_outermost_main_frame,
                                bool is_prerendering,
                                ui::PageTransition transition,
                                bool* ignore_navigation) final {
    // See `NavigationRequest::BeginNavigationImpl()`.
    *ignore_navigation = true;
    return true;
  }
};
}  // namespace

// Test that the animator is behaving correctly, even after the renderer acks
// the BeforeUnload message to proceed (begin) the navigation, but
// `BeginNavigationImpl()` hits an early out so we never each
// `DidStartNavigation()`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_BeginNavigationImplFails) {
  FailBeginNavigationImpl fail_begin_navigation_client;

  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadAndSetStickyUserActivation(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cancel;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(did_cancel.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/true);

  ASSERT_TRUE(destroyed.Wait());
  ASSERT_TRUE(dialog_observer.shown());

  // Still on the green page.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
}

// Testing that, on the back nav from green.html to red.html, red.html redirects
// to blue.html. while the cross-fading animation is playing from the red.html's
// screenshot to the live page. We should abort the cross-fade animation when
// the redirect to blue.html commits.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirect_AnimatorDestroyedDuringCrossFade) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  GURL client_redirect =
      embedded_test_server()->GetURL("/red_redirect_to_blue.html#redirect");

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  web_contents()->GetController().GetEntryAtIndex(0)->SetURL(client_redirect);

  TestNavigationManager back_nav_to_red(web_contents(), client_redirect);
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());

  GetAnimator()->PauseAnimationAtDisplayingCrossFadeAnimation();

  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Force a call of `OnRenderFrameMetadataChangedAfterActivation()` when the
  // navigation back to red is committed. This makes sure that the animation
  // manager is displaying the cross-fade animation while the redirec to blue
  // is happening.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimator()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(
            back_nav_to_red.GetNavigationHandle());
    GetAnimator()->set_did_finish_navigation_callback(
        base::BindLambdaForTesting([&](bool) {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_red.was_successful());
  ASSERT_TRUE(did_invoke.Wait());
  EXPECT_STATE_EQ(kDisplayingCrossFadeAnimation, GetAnimator()->state());

  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            BlueURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(1)->GetURL(),
            GreenURL());
}

// Test that input isn't dispatched to the renderer while the transition
// animation is in progress.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       SuppressRendererInputDuringTransition) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Start a back transition gesture.
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  // Once the gesture's invoked, block the response so we're waiting with the
  // transition active.
  TestNavigationManager back_nav_to_green(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_green.WaitForResponse());

  // Simulate various kinds of user input, these events should not be dispatched
  // to the renderer.
  {
    InputMsgWatcher input_watcher(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kUndefined);
    SimulateGestureScrollSequence(web_contents(), gfx::Point(100, 100),
                                  gfx::Vector2dF(0, 50));
    {
      SCOPED_TRACE("process_scroll");
      RunUntilInputProcessed(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    }
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);

    SimulateTapDownAt(web_contents(), gfx::Point(100, 100));
    SimulateTapAt(web_contents(), gfx::Point(100, 100));
    {
      SCOPED_TRACE("process_tap");
      RunUntilInputProcessed(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    }

    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);

    SimulateMouseClick(web_contents(),
                       blink::WebInputEvent::Modifiers::kNoModifiers,
                       blink::WebMouseEvent::Button::kLeft);
    {
      SCOPED_TRACE("process_mouse_click");
      RunUntilInputProcessed(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    }
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);
  }

  // Unblock the navigation and wait until the transition is completed.
  ASSERT_TRUE(back_nav_to_green.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_green.was_successful());

  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());

  // Ensure input is now successfully dispatched.
  {
    InputMsgWatcher input_watcher(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kUndefined);
    SimulateTapDownAt(web_contents(), gfx::Point(100, 100));
    SimulateTapAt(web_contents(), gfx::Point(100, 100));

    {
      SCOPED_TRACE("process_not_suppressed_tap");
      RunUntilInputProcessed(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    }
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kGestureTap);
  }
}

// Regression test for https://crbug.com/339501357: If the animator is destroyed
// in the middle of a gesture, the history navigation should still proceed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimatorDestroyedMidGesture) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Start a navigation and wait until the request has been sent
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  web_contents()->GetController().LoadURL(
      BlueURL(), Referrer{},
      ui::PageTransitionFromInt(
          ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
          ui::PageTransition::PAGE_TRANSITION_TYPED),
      std::string{});
  ASSERT_TRUE(nav_to_blue.WaitForRequestStart());

  // Start a swipe gesture
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  // When the navigation above commits the animator should be destroyed with an
  // abort
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_red.was_committed());
}

// Regression test for https://crbug.com/344761329: If the
// WebContentsViewAndroid's native view is detached from the root window, we
// should abort the transition.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimatorDestroyedWhenViewAndroidDetachedFromWindow) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();
  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());

  web_contents()->GetWebContentsAndroid()->SetTopLevelNativeWindow(
      /*env=*/nullptr,
      /*jwindow_android=*/base::android::JavaParamRef<jobject>(nullptr));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AbortAnimationOnPhysicalSizeChange) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  EXPECT_TRUE(back_nav_to_red.WaitForRequestStart());

  web_contents()->GetNativeView()->OnPhysicalBackingSizeChanged(
      gfx::Size(1, 1));

  // The navigation should proceed regardless of the animation.
  EXPECT_TRUE(back_nav_to_red.WaitForNavigationFinished());

  EXPECT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ScreenshotCompression) {
  SkBitmap expected_pixels;
  {
    NavigationEntryScreenshot::SetDisableCompressionForTesting(true);
    ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
              GreenURL());
    GetAnimationManager()->OnGestureStarted(
        ui::BackGestureEvent(0), SwipeEdge::LEFT, NavType::kBackward);
    GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

    TestFuture<gfx::Image> result;
    auto* window = web_contents()->GetNativeView()->GetWindowAndroid();
    ui::GrabWindowSnapshot(window, gfx::Rect(), result.GetCallback());
    expected_pixels = result.Get().AsBitmap();
    ASSERT_FALSE(expected_pixels.empty());

    for (int col = 0.6 * expected_pixels.width() + 1;
         col < expected_pixels.width(); col++) {
      for (int row = 0; row < expected_pixels.height(); row++) {
        ASSERT_EQ(expected_pixels.getColor(col, row), SK_ColorGREEN)
            << col << "," << row << " and image "
            << cc::GetPNGDataUrl(expected_pixels);
      }
    }

    TestFuture<AnimatorState> destroyed;
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    GetAnimationManager()->OnGestureInvoked();
    ASSERT_TRUE(destroyed.Wait());
    ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
              RedURL());
    observer.Wait();
  }

  SkBitmap actual_pixels;
  {
    NavigationEntryScreenshot::SetDisableCompressionForTesting(false);
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), GreenURL()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
    NavigationTransitionTestUtils::WaitForScreenshotCompressed(
        web_contents()->GetController(),
        web_contents()->GetController().GetLastCommittedEntryIndex() - 1);

    GetAnimationManager()->OnGestureStarted(
        ui::BackGestureEvent(0), SwipeEdge::LEFT, NavType::kBackward);
    GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

    TestFuture<gfx::Image> result;
    auto* window = web_contents()->GetNativeView()->GetWindowAndroid();
    ui::GrabWindowSnapshot(window, gfx::Rect(), result.GetCallback());
    actual_pixels = result.Get().AsBitmap();
    ASSERT_FALSE(actual_pixels.empty());

    TestFuture<AnimatorState> destroyed;
    GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
    GetAnimationManager()->OnGestureCancelled();
    ASSERT_TRUE(destroyed.Wait());
  }

  // Allow all pixels to be off by 1.
  auto comparator = cc::FuzzyPixelComparator()
                        .SetErrorPixelsPercentageLimit(100.0f)
                        .SetAbsErrorLimit(2);
  EXPECT_TRUE(cc::MatchesBitmap(actual_pixels, expected_pixels, comparator));
}

namespace {
class BackForwardTransitionAnimationManagerBrowserTestWithProgressBar
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
    web_contents()
        ->GetNativeView()
        ->GetWindowAndroid()
        ->set_progress_bar_config_for_testing(kConfig);
  }

  void ValidateNoProgressBar() {
    const auto* screenshot_layer = GetScreenshotLayer();
    EXPECT_EQ(screenshot_layer->children().size(), 1u);
  }

 protected:
  static constexpr ui::ProgressBarConfig kConfig = {
      .background_color = SkColors::kWhite,
      .height_physical = 10,
      .color = SkColors::kBlue,
      .hairline_color = SkColors::kWhite};
};
}  // namespace

// Tests that the progress bar is drawn at the correct position during the
// invoke phase.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestWithProgressBar,
    ProgressBar) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.3));
  ValidateNoProgressBar();

  GetAnimationManager()->OnGestureInvoked();
  {
    // Progress bar should be displayed when invoke animation starts.
    TestFuture<void> on_animate;
    GetAnimator()->set_next_on_animate_callback(on_animate.GetCallback());
    ASSERT_TRUE(on_animate.Wait())
        << "Timed out waiting for invoke animation to start";
    EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());
    const auto* progress_layer = GetProgressBarLayer();
    const int viewport_width = GetViewportSize().width();
    EXPECT_EQ(progress_layer->bounds(),
              gfx::Size(viewport_width, kConfig.height_physical));
  }

  {
    TestFuture<void> did_invoke;
    GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
    ASSERT_TRUE(did_invoke.Wait())
        << "Timed out waiting for invoke animation to finish";

    TestFuture<void> on_animate;
    GetAnimator()->set_next_on_animate_callback(on_animate.GetCallback());
    ASSERT_TRUE(on_animate.Wait())
        << "Timed out waiting for animation after invoke to start";

    // Progress bar should be removed.
    ValidateNoProgressBar();
  }

  TestFuture<void> on_destroyed;
  GetAnimator()->set_next_on_animate_callback(on_destroyed.GetCallback());
  ASSERT_TRUE(on_destroyed.Wait())
      << "Timed out waiting for animator to be destroyed";
}

class BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing() =
      default;
  ~BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing()
      override = default;

  void SetUp() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kQueueNavigationsWhileWaitingForCommit,
         {{"queueing_level", "full"}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpCommandLine(
        command_line);
    // Force --site-per-process because this test is testing races with
    // committing a navigation in a speculative `RenderFrameHost`.
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Assert that once the gesture navigation has sent the commit message to the
// renderer, the animation will not be cancelled.
//
// TODO(https://crbug.com/326256165): Re-enable this in a follow up.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing,
    DISABLED_QueuedNavigationNoCancel) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
  // We haven't started the navigation at this point.
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  // Set the interceptor, so we start the navigation to blue when the
  // DidCommit message to red has just arrived at the browser.
  CommitMessageDelayer delay_nav_to_red(
      web_contents(), RedURL(),
      base::BindOnce(
          [](WebContentsImpl* web_contents, const GURL& blue_url,
             RenderFrameHost* rfh) {
            web_contents->GetController().LoadURL(
                blue_url, Referrer{},
                ui::PageTransitionFromInt(
                    ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                    ui::PageTransition::PAGE_TRANSITION_TYPED),
                std::string{});
          },
          web_contents(), BlueURL()));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  // The user has lifted the finger - signaling the start of the navigation.
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Wait for the navigation to the blue page has started.
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  back_nav_to_red.ResumeNavigation();

  // Wait for the DidCommit message to red is intercepted, and then the
  // navigation to blue has started.
  delay_nav_to_red.Wait();
  // Pause the navigation to the blue page so we can let the committing red
  // navigation and its animations to finish.
  ASSERT_TRUE(nav_to_blue.WaitForRequestStart());

  // The start of navigation to the blue page means the history nav to the red
  // page has committed. Since the history nav to the red page has committed,
  // the animation manager must have brought the red page to the center of the
  // viewport.
  ASSERT_TRUE(back_nav_to_red.was_successful());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  ASSERT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRANSFORM_NEAR(kIdentityTransform, GetLivePageLayer()->transform(),
                        kFloatTolerance);

  // Wait for the navigation to the blue have finished.
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  ASSERT_TRUE(nav_to_blue.was_successful());

  // [red, blue]. The green NavigationEntry is pruned because we performed a
  // forward navigation from red to blue.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            BlueURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
}

namespace {

class BackForwardTransitionAnimationManagerWithRedirectBrowserTest
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerWithRedirectBrowserTest() = default;
  ~BackForwardTransitionAnimationManagerWithRedirectBrowserTest() override =
      default;

  void SetUpOnMainThread() override {
    SetupCrossSiteRedirector(embedded_test_server());
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerWithRedirectBrowserTest,
    AbortedOnCrossOriginRedirect) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  std::string different_host("b.com");
  GURL redirect = embedded_test_server()->GetURL(
      "/cross-site/" + different_host + "/empty.html");
  GURL expected_url =
      embedded_test_server()->GetURL(different_host, "/empty.html");

  // [red&, green*]
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  web_contents()->GetController().GetEntryAtIndex(0)->SetURL(redirect);

  TestNavigationManager redirect_nav(web_contents(), redirect);

  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(redirect_nav.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
  ASSERT_FALSE(did_invoke.IsReady());

  // [empty.html*, green&]
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            expected_url);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(1)->GetURL(),
            GreenURL());
}

// Assert that the navigation back to a site with an opaque origin is not
// considered as redirect. Such sites can be "chrome://newtabpage", "data:" or
// "file://".
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerWithRedirectBrowserTest,
    OpaqueOriginsAreNotRedirects) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  constexpr char kGreenDataURL[] = R"(
    data:text/html,<body style="background-color:green"></body>
  )";

  ASSERT_TRUE(NavigateToURL(web_contents(), GURL(kGreenDataURL)));
  WaitForCopyableViewInWebContents(web_contents());
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  WaitForCopyableViewInWebContents(web_contents());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationManager back_nav_to_data_url(web_contents(),
                                             GURL(kGreenDataURL));

  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(back_nav_to_data_url.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
}

namespace {

class BackForwardTransitionAnimationManagerBrowserTestSameDocument
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestSameDocument() = default;
  ~BackForwardTransitionAnimationManagerBrowserTestSameDocument() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable the vertical scroll bar, otherwise they might show up on the
    // screenshot, making the test flaky.
    command_line->AppendSwitch(switches::kHideScrollbars);
    BackForwardTransitionAnimationManagerBrowserTest::SetUpCommandLine(
        command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Load the red portion of the page.
    ASSERT_TRUE(NavigateToURL(web_contents(), embedded_test_server()->GetURL(
                                                  "/changing_color.html")));
    WaitForCopyableViewInWebContents(web_contents());

    auto* manager =
        BrowserContextImpl::From(web_contents()->GetBrowserContext())
            ->GetNavigationEntryScreenshotManager();
    ASSERT_TRUE(manager);
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());

    // Limit three screenshots.
    manager->SetMemoryBudgetForTesting(4 * GetViewportSize().Area64() * 3);

    auto& controller = web_contents()->GetController();
    // Navigate to the green portion of the page.
    const int num_request_before_nav =
        NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting();
    const int entries_count_before_nav = controller.GetEntryCount();
    {
      ScopedScreenshotCapturedObserverForTesting observer(
          controller.GetLastCommittedEntryIndex());
      ASSERT_TRUE(NavigateToURL(
          web_contents(),
          embedded_test_server()->GetURL("/changing_color.html#green")));
      observer.Wait();
    }
    ASSERT_EQ(controller.GetEntryCount(), entries_count_before_nav + 1);
    ASSERT_EQ(
        NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(),
        num_request_before_nav + 1);

    GetAnimationManager()->set_animator_factory_for_testing(
        std::make_unique<FactoryForTesting>(EmbedderBitmap()));
  }
};

}  // namespace

// Basic test for the animated transition on same-doc navigations. The
// transition is from a green portion of a page to a red portion of the same
// page.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSameDocument,
    SmokeTest) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_cross_fade;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      did_cross_fade.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationManager back_to_red(
      web_contents(), embedded_test_server()->GetURL("/changing_color.html"));
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_TRUE(did_invoke.IsReady());
  EXPECT_TRUE(did_cross_fade.IsReady());
}

namespace {
class BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions() =
      default;
  ~BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions()
      override = default;

  void SetUpOnMainThread() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
    // Load the main frame with title1.html. In all the tests the mainframe's
    // URL should always be title1.html. No subframe navigations can change
    // that.
    ASSERT_TRUE(NavigateToURL(web_contents(), MainFrameURL()));
    web_contents()->GetController().PruneAllButLastCommitted();
    AddIFrame(RedURL());
  }

  void AddIFrame(const GURL& url) {
    constexpr char kAddIframeScript[] = R"({
      (()=>{
          return new Promise((resolve) => {
            const frame = document.createElement('iframe');
            frame.addEventListener('load', () => {resolve();});
            frame.src = $1;
            document.body.appendChild(frame);
          });
      })();
    })";
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace(kAddIframeScript, url),
                       EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  FrameTreeNode* GetIFrameFrameTreeNodeAt(size_t child_index) {
    return web_contents()->GetPrimaryMainFrame()->child_at(child_index);
  }

  GURL MainFrameURL() const {
    return embedded_test_server()->GetURL("/title1.html");
  }
};
}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    SmokeTest) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  TestFuture<void> crossfade_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationObserver iframe_back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  iframe_back_to_red.Wait();
  ASSERT_TRUE(iframe_back_to_red.last_navigation_succeeded());
  ASSERT_EQ(iframe_back_to_red.last_navigation_url(), RedURL());
  ASSERT_TRUE(did_invoke.Wait());
  ASSERT_TRUE(crossfade_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());

  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());
}

// Test the iframe's renderer shows a prompt for the BeforeUnload message, and
// the user decides to proceed.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    BeforeUnload_Proceed_WithPrompt) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  // Note:
  // - We can't use `NavigateToURLFromRendererWithoutUserGesture()`. A subframe
  // navigation without a user gesture will make the navigation entry being
  // skipped on the back forward UI.
  // - `NavigateToURLFromRenderer()` will set the the sticky user activation bit
  // on the renderer.
  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  InjectBeforeUnload(iframe->current_frame_host());
  ASSERT_TRUE(iframe->current_frame_host()->HasStickyUserActivation());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  TestFuture<void> cancel_displayed;
  TestFuture<void> crossfade_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cancel_animation_displayed(
      cancel_displayed.GetCallback());
  GetAnimator()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(iframe->current_frame_host());
  TestNavigationObserver iframe_back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(cancel_displayed.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/true);
  iframe_back_to_red.Wait();
  ASSERT_TRUE(iframe_back_to_red.last_navigation_succeeded());
  ASSERT_EQ(iframe_back_to_red.last_navigation_url(), RedURL());

  ASSERT_TRUE(did_invoke.Wait());
  ASSERT_TRUE(crossfade_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());
  ASSERT_TRUE(dialog_observer.shown());
}

// Test that the user cancels the navigation via the prompt, after the cancel
// animation finishes.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    BeforeUnload_Cancel_AfterCancelAnimationFinishes) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  InjectBeforeUnload(iframe->current_frame_host());
  ASSERT_TRUE(iframe->current_frame_host()->HasStickyUserActivation());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  TestFuture<void> cancel_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cancel_animation_displayed(
      cancel_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(iframe->current_frame_host());
  TestNavigationObserver back_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(cancel_displayed.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/false);

  ASSERT_TRUE(destroyed.Wait());
  ASSERT_FALSE(back_to_red.last_navigation_succeeded());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_TRUE(dialog_observer.shown());
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), GreenURL());
}

// Test that the user cancels the navigation via the prompt, before the cancel
// animation finishes.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    BeforeUnload_Cancel_BeforeCancelAnimationFinishes) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  InjectBeforeUnload(iframe->current_frame_host());
  ASSERT_TRUE(iframe->current_frame_host()->HasStickyUserActivation());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  TestFuture<void> cancel_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cancel_animation_displayed(
      cancel_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(iframe->current_frame_host());
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager()->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimator()->UnpauseAnimation();

  ASSERT_TRUE(cancel_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_TRUE(dialog_observer.shown());
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), GreenURL());
}

// Test that when the user has decided not leave the current page by interacting
// with the prompt and the cancel animation is still playing, another navigation
// commits in the main frame. We should destroy the animator when the other
// navigation commits.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    BeforeUnload_RequestCancelledBeforeStart) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  InjectBeforeUnload(iframe->current_frame_host());
  ASSERT_TRUE(iframe->current_frame_host()->HasStickyUserActivation());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  bool cancel_played = false;
  GetAnimator()->set_on_cancel_animation_displayed(
      base::BindLambdaForTesting([&]() { cancel_played = true; }));
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(iframe->current_frame_host());
  TestNavigationObserver back_to_red(web_contents());
  GetAnimator()->set_duration_between_frames(base::Microseconds(1));
  GetAnimator()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager()->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kDisplayingCancelAnimation, GetAnimator()->state());
  // Expectation the animator will be destroyed while playing the cancel
  // animation.
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimator()->UnpauseAnimation();

  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());

  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_FALSE(cancel_played);
  ASSERT_TRUE(dialog_observer.shown());
}

// Test that the animator is behaving correctly, even after the renderer acks
// the BeforeUnload message to proceed (begin) the navigation, but
// `BeginNavigationImpl()` hits an early out so we never each
// `DidStartNavigation()`.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    BeforeUnload_BeginNavigationImplFails) {
  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());

  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  ASSERT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            MainFrameURL());

  InjectBeforeUnload(iframe->current_frame_host());
  ASSERT_TRUE(iframe->current_frame_host()->HasStickyUserActivation());

  std::vector<NavigationEntryImpl*> entries_before;
  for (int i = 0; i < web_contents()->GetController().GetEntryCount(); ++i) {
    entries_before.push_back(
        web_contents()->GetController().GetEntryAtIndex(i));
  }

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  // Fail the next `BeginNavigationImpl()`.
  FailBeginNavigationImpl fail_begin_navigation_client;

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  TestFuture<void> cancel_displayed;
  GetAnimator()->set_on_cancel_animation_displayed(
      cancel_displayed.GetCallback());

  BeforeUnloadDialogObserver dialog_observer(iframe->current_frame_host());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(cancel_displayed.Wait());
  dialog_observer.WaitForDialog();
  EXPECT_STATE_EQ(kWaitingForBeforeUnloadResponse, GetAnimator()->state());
  dialog_observer.RespondToDialogue(/*proceed=*/true);

  ASSERT_TRUE(destroyed.Wait());

  std::vector<NavigationEntryImpl*> entries_after;
  for (int i = 0; i < web_contents()->GetController().GetEntryCount(); ++i) {
    entries_after.push_back(web_contents()->GetController().GetEntryAtIndex(i));
  }
  ASSERT_THAT(entries_after, ::testing::ContainerEq(entries_before));
}

// If a primary main frame request is present, all subframe requests are
// ignored. Note: this is only possible when the main frame is navigating within
// the same document.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    OneMainFrameRequest_OneSubframeRequest) {
  auto& controller = web_contents()->GetController();

  // Navigate to the red portion of the document.
  // [..., red*]
  ASSERT_TRUE(NavigateToURL(web_contents(), embedded_test_server()->GetURL(
                                                "/changing_color.html#red")));
  WaitForCopyableViewInWebContents(web_contents());

  // Reset the list of entries.
  controller.PruneAllButLastCommitted();

  // Add a blue iframe at the red portion.
  // [red(blue)*]
  AddIFrame(BlueURL());

  // Navigate the iframe to another URL.
  // [red(blue), red(green)*]
  ASSERT_TRUE(NavigateToURLFromRenderer(
      GetIFrameFrameTreeNodeAt(0)->current_frame_host(), GreenURL()));
  WaitForCopyableViewInWebContents(web_contents());

  // Navigate to the green portion of the document.
  // [red(blue), red(green), green(green)*]
  {
    ScopedScreenshotCapturedObserverForTesting observer(
        controller.GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/changing_color.html#green")));
    observer.Wait();
  }

  ASSERT_EQ(controller.GetEntryCount(), 3);
  // Mark the middle entry as skipped.
  controller.GetEntryAtIndex(1)->set_should_skip_on_back_forward_ui(true);

  // Perform a back navigation from green(green) to red(blue), skipping
  // red(green) completely.
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> crossfade_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationManager back_to_red(
      web_contents(),
      embedded_test_server()->GetURL("/changing_color.html#red"));
  TestNavigationManager iframe_to_blue(web_contents(), BlueURL());
  GetAnimationManager()->OnGestureInvoked();

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(did_invoke.Wait());
  ASSERT_TRUE(crossfade_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());
  ASSERT_TRUE(iframe_to_blue.WaitForNavigationFinished());
}

// The mainframe, who embeds an iframe, is doing a cross-document navigation.
// We should animate the mainframe navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    MainFrameCrossDocNav_WithIFrame) {
  auto& controller = web_contents()->GetController();
  // [title1&, red*]
  {
    ScopedScreenshotCapturedObserverForTesting observer(
        controller.GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), RedURL()));
    observer.Wait();
  }
  // [title1&, red(blue)*]
  AddIFrame(BlueURL());
  // [title1&, red(blue), red(green)*]
  ASSERT_TRUE(NavigateToURLFromRenderer(
      GetIFrameFrameTreeNodeAt(0)->current_frame_host(), GreenURL()));

  ASSERT_EQ(controller.GetEntryCount(), 3);
  controller.GetEntryAtIndex(1)->set_should_skip_on_back_forward_ui(true);

  // Perform a back navigation from green(green) to title1, skipping red(blue).
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> crossfade_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationObserver mainframe_back_to_title1(
      web_contents(), /*expected_number_of_navigations=*/1);
  GetAnimationManager()->OnGestureInvoked();

  mainframe_back_to_title1.Wait();
  ASSERT_TRUE(mainframe_back_to_title1.last_navigation_succeeded());
  ASSERT_EQ(mainframe_back_to_title1.last_navigation_url(), MainFrameURL());
  ASSERT_TRUE(did_invoke.Wait());
  ASSERT_TRUE(crossfade_displayed.Wait());
  ASSERT_TRUE(destroyed.Wait());
}

// If a back navigation has more than one subframe requests and no main frame
// requests, the animated transition will be aborted.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    MultipleSubframeRequests) {
  auto& controller = web_contents()->GetController();
  // Reset the list of entries.
  controller.PruneAllButLastCommitted();

  // [main(red)*]
  auto* iframe1 = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe1->current_frame_host()->GetLastCommittedURL(), RedURL());

  // [main(red, green)*]: the initial navigation of the iframe won't create an
  // entry.
  AddIFrame(GreenURL());
  auto* iframe2 = GetIFrameFrameTreeNodeAt(1);
  ASSERT_EQ(iframe2->current_frame_host()->GetLastCommittedURL(), GreenURL());

  // [main(red, green), main(green, green)*]
  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe1->current_frame_host(), GreenURL()));
  // [main(red, green), main(green, green), main(green, blue)*]
  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe2->current_frame_host(), BlueURL()));

  ASSERT_EQ(controller.GetEntryCount(), 3);
  controller.GetEntryAtIndex(1)->set_should_skip_on_back_forward_ui(true);

  // Perform a back navigation from main(green, blue) to main(red, green),
  // skipping main(green, green) completely. This navigation will create two
  // subframe requests, and the animated transition will be aborted.
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> crossfade_displayed;
  TestFuture<AnimatorState> destroyed;
  TestFuture<void> did_invoke;
  GetAnimator()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());

  TestNavigationObserver back_nav(web_contents());
  GetAnimationManager()->OnGestureInvoked();

  back_nav.WaitForNavigationFinished();
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
  ASSERT_FALSE(did_invoke.IsReady());
  ASSERT_FALSE(crossfade_displayed.IsReady());

  ASSERT_EQ(controller.GetEntryCount(), 3);
  ASSERT_EQ(controller.GetLastCommittedEntryIndex(), 0);
}

// The transition of a subframe navigation will be cancelled if there is a main
// frame navigation.
//
// TODO(crbug/356417937): Flaky on bots. Reneable before Launch.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSubframeTransitions,
    DISABLED_MainframeNavCancelsSubframeTransition) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  auto* iframe = GetIFrameFrameTreeNodeAt(0);
  ASSERT_EQ(iframe->current_frame_host()->GetLastCommittedURL(), RedURL());
  ASSERT_TRUE(
      NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  GetAnimator()->set_subframe_navigation(true);

  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager iframe_back_to_red(web_contents(), RedURL());
  GetAnimator()->PauseAnimationAtDisplayingInvokeAnimation();
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(iframe_back_to_red.WaitForRequestStart());
  EXPECT_STATE_EQ(kDisplayingInvokeAnimation, GetAnimator()->state());

  GURL title2 = embedded_test_server()->GetURL("/title2.html");
  TestNavigationManager navigation_mainframe(web_contents(), title2);
  web_contents()->GetController().LoadURL(
      title2, Referrer{},
      ui::PageTransitionFromInt(
          ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
          ui::PageTransition::PAGE_TRANSITION_TYPED),
      std::string{});
  ASSERT_TRUE(navigation_mainframe.WaitForNavigationFinished());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
  ASSERT_FALSE(iframe_back_to_red.was_committed());
}

namespace {
class BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding() {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}},
        {blink::features::kIncrementLocalSurfaceIdForMainframeSameDocNavigation,
         {}},
        {features::kRenderDocument,
         {{kRenderDocumentLevelParameterName,
           GetRenderDocumentLevelName(RenderDocumentLevel::kAllFrames)}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding() override =
      default;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding,
    PaintHoldingDisabledOnTransition) {
  // Disable BFCache. Since RenderDocument is fully enabled (in the test
  // harness), the cross-doc navigation will have paint holding enabled.
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<bool> is_paint_holding_timer_running_when_nav_finishes;
  TestFuture<void> invoke_played;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_did_finish_navigation_callback(
      is_paint_holding_timer_running_when_nav_finishes.GetCallback());
  GetAnimator()->set_on_invoke_animation_displayed(invoke_played.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(is_paint_holding_timer_running_when_nav_finishes.Wait());
  EXPECT_FALSE(is_paint_holding_timer_running_when_nav_finishes.Get());
  ASSERT_TRUE(invoke_played.Wait());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());
}

// Test that the timer to dismiss the screenshot is properly started, if the
// renderer never submits a new frame post-navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding,
    ScreenshotDismissalTimer) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> invoke_played;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_invoke_animation_displayed(invoke_played.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);

  TestFrameNavigationObserver back_nav_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();
  back_nav_to_red.Wait();
  ASSERT_EQ(back_nav_to_red.last_committed_url(), RedURL());
  ASSERT_TRUE(back_nav_to_red.last_navigation_succeeded());
  ASSERT_TRUE(invoke_played.Wait());

  ASSERT_TRUE(
      GetAnimator()->dismiss_screenshot_timer_for_testing()->IsRunning());
  ASSERT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRUE(base::IsApproximatelyEqual(
      GetAnimator()->scrim_layer_for_testing()->background_color().fA, 0.f,
      kFloatTolerance));
  GetAnimator()->dismiss_screenshot_timer_for_testing()->FireNow();
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationAborted, destroyed.Get());
}

// Test that the timer to dismiss the screenshot is stopped once the renderer
// submits a new frame.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestNoPaintHolding,
    ScreenshotDismissalTimerStopped) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));

  TestFuture<void> invoke_played;
  TestFuture<AnimatorState> destroyed;
  GetAnimator()->set_on_invoke_animation_displayed(invoke_played.GetCallback());
  GetAnimator()->set_on_impl_destroyed(destroyed.GetCallback());
  GetAnimator()->set_intercept_render_frame_metadata_changed(true);

  TestFrameNavigationObserver back_nav_to_red(web_contents());
  GetAnimationManager()->OnGestureInvoked();
  back_nav_to_red.Wait();
  ASSERT_EQ(back_nav_to_red.last_committed_url(), RedURL());
  ASSERT_TRUE(back_nav_to_red.last_navigation_succeeded());
  ASSERT_TRUE(invoke_played.Wait());

  ASSERT_TRUE(
      GetAnimator()->dismiss_screenshot_timer_for_testing()->IsRunning());
  ASSERT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRUE(base::IsApproximatelyEqual(
      GetAnimator()->scrim_layer_for_testing()->background_color().fA, 0.f,
      kFloatTolerance));

  cc::RenderFrameMetadata metadata;
  metadata.primary_main_frame_item_sequence_number =
      GetAnimator()->primary_main_frame_navigation_entry_item_sequence_number();
  web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderWidgetHost()
      ->render_frame_metadata_provider()
      ->SetLastRenderFrameMetadataForTest(metadata);

  GetAnimator()->set_intercept_render_frame_metadata_changed(false);
  GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(base::TimeTicks());

  EXPECT_FALSE(
      GetAnimator()->dismiss_screenshot_timer_for_testing()->IsRunning());
  ASSERT_TRUE(destroyed.Wait());
  EXPECT_STATE_EQ(kAnimationFinished, destroyed.Get());
}

namespace {

class BackForwardTransitionAnimationManagerBrowserTestEmbedderLiveContent
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  SkBitmap EmbedderBitmap() override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10, true);
    bitmap.eraseColor(SkColors::kRed);
    bitmap.setImmutable();
    return bitmap;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestEmbedderLiveContent,
    Cancel_EmbedderScreenshot) {
  TestFuture<void> did_cancel;

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  GetAnimator()->set_on_cancel_animation_displayed(did_cancel.GetCallback());
  GetAnimationManager()->OnGestureCancelled();
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  ASSERT_TRUE(did_cancel.Wait());
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  EXPECT_EQ(GetAnimationManager()->GetCurrentAnimationStage(),
            AnimationStage::kWaitingForEmbedderContentForCommittedEntry);
  TestFuture<AnimatorForTesting::State> did_finish;
  GetAnimator()->set_on_impl_destroyed(did_finish.GetCallback());

  GetAnimationManager()->OnContentForNavigationEntryShown();

  EXPECT_EQ(did_finish.Get(), AnimatorForTesting::State::kAnimationFinished);
  EXPECT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestEmbedderLiveContent,
    Invoke_EmbedderScreenshot) {
  TestFuture<void> did_invoke;

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6));
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  GetAnimator()->set_intercept_render_frame_metadata_changed(true);
  GetAnimator()->set_on_invoke_animation_displayed(did_invoke.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  EXPECT_EQ("[Screenshot[Scrim],LivePage,EmbedderContentLayer]",
            ChildrenInOrder(*GetViewLayer()));

  ASSERT_TRUE(did_invoke.Wait());
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  GetAnimator()->set_intercept_render_frame_metadata_changed(false);
  base::TimeTicks now = base::TimeTicks();
  GetAnimator()->OnRenderFrameMetadataChangedAfterActivation(now);

  TestFuture<AnimatorForTesting::State> did_finish;
  GetAnimator()->set_on_impl_destroyed(did_finish.GetCallback());
  EXPECT_EQ(did_finish.Get(), AnimatorForTesting::State::kAnimationFinished);
  EXPECT_EQ("[LivePage]", ChildrenInOrder(*GetViewLayer()));
}

namespace {

using ModalDialogType = ui::ModalDialogManagerBridge::ModalDialogType;

// Note: This set of tests are unittesting ModalDialogManager#suspendType()
// and ModalDialogManager#resumeType(), as `ShellJavaScriptDialogManager` is
// not wired to Java UI code.
class BackForwardTransitionAnimationManagerBrowserTestSuspendDialog
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  ~BackForwardTransitionAnimationManagerBrowserTestSuspendDialog() override =
      default;

  void SetUpOnMainThread() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
    fake_dialog_manager_ = ui::FakeModalDialogManagerBridge::CreateForTab(
        web_contents()->GetTopLevelNativeWindow(),
        /*use_empty_java_presenter=*/true);
  }

  void TearDown() override {
    // If the tests are skipped on emulators, `fake_dialog_manager()` is null.
    if (fake_dialog_manager()) {
      EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
      EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));
    }
  }

  ui::FakeModalDialogManagerBridge* fake_dialog_manager() {
    return fake_dialog_manager_.get();
  }

 private:
  std::unique_ptr<ui::FakeModalDialogManagerBridge> fake_dialog_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSuspendDialog,
    JavascriptDialogSuspendedDuringTransition) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  TestFuture<AnimatorForTesting::State> on_destroyed;
  GetAnimator()->set_on_impl_destroyed(on_destroyed.GetCallback());
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6f));
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForRequestStart());
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_red.was_successful());
  // The navigation has finished. Tab level dialogs are resumed immediately.
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));
  ASSERT_TRUE(on_destroyed.Wait());
  EXPECT_EQ(on_destroyed.Get(), kAnimationFinished);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSuspendDialog,
    JavascriptDialogResumedOnCancel) {
  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  TestFuture<AnimatorForTesting::State> on_destroyed;
  GetAnimator()->set_on_impl_destroyed(on_destroyed.GetCallback());
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6f));
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureCancelled();
  // Tab level dialogs are resumed immediately after the gesture is cancelled.
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));
  ASSERT_TRUE(on_destroyed.Wait());
  EXPECT_EQ(on_destroyed.Get(), kAnimationFinished);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSuspendDialog,
    JavascriptDialogResumedOnAbort) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));

  TestFuture<AnimatorForTesting::State> on_destroyed;
  GetAnimator()->set_on_impl_destroyed(on_destroyed.GetCallback());
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6f));
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForRequestStart());
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnDetachCompositor();
  ASSERT_TRUE(on_destroyed.Wait());
  EXPECT_EQ(on_destroyed.Get(), kAnimationAborted);
  // Dialogs are resumed when the transition is aborted.
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));
}

// For subframe navigations, we resume the dialogs as soon as the subframe
// navigation starts.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSuspendDialog,
    JavascriptDialogNotSuppressedForSubframeNavs) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  FrameTreeNode* iframe = nullptr;
  {
    ASSERT_TRUE(NavigateToURL(web_contents(), RedURL()));
    web_contents()->GetController().PruneAllButLastCommitted();
    static constexpr char kAddIframeScript[] = R"({
      (()=>{
          return new Promise((resolve) => {
            const frame = document.createElement('iframe');
            frame.addEventListener('load', () => {resolve();});
            frame.src = $1;
            document.body.appendChild(frame);
          });
      })();
    })";
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace(kAddIframeScript, BlueURL())));
    ASSERT_EQ(
        web_contents()->GetPrimaryMainFrame()->frame_tree_node()->child_count(),
        1u);
    iframe =
        web_contents()->GetPrimaryMainFrame()->frame_tree_node()->child_at(0);
    ASSERT_TRUE(
        NavigateToURLFromRenderer(iframe->current_frame_host(), GreenURL()));
  }

  // [red(blue), red(green)*] -> [red(blue)*, red(green)]
  TestNavigationManager iframe_back_to_blue(web_contents(), BlueURL());

  GetAnimationManager()->OnGestureStarted(ui::BackGestureEvent(0),
                                          SwipeEdge::LEFT, NavType::kBackward);
  EXPECT_EQ("[Screenshot[Scrim],LivePage]", ChildrenInOrder(*GetViewLayer()));
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnGestureProgressed(ui::BackGestureEvent(0.6f));
  EXPECT_TRUE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  TestFuture<AnimatorForTesting::State> on_destroyed;
  GetAnimator()->set_on_impl_destroyed(on_destroyed.GetCallback());
  GetAnimationManager()->OnGestureInvoked();
  ASSERT_TRUE(iframe_back_to_blue.WaitForRequestStart());
  // As soon as `OnGestureInvoked()` is called the dialogs are resumed.
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));

  GetAnimationManager()->OnDetachCompositor();
  ASSERT_TRUE(on_destroyed.Wait());
  EXPECT_EQ(on_destroyed.Get(), kAnimationAborted);
  // Dialogs are resumed when the transition is aborted.
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kTab));
  EXPECT_FALSE(fake_dialog_manager()->IsSuspend(ModalDialogType::kApp));
}

}  // namespace content
