// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_controller_android.h"
#include <memory>
#include "base/memory/raw_ptr.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/android/overscroll_glow.h"
#include "ui/android/overscroll_refresh.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/window_android_compositor.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/did_overscroll_params.h"

using ::testing::_;
using ::testing::Return;
using ui::EdgeEffect;
using ui::OverscrollGlow;
using ui::OverscrollGlowClient;
using ui::OverscrollRefresh;
using ui::ResourceManager;
using ui::WindowAndroidCompositor;

namespace content {

namespace {

class MockCompositor : public WindowAndroidCompositor {
 public:
  ~MockCompositor() override = default;
  ui::WindowAndroidCompositor::ScopedKeepSurfaceAliveCallback
  TakeScopedKeepSurfaceAliveCallback(const viz::SurfaceId&) override {
    return base::OnceClosure();
  }
  void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<viz::CopyOutputRequest>) override {}
  void SetNeedsAnimate() override {}
  MOCK_METHOD0(GetResourceManager, ResourceManager&());
  MOCK_METHOD0(GetFrameSinkId, viz::FrameSinkId());
  void AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) override {}
  void RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) override {}
  bool IsDrawingFirstVisibleFrame() const override { return false; }
  void SetVSyncPaused(bool paused) override {}
  void OnUpdateRefreshRate(float refresh_rate) override {}
  void OnUpdateSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) override {}
  std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      base::TimeDelta timeout) override {
    return nullptr;
  }
  void OnUpdateOverlayTransform() override {}
  void PostRequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulPresentationTimeCallback callback) override {}
  void AddFrameSubmissionObserver(FrameSubmissionObserver* observer) override {}
  void RemoveFrameSubmissionObserver(
      FrameSubmissionObserver* observer) override {}
};

class MockGlowClient : public OverscrollGlowClient {
 public:
  MOCK_METHOD0(CreateEdgeEffect, std::unique_ptr<EdgeEffect>());
};

class MockGlow : public OverscrollGlow {
 public:
  MockGlow() : OverscrollGlow(new MockGlowClient()) {}
  MOCK_METHOD5(OnOverscrolled,
               bool(base::TimeTicks,
                    gfx::Vector2dF,
                    gfx::Vector2dF,
                    gfx::Vector2dF,
                    gfx::Vector2dF));
};

class MockRefresh : public OverscrollRefresh {
 public:
  MockRefresh() : OverscrollRefresh() {}
  MOCK_METHOD1(OnOverscrolled, void(const cc::OverscrollBehavior& behavior));
  MOCK_METHOD0(Reset, void());
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_CONST_METHOD0(IsAwaitingScrollUpdateAck, bool());
};

class OverscrollControllerAndroidUnitTest : public testing::Test {
 public:
  OverscrollControllerAndroidUnitTest() {
    dip_scale_ = 560;
    std::unique_ptr<MockGlow> glow_ptr = std::make_unique<MockGlow>();
    std::unique_ptr<MockRefresh> refresh_ptr = std::make_unique<MockRefresh>();
    compositor_ = std::make_unique<MockCompositor>();
    glow_ = glow_ptr.get();
    refresh_ = refresh_ptr.get();
    controller_ = OverscrollControllerAndroid::CreateForTests(
        compositor_.get(), dip_scale_, std::move(glow_ptr),
        std::move(refresh_ptr));
  }

  ui::DidOverscrollParams CreateVerticalOverscrollParams() {
    ui::DidOverscrollParams params;
    params.accumulated_overscroll = gfx::Vector2dF(0, 1);
    params.latest_overscroll_delta = gfx::Vector2dF(0, 1);
    params.current_fling_velocity = gfx::Vector2dF(0, 1);
    params.causal_event_viewport_point = gfx::PointF(100, 100);
    params.accumulated_overscroll.Scale(dip_scale_);
    params.latest_overscroll_delta.Scale(dip_scale_);
    params.current_fling_velocity.Scale(dip_scale_);
    params.causal_event_viewport_point.Scale(dip_scale_);

    return params;
  }

 protected:
  raw_ptr<MockGlow> glow_;
  raw_ptr<MockRefresh> refresh_;
  std::unique_ptr<MockCompositor> compositor_;
  std::unique_ptr<OverscrollControllerAndroid> controller_;
  float dip_scale_;
};

