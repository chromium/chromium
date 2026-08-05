// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_or_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanordomstringparameters_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_camera_element.h"
#include "third_party/blink/renderer/core/html/html_microphone_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/media_capture_element_constraints.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class MediaTrackElementConstraintsTest : public PageTestBase {};

TEST_F(MediaTrackElementConstraintsTest, SetConstraintsStoresValue) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* camera = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  MediaTrackConstraintSet* constraints = MediaTrackConstraintSet::Create();

  MediaCaptureElementConstraints::setConstraints(*camera, constraints);

  EXPECT_TRUE(MediaCaptureElementConstraints::From(*camera).Constraints());
}

TEST_F(MediaTrackElementConstraintsTest, SetConstraintsOnlySetsOnce) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* camera = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  MediaTrackConstraintSet* constraints1 = MediaTrackConstraintSet::Create();
  constraints1->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(480));

  MediaCaptureElementConstraints::setConstraints(*camera, constraints1);
  const HTMLMediaStreamConstraints* sanitized =
      MediaCaptureElementConstraints::From(*camera).Constraints();

  EXPECT_TRUE(sanitized);

  MediaTrackConstraintSet* constraints2 = MediaTrackConstraintSet::Create();
  constraints2->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(1080));

  MediaCaptureElementConstraints::setConstraints(*camera, constraints2);
  EXPECT_EQ(MediaCaptureElementConstraints::From(*camera).Constraints(),
            sanitized);
}

TEST_F(MediaTrackElementConstraintsTest, SanitizeTrackConstraints) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* camera = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  MediaTrackConstraintSet* constraints = MediaTrackConstraintSet::Create();

  // Set some valid basic constraints
  constraints->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(480));
  constraints->setSampleSize(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(16));

  // Set some invalid range constraints
  ConstrainLongRange* width_range = ConstrainLongRange::Create();
  width_range->setIdeal(1280);
  constraints->setWidth(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(width_range));

  ConstrainDoubleRange* frame_rate_range = ConstrainDoubleRange::Create();
  frame_rate_range->setExact(30.0);
  constraints->setFrameRate(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          frame_rate_range));

  ConstrainBooleanOrDOMStringParameters* echo_cancellation_params =
      ConstrainBooleanOrDOMStringParameters::Create();
  echo_cancellation_params->setExact(
      MakeGarbageCollected<V8UnionBooleanOrString>(true));
  constraints->setEchoCancellation(
      MakeGarbageCollected<
          V8UnionBooleanOrConstrainBooleanOrDOMStringParametersOrString>(
          echo_cancellation_params));

  MediaCaptureElementConstraints::setConstraints(*camera, constraints);

  const HTMLMediaStreamConstraints* stored =
      MediaCaptureElementConstraints::From(*camera).Constraints();

  ASSERT_TRUE(stored);
  ASSERT_TRUE(stored->hasVideo());
  const MediaTrackConstraints* sanitized =
      static_cast<const MediaTrackConstraints*>(stored->video());

  // Valid basic constraints preserved
  EXPECT_TRUE(sanitized->hasHeight());
  EXPECT_EQ(sanitized->height()->GetAsLong(), 480);
  EXPECT_TRUE(sanitized->hasSampleSize());
  EXPECT_EQ(sanitized->sampleSize()->GetAsLong(), 16);

  // Invalid range constraints cleared
  EXPECT_FALSE(sanitized->hasWidth());
  EXPECT_FALSE(sanitized->hasFrameRate());
  EXPECT_FALSE(sanitized->hasEchoCancellation());
}

TEST_F(MediaTrackElementConstraintsTest, SanitizeTrackConstraintsMutatesCopy) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* camera = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  MediaTrackConstraintSet* constraints = MediaTrackConstraintSet::Create();

  constraints->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(480));

  ConstrainLongRange* width_range = ConstrainLongRange::Create();
  width_range->setIdeal(1280);
  constraints->setWidth(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(width_range));

  MediaCaptureElementConstraints::setConstraints(*camera, constraints);

  const HTMLMediaStreamConstraints* stored =
      MediaCaptureElementConstraints::From(*camera).Constraints();

  // Test that original constraints are unmodified
  EXPECT_TRUE(constraints->width()->IsConstrainLongRange());

  // Test that sanitized constraints are modified
  ASSERT_TRUE(stored);
  ASSERT_TRUE(stored->hasVideo());
  EXPECT_FALSE(static_cast<const MediaTrackConstraints*>(stored->video())
                   ->hasWidth());
}

}  // namespace blink

