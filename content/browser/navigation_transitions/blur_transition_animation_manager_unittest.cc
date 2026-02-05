// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/blur_transition_animation_manager.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/size.h"

namespace content {

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace {

class MockWebContentsViewAndroidDelegate
    : public BlurTransitionAnimationManager::WebContentsViewAndroidDelegate {
 public:
  MockWebContentsViewAndroidDelegate() = default;
  ~MockWebContentsViewAndroidDelegate() override = default;

  MOCK_METHOD(bool,
              ShouldShowBlurTransitionAnimation,
              (NavigationHandle*),
              (override));
  MOCK_METHOD(BackForwardTransitionAnimationManager*,
              GetBackForwardTransitionAnimationManager,
              (),
              (override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (override));
  MOCK_METHOD(ui::WindowAndroid*, GetWindowAndroid, (), (override));
  MOCK_METHOD(viz::SurfaceId, GetCurrentSurfaceId, (), (override));
};

class MockBackForwardTransitionAnimationManager
    : public BackForwardTransitionAnimationManager {
 public:
  MOCK_METHOD(void,
              OnGestureStarted,
              (const ui::BackGestureEvent& event,
               ui::BackGestureEventSwipeEdge edge,
               NavigationDirection navigation_direction),
              (override));
  MOCK_METHOD(void,
              OnGestureProgressed,
              (const ui::BackGestureEvent& event),
              (override));
  MOCK_METHOD(void, OnGestureCancelled, (), (override));
  MOCK_METHOD(void, OnGestureInvoked, (), (override));
  MOCK_METHOD(void, OnContentForNavigationEntryShown, (), (override));
  MOCK_METHOD(AnimationStage, GetCurrentAnimationStage, (), (override));
  MOCK_METHOD(void, SetFavicon, (const SkBitmap& bitmap), (override));
};

}  // namespace

class TestBlurTransitionAnimationManager
    : public BlurTransitionAnimationManager {
 public:
  explicit TestBlurTransitionAnimationManager(WebContents* web_contents)
      : BlurTransitionAnimationManager(web_contents) {}

  void SetDelegate(std::unique_ptr<WebContentsViewAndroidDelegate> delegate) {
    web_contents_view_android_delegate_ = std::move(delegate);
  }
};

class BlurTransitionAnimationManagerTest : public RenderViewHostTestHarness {
 public:
  BlurTransitionAnimationManagerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    view_android_ =
        std::make_unique<ui::ViewAndroid>(ui::ViewAndroid::LayoutType::kNormal);
    view_android_->SetLayer(cc::slim::Layer::Create());

    auto manager =
        std::make_unique<TestBlurTransitionAnimationManager>(web_contents());
    manager_ = manager.get();

    auto mock_delegate =
        std::make_unique<NiceMock<MockWebContentsViewAndroidDelegate>>();
    mock_delegate_ = mock_delegate.get();

    ON_CALL(*mock_delegate_, GetNativeView())
        .WillByDefault(Return(view_android_.get()));
    // Set a valid SurfaceId for the mock to return.
    fake_surface_id_ = viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
    ON_CALL(*mock_delegate_, GetCurrentSurfaceId())
        .WillByDefault(Return(fake_surface_id_));

    manager_->SetDelegate(std::move(mock_delegate));
    web_contents()->SetUserData(BlurTransitionAnimationManager::UserDataKey(),
                                std::move(manager));
  }

  void TearDown() override {
    mock_delegate_ = nullptr;
    web_contents()->RemoveUserData(
        BlurTransitionAnimationManager::UserDataKey());
    view_android_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  using AnimationState = BlurTransitionAnimationManager::AnimationState;
  using TransitionExitReason =
      BlurTransitionAnimationManager::TransitionExitReason;

  AnimationState GetAnimationState() const {
    return manager_->animation_state_;
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ui::ViewAndroid> view_android_;
  raw_ptr<MockWebContentsViewAndroidDelegate> mock_delegate_;
  raw_ptr<TestBlurTransitionAnimationManager> manager_;
  viz::SurfaceId fake_surface_id_;
};

TEST_F(BlurTransitionAnimationManagerTest, SuccessfulLayerCreation) {
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents());
  simulator->Start();

  EXPECT_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_delegate_, GetBackForwardTransitionAnimationManager())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_delegate_, GetCurrentSurfaceId())
      .WillOnce(Return(fake_surface_id_));