TEST_F(OverscrollControllerAndroidUnitTest,
       OverscrollBehaviorYAutoAllowsRefresh) {
  ui::DidOverscrollParams params = CreateVerticalOverscrollParams();
  params.overscroll_behavior.y = cc::OverscrollBehavior::Type::kAuto;

  // Test that refresh is activated but glow is not rendered.
  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(true));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).Times(0);
  EXPECT_CALL(*glow_, OnOverscrolled(_, _, _, _, _)).Times(0);

  controller_->OnOverscrolled(params);
  testing::Mock::VerifyAndClearExpectations(&refresh_);
}

TEST_F(OverscrollControllerAndroidUnitTest,
       OverscrollBehaviorYContainAllowsGlowOnly) {
  ui::DidOverscrollParams params = CreateVerticalOverscrollParams();
  params.overscroll_behavior.y = cc::OverscrollBehavior::Type::kContain;

  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(false));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).WillOnce(Return(false));
  EXPECT_CALL(*glow_,
              OnOverscrolled(_, gfx::Vector2dF(0, 560), gfx::Vector2dF(0, 560),
                             gfx::Vector2dF(0, 560), _));

  controller_->OnOverscrolled(params);
  testing::Mock::VerifyAndClearExpectations(refresh_);
  testing::Mock::VerifyAndClearExpectations(glow_);

  // Test that the "contain" set on x-axis would not affect navigation.
  params.overscroll_behavior.y = cc::OverscrollBehavior::Type::kAuto;
  params.overscroll_behavior.x = cc::OverscrollBehavior::Type::kContain;

  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(true));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).Times(0);
  EXPECT_CALL(*glow_, OnOverscrolled(_, _, _, _, _)).Times(0);

  controller_->OnOverscrolled(params);
  testing::Mock::VerifyAndClearExpectations(refresh_);
  testing::Mock::VerifyAndClearExpectations(glow_);
}

TEST_F(OverscrollControllerAndroidUnitTest,
       OverscrollBehaviorYNonePreventsGlowAndRefresh) {
  ui::DidOverscrollParams params = CreateVerticalOverscrollParams();
  params.overscroll_behavior.y = cc::OverscrollBehavior::Type::kNone;

  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(false));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).WillOnce(Return(false));
  EXPECT_CALL(*glow_, OnOverscrolled(_, gfx::Vector2dF(), gfx::Vector2dF(),
                                     gfx::Vector2dF(), _));

  controller_->OnOverscrolled(params);
  testing::Mock::VerifyAndClearExpectations(refresh_);
  testing::Mock::VerifyAndClearExpectations(glow_);

  // Test that the "none" set on y-axis would not affect glow on x-axis.
  params.accumulated_overscroll = gfx::Vector2dF(1, 1);
  params.latest_overscroll_delta = gfx::Vector2dF(1, 1);
  params.current_fling_velocity = gfx::Vector2dF(1, 1);
  params.accumulated_overscroll.Scale(dip_scale_);
  params.latest_overscroll_delta.Scale(dip_scale_);
  params.current_fling_velocity.Scale(dip_scale_);

  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(false));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).WillOnce(Return(false));
  EXPECT_CALL(*glow_,
              OnOverscrolled(_, gfx::Vector2dF(560, 0), gfx::Vector2dF(560, 0),
                             gfx::Vector2dF(560, 0), _));

  controller_->OnOverscrolled(params);
  testing::Mock::VerifyAndClearExpectations(refresh_);
  testing::Mock::VerifyAndClearExpectations(glow_);
}

TEST_F(OverscrollControllerAndroidUnitTest,
       ConsumedUpdateDoesNotResetEnabledRefresh) {
  ui::DidOverscrollParams params = CreateVerticalOverscrollParams();
  params.overscroll_behavior.y = cc::OverscrollBehavior::Type::kAuto;

  EXPECT_CALL(*refresh_, IsActive()).WillOnce(Return(true));
  EXPECT_CALL(*refresh_, IsAwaitingScrollUpdateAck()).WillOnce(Return(false));

  // Enable the refresh effect.
  controller_->OnOverscrolled(params);

  // Generate a consumed scroll update.
  blink::WebGestureEvent event(blink::WebInputEvent::Type::kGestureScrollUpdate,
                               blink::WebInputEvent::kNoModifiers,
                               ui::EventTimeForNow());
  controller_->OnGestureEventAck(
      event, blink::mojom::InputEventResultState::kConsumed);

  testing::Mock::VerifyAndClearExpectations(&refresh_);
}

}  // namespace

}  // namespace content
