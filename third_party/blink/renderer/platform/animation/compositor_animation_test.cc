// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

#include <memory>

#include "base/time/time.h"
#include "cc/animation/animation_id_provider.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
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

  void NotifyAnimationStarted(base::TimeDelta, int) override {
    started_ = true;
  }
  void NotifyAnimationFinished(base::TimeDelta, int) override {
    finished_ = true;
  }
  void NotifyAnimationAborted(base::TimeDelta, int) override {
    aborted_ = true;
  }

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

  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  std::unique_ptr<CompositorAnimationTestClient> client(
      new CompositorAnimationTestClient);
  CompositorAnimation* animation = client->GetCompositorAnimation();
  cc::Animation* cc_animation = animation->CcAnimation();
  timeline->AnimationAttached(*client);
  int timeline_id = cc_animation->animation_timeline()->id();

  auto curve = gfx::KeyframedFloatAnimationCurve::Create();
  auto keyframe_model = cc::KeyframeModel::Create(
      std::move(curve), cc::AnimationIdProvider::NextKeyframeModelId(), 1,
      cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::TRANSFORM));
  int keyframe_model_id = keyframe_model->id();
  animation->AddKeyframeModel(std::move(keyframe_model));

  animation->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_animation->NotifyKeyframeModelFinishedForTesting(
      timeline_id, keyframe_model_id, cc::TargetProperty::TRANSFORM, 1);
  EXPECT_TRUE(delegate->finished_);

  delegate->ResetFlags();

  animation->SetAnimationDelegate(nullptr);
  cc_animation->NotifyKeyframeModelFinishedForTesting(
      timeline_id, keyframe_model_id, cc::TargetProperty::TRANSFORM, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationTest, NotifyFromCCAfterCompositorAnimationDeletion) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  std::unique_ptr<CompositorAnimationTestClient> client(
      new CompositorAnimationTestClient);
  CompositorAnimation* animation = client->GetCompositorAnimation();
  scoped_refptr<cc::Animation> cc_animation = animation->CcAnimation();
  timeline->AnimationAttached(*client);
  int timeline_id = cc_animation->animation_timeline()->id();

  auto curve = gfx::KeyframedFloatAnimationCurve::Create();
  auto keyframe_model = cc::KeyframeModel::Create(
      std::move(curve), cc::AnimationIdProvider::NextKeyframeModelId(), 1,
      cc::KeyframeModel::TargetPropertyId(cc::TargetProperty::OPACITY));
  int keyframe_model_id = keyframe_model->id();
  animation->AddKeyframeModel(std::move(keyframe_model));

  animation->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_animation->NotifyKeyframeModelFinishedForTesting(
      timeline_id, keyframe_model_id, cc::TargetProperty::OPACITY, 1);
  EXPECT_TRUE(delegate->finished_);
  delegate->finished_ = false;

  // Delete CompositorAnimation. ccAnimation stays alive.
  client = nullptr;

  // No notifications. Doesn't crash.
  cc_animation->NotifyKeyframeModelFinishedForTesting(
      timeline_id, keyframe_model_id, cc::TargetProperty::OPACITY, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationTest,
       CompositorAnimationDeletionDetachesFromCCTimeline) {
  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  std::unique_ptr<CompositorAnimationTestClient> client(
      new CompositorAnimationTestClient);

  scoped_refptr<cc::AnimationTimeline> cc_timeline =
      timeline->GetAnimationTimeline();
  scoped_refptr<cc::Animation> cc_animation = client->animation_->CcAnimation();
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