  simulator->ReadyToCommit();

  // Layer should be created immediately.
  const auto& children = view_android_->GetLayer()->children();
  ASSERT_EQ(children.size(), 1u);

  EXPECT_EQ(GetAnimationState(), AnimationState::kFadeIn);
}

TEST_F(BlurTransitionAnimationManagerTest,
       CorrectlyFadesOutBlurIfFirstPaintHappensDuringFadeIn) {
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents());
  simulator->Start();

  EXPECT_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_delegate_, GetCurrentSurfaceId())
      .WillOnce(Return(fake_surface_id_));

  simulator->ReadyToCommit();

  EXPECT_EQ(GetAnimationState(), AnimationState::kFadeIn);
  EXPECT_EQ(view_android_->GetLayer()->children().size(), 1u);

  // Trigger paint.
  manager_->DidFirstVisuallyNonEmptyPaint();

  EXPECT_EQ(GetAnimationState(), AnimationState::kFadeOut);

  // Manually drive the animation.
  manager_->OnAnimate(base::TimeTicks::Now() + base::Milliseconds(200));

  EXPECT_EQ(GetAnimationState(), AnimationState::kNone);
  EXPECT_TRUE(view_android_->GetLayer()->children().empty());

  histogram_tester_.ExpectUniqueSample(
      "Navigation.BlurTransitionAnimation.ExitReason",
      TransitionExitReason::kFinished, 1);
}

// Verifies that if a new navigation starts before the previous blur
// transition has finished, the old transition is correctly interrupted and
// cleaned up.
TEST_F(BlurTransitionAnimationManagerTest, StaleNavigationProtection) {
  // Navigation 1.
  auto sim1 = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/1"), web_contents());
  sim1->Start();
  EXPECT_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillOnce(Return(true));
  sim1->ReadyToCommit();

  EXPECT_EQ(view_android_->GetLayer()->children().size(), 1u);

  // Navigation 2 starts immediately (superseding 1).
  auto sim2 = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/2"), web_contents());
  sim2->Start();
  EXPECT_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillOnce(Return(true));
  sim2->ReadyToCommit();

  // The first layer should be removed and replaced by the second one.
  EXPECT_EQ(view_android_->GetLayer()->children().size(), 1u);

  // The first navigation was interrupted.
  histogram_tester_.ExpectBucketCount(
      "Navigation.BlurTransitionAnimation.ExitReason",
      TransitionExitReason::kNavigationInterrupted, 1);
}

TEST_F(BlurTransitionAnimationManagerTest,
       NoAnimationOnSameDocumentNavigation) {
  ON_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillByDefault(Return(true));

  MockNavigationHandle handle;
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(true);

  EXPECT_CALL(*mock_delegate_, GetCurrentSurfaceId()).Times(0);
  manager_->ReadyToCommitNavigation(&handle);
}

TEST_F(BlurTransitionAnimationManagerTest, NoAnimationOnBFCacheRestore) {
  ON_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillByDefault(Return(true));

  MockNavigationHandle handle;
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  handle.set_is_served_from_bfcache(true);

  EXPECT_CALL(*mock_delegate_, GetCurrentSurfaceId()).Times(0);
  manager_->ReadyToCommitNavigation(&handle);
}

TEST_F(BlurTransitionAnimationManagerTest,
       NoAnimationWhenBackForwardGestureIsActive) {
  ON_CALL(*mock_delegate_, ShouldShowBlurTransitionAnimation(_))
      .WillByDefault(Return(true));

  MockNavigationHandle handle;
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  handle.set_is_served_from_bfcache(false);

  StrictMock<MockBackForwardTransitionAnimationManager> mock_anim_manager;
  EXPECT_CALL(mock_anim_manager, GetCurrentAnimationStage())
      .WillRepeatedly(Return(BackForwardTransitionAnimationManager::
                                 AnimationStage::kInvokeAnimation));
  EXPECT_CALL(*mock_delegate_, GetBackForwardTransitionAnimationManager())
      .WillRepeatedly(Return(&mock_anim_manager));

  EXPECT_CALL(*mock_delegate_, GetCurrentSurfaceId()).Times(0);
  manager_->ReadyToCommitNavigation(&handle);
}

}  // namespace content
