// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

#include <memory>

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/testing/compositor_test.h"

namespace blink {

class CompositorAnimationDelegateForTesting
    : public CompositorAnimationDelegate {
 public:
  CompositorAnimationDelegateForTesting() { ResetFlags(); }

  void ResetFlags() {
    started_ = false;
    finished_ = false;
    aborted_ = false;
  }

  void NotifyAnimationStarted(double, int) override { started_ = true; }
  void NotifyAnimationFinished(double, int) override { finished_ = true; }
  void NotifyAnimationAborted(double, int) override { aborted_ = true; }

  bool started_;
  bool finished_;
  bool aborted_;
};

class CompositorAnimationTestClient : public CompositorAnimationClient {
 public:
  CompositorAnimationTestClient() : animation_(CompositorAnimation::Create()) {}

  CompositorAnimation* GetCompositorAnimation() const override {
    return animation_.get();
  }

  std::unique_ptr<CompositorAnimation> animation_;
};

class CompositorAnimationTest : public CompositorTest {};

// Test that when the animation delegate is null, the animation animation
// doesn't forward the finish notification.
TEST_F(CompositorAnimationTest, NullDelegate) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimation> animation =
      CompositorAnimation::Create();
  cc::SingleKeyframeEffectAnimation* cc_animation = animation->CcAnimation();

  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  auto keyframe_model = std::make_unique<CompositorKeyframeModel>(
      *curve, compositor_target_property::TRANSFORM, 0, 1);
  animation->AddKeyframeModel(std::move(keyframe_model));

  animation->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_animation->NotifyKeyframeModelFinishedForTesting(
      compositor_target_property::TRANSFORM, 1);
  EXPECT_TRUE(delegate->finished_);

  delegate->ResetFlags();

  animation->SetAnimationDelegate(nullptr);
  cc_animation->NotifyKeyframeModelFinishedForTesting(
      compositor_target_property::TRANSFORM, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationTest, NotifyFromCCAfterCompositorAnimationDeletion) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimation> animation =
      CompositorAnimation::Create();
  scoped_refptr<cc::SingleKeyframeEffectAnimation> cc_animation =
      animation->CcAnimation();

  auto curve = std::make_unique<CompositorFloatAnimationCurve>();
  auto keyframe_model = std::make_unique<CompositorKeyframeModel>(
      *curve, compositor_target_property::OPACITY, 0, 1);
  animation->AddKeyframeModel(std::move(keyframe_model));

  animation->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_animation->NotifyKeyframeModelFinishedForTesting(
      compositor_target_property::OPACITY, 1);
  EXPECT_TRUE(delegate->finished_);
  delegate->finished_ = false;

  // Delete CompositorAnimation. ccAnimation stays alive.
  animation = nullptr;

  // No notifications. Doesn't crash.
  cc_animation->NotifyKeyframeModelFinishedForTesting(
      compositor_target_property::OPACITY, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationTest,
       CompositorAnimationDeletionDetachesFromCCTimeline) {
  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  std::unique_ptr<CompositorAnimationTestClient> client(
      new CompositorAnimationTestClient);

  scoped_refptr<cc::AnimationTimeline> cc_timeline =
      timeline->GetAnimationTimeline();
  scoped_refptr<cc::SingleKeyframeEffectAnimation> cc_animation =
      client->animation_->CcAnimation();
  EXPECT_FALSE(cc_animation->animation_timeline());

  timeline->AnimationAttached(*client);
  EXPECT_TRUE(cc_animation->animation_timeline());
  EXPECT_TRUE(cc_timeline->GetAnimationById(cc_animation->id()));

  // Delete client and CompositorAnimation while attached to timeline.
  client = nullptr;

  EXPECT_FALSE(cc_animation->animation_timeline());
  EXPECT_FALSE(cc_timeline->GetAnimationById(cc_animation->id()));
}

}  // namespace blink
