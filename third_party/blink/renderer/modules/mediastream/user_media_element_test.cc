#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_or_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanordomstringparameters_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class UserMediaElementTest : public ::testing::Test {
 public:
  test::TaskEnvironment task_environment_;
};

TEST_F(UserMediaElementTest, SetConstraintsStoresValue) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());

  UserMediaElementConstraints::setConstraints(*element, constraints);

  EXPECT_TRUE(UserMediaElementConstraints::From(*element).Constraints());
}

TEST_F(UserMediaElementTest, SetConstraintsOnlySetsOnce) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());

  UserMediaElementConstraints::setConstraints(*element, constraints);
  const HTMLMediaStreamConstraints* sanitized_constraints =
      UserMediaElementConstraints::From(*element).Constraints();

  EXPECT_TRUE(sanitized_constraints);

  HTMLMediaStreamConstraints* constraints2 =
      HTMLMediaStreamConstraints::Create();
  constraints2->setAudio(MediaTrackConstraintSet::Create());
  UserMediaElementConstraints::setConstraints(*element, constraints2);
  EXPECT_EQ(UserMediaElementConstraints::From(*element).Constraints(),
            sanitized_constraints);
}

TEST_F(UserMediaElementTest, SanitizeTrackConstraints) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  MediaTrackConstraintSet* video_constraints = MediaTrackConstraintSet::Create();
  MediaTrackConstraintSet* audio_constraints = MediaTrackConstraintSet::Create();

  // Set some valid constraints
  video_constraints->setWidth(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(640));
  video_constraints->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(480));
  audio_constraints->setSampleSize(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(16));

  // Set some invalid constraints
  ConstrainLongRange* width_range = ConstrainLongRange::Create();
  width_range->setIdeal(1280);
  // width gets overwritten
  video_constraints->setWidth(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(width_range));

  ConstrainDoubleRange* frame_rate_range = ConstrainDoubleRange::Create();
  frame_rate_range->setExact(30.0);
  video_constraints->setFrameRate(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          frame_rate_range));

  ConstrainBooleanOrDOMStringParameters* echo_cancellation_params =
      ConstrainBooleanOrDOMStringParameters::Create();
  echo_cancellation_params->setExact(
      MakeGarbageCollected<V8UnionBooleanOrString>(true));
  audio_constraints->setEchoCancellation(
      MakeGarbageCollected<
          V8UnionBooleanOrConstrainBooleanOrDOMStringParametersOrString>(
          echo_cancellation_params));

  constraints->setVideo(video_constraints);
  constraints->setAudio(audio_constraints);

  UserMediaElementConstraints::setConstraints(*element, constraints);

  const HTMLMediaStreamConstraints* sanitized_constraints =
      UserMediaElementConstraints::From(*element).Constraints();

  // Valid constraints should be preserved
  EXPECT_TRUE(sanitized_constraints->video()->hasHeight());
  EXPECT_EQ(sanitized_constraints->video()->height()->GetAsLong(), 480);
  EXPECT_TRUE(sanitized_constraints->audio()->hasSampleSize());
  EXPECT_EQ(sanitized_constraints->audio()->sampleSize()->GetAsLong(), 16);

  // Invalid constraints should be cleared
  EXPECT_FALSE(sanitized_constraints->video()->hasWidth());
  EXPECT_FALSE(sanitized_constraints->video()->hasFrameRate());
  EXPECT_FALSE(sanitized_constraints->audio()->hasEchoCancellation());

  // Unsupported constraints, e.g of image and share screen tracks are ignored.
  EXPECT_FALSE(sanitized_constraints->video()->hasBrightness());
  EXPECT_FALSE(sanitized_constraints->video()->hasDisplaySurface());
}

TEST_F(UserMediaElementTest, SanitizeTrackConstraintsMutatesCopy) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  MediaTrackConstraintSet* video_constraints = MediaTrackConstraintSet::Create();
  MediaTrackConstraintSet* audio_constraints = MediaTrackConstraintSet::Create();

  // Set some valid constraints
  video_constraints->setHeight(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(480));

  // Set some invalid constraints
  ConstrainLongRange* width_range = ConstrainLongRange::Create();
  width_range->setIdeal(1280);
  video_constraints->setWidth(
      MakeGarbageCollected<V8UnionConstrainLongRangeOrLong>(width_range));

  ConstrainBooleanOrDOMStringParameters* echo_cancellation_params =
      ConstrainBooleanOrDOMStringParameters::Create();
  echo_cancellation_params->setExact(
      MakeGarbageCollected<V8UnionBooleanOrString>(true));
  audio_constraints->setEchoCancellation(
      MakeGarbageCollected<
          V8UnionBooleanOrConstrainBooleanOrDOMStringParametersOrString>(
          echo_cancellation_params));

  constraints->setVideo(video_constraints);
  constraints->setAudio(audio_constraints);

  UserMediaElementConstraints::setConstraints(*element, constraints);

  const HTMLMediaStreamConstraints* sanitized_constraints =
      UserMediaElementConstraints::From(*element).Constraints();

  // Test that original constraints are unmodified
  EXPECT_TRUE(constraints->video()->width()->IsConstrainLongRange());
  EXPECT_TRUE(constraints->audio()
                  ->echoCancellation()
                  ->IsConstrainBooleanOrDOMStringParameters());

  // Test that sanitized constraints are modified
  EXPECT_FALSE(sanitized_constraints->video()->hasWidth());
  EXPECT_FALSE(sanitized_constraints->audio()->hasEchoCancellation());
}

}  // namespace blink
