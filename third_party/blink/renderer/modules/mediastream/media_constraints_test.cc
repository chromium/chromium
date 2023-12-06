// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

// The MediaTrackConstraintsTest group tests the types declared in
// third_party/blink/renderer/platform/mediastream/media_constraints.h
TEST(MediaTrackConstraintsTest, LongConstraint) {
  test::TaskEnvironment task_environment;
  LongConstraint range_constraint(nullptr);
  range_constraint.SetMin(5);
  range_constraint.SetMax(6);
  EXPECT_TRUE(range_constraint.Matches(5));
  EXPECT_TRUE(range_constraint.Matches(6));
  EXPECT_FALSE(range_constraint.Matches(4));
  EXPECT_FALSE(range_constraint.Matches(7));
  LongConstraint exact_constraint(nullptr);
  exact_constraint.SetExact(5);
  EXPECT_FALSE(exact_constraint.Matches(4));
  EXPECT_TRUE(exact_constraint.Matches(5));
  EXPECT_FALSE(exact_constraint.Matches(6));
}

TEST(MediaTrackConstraintsTest, DoubleConstraint) {
  test::TaskEnvironment task_environment;
  DoubleConstraint range_constraint(nullptr);
  EXPECT_TRUE(range_constraint.IsUnconstrained());
  range_constraint.SetMin(5.0);
  range_constraint.SetMax(6.5);
  EXPECT_FALSE(range_constraint.IsUnconstrained());
  // Matching within epsilon
  EXPECT_TRUE(
      range_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      range_constraint.Matches(6.5 + DoubleConstraint::kConstraintEpsilon / 2));
  DoubleConstraint exact_constraint(nullptr);
  exact_constraint.SetExact(5.0);
  EXPECT_FALSE(range_constraint.IsUnconstrained());
  EXPECT_FALSE(exact_constraint.Matches(4.9));
  EXPECT_TRUE(exact_constraint.Matches(5.0));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 + DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_FALSE(exact_constraint.Matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint) {
  test::TaskEnvironment task_environment;
  BooleanConstraint bool_constraint(nullptr);
  EXPECT_TRUE(bool_constraint.IsUnconstrained());
  EXPECT_TRUE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
  bool_constraint.SetExact(false);
  EXPECT_FALSE(bool_constraint.IsUnconstrained());
  EXPECT_FALSE(bool_constraint.Matches(true));
  EXPECT_TRUE(bool_constraint.Matches(false));
  bool_constraint.SetExact(true);
  EXPECT_FALSE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
}

TEST(MediaTrackConstraintsTest, ConstraintSetEmpty) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraintSetPlatform the_set;
  EXPECT_TRUE(the_set.IsUnconstrained());
  the_set.echo_cancellation.SetExact(false);
  EXPECT_FALSE(the_set.IsUnconstrained());
}

TEST(MediaTrackConstraintsTest, ConstraintName) {
  test::TaskEnvironment task_environment;
  const char* the_name = "name";
  BooleanConstraint bool_constraint(the_name);
  EXPECT_EQ(the_name, bool_constraint.GetName());
}

TEST(MediaTrackConstraintsTest, MandatoryChecks) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraintSetPlatform the_set;
  String found_name;
  EXPECT_FALSE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_FALSE(the_set.width.HasMandatory());
  the_set.width.SetMax(240);
  EXPECT_TRUE(the_set.width.HasMandatory());
  EXPECT_TRUE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"height"}, found_name));
  EXPECT_EQ("width", found_name);
  the_set.echo_cancellation.SetExact(true);
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_EQ("echoCancellation", found_name);
}

TEST(MediaTrackConstraintsTest, SetToString) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraintSetPlatform the_set;
  EXPECT_EQ("", the_set.ToString());
  the_set.width.SetMax(240);
  EXPECT_EQ("width: {max: 240}", the_set.ToString().Utf8());
  the_set.echo_cancellation.SetIdeal(true);
  EXPECT_EQ("width: {max: 240}, echoCancellation: {ideal: true}",
            the_set.ToString().Utf8());
}

TEST(MediaTrackConstraintsTest, ConstraintsToString) {
  test::TaskEnvironment task_environment;
  MediaConstraints the_constraints;
  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced(static_cast<size_t>(1));
  basic.width.SetMax(240);
  advanced[0].echo_cancellation.SetExact(true);
  the_constraints.Initialize(basic, advanced);
  EXPECT_EQ(
      "{width: {max: 240}, advanced: [{echoCancellation: {exact: true}}]}",
      the_constraints.ToString().Utf8());

  MediaConstraints null_constraints;
  EXPECT_EQ("", null_constraints.ToString().Utf8());

  MediaConstraints pan_constraints;
  MediaTrackConstraintSetPlatform pan_basic;
  Vector<MediaTrackConstraintSetPlatform> pan_advanced(static_cast<size_t>(1));
  pan_basic.pan.SetIsPresent(false);
  pan_advanced[0].pan.SetIsPresent(true);
  pan_constraints.Initialize(pan_basic, pan_advanced);
  EXPECT_EQ("{advanced: [{pan: {}}]}", pan_constraints.ToString().Utf8());

  MediaConstraints tilt_constraints;
  MediaTrackConstraintSetPlatform tilt_basic;
  Vector<MediaTrackConstraintSetPlatform> tilt_advanced(static_cast<size_t>(1));
  tilt_basic.tilt.SetIsPresent(false);
  tilt_advanced[0].tilt.SetIsPresent(true);
  tilt_constraints.Initialize(tilt_basic, tilt_advanced);
  EXPECT_EQ("{advanced: [{tilt: {}}]}", tilt_constraints.ToString().Utf8());

  MediaConstraints zoom_constraints;
  MediaTrackConstraintSetPlatform zoom_basic;
  Vector<MediaTrackConstraintSetPlatform> zoom_advanced(static_cast<size_t>(1));
  zoom_basic.zoom.SetIsPresent(false);
  zoom_advanced[0].zoom.SetIsPresent(true);
  zoom_constraints.Initialize(zoom_basic, zoom_advanced);
  EXPECT_EQ("{advanced: [{zoom: {}}]}", zoom_constraints.ToString().Utf8());

  // TODO(crbug.com/1086338): Test other constraints with IsPresent.
}

TEST(MediaTrackConstraintsTest, ConvertWebConstraintsBasic) {
  test::TaskEnvironment task_environment;
  MediaConstraints input;
  [[maybe_unused]] MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
}

TEST(MediaTrackConstraintsTest, ConvertWebSingleStringConstraint) {
  test::TaskEnvironment task_environment;
  MediaConstraints input;

  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced;

  basic.facing_mode.SetIdeal(Vector<String>({"foo"}));
  input.Initialize(basic, advanced);
  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode()->IsString());
  EXPECT_EQ("foo", output->facingMode()->GetAsString());
}

TEST(MediaTrackConstraintsTest, ConvertWebDoubleStringConstraint) {
  test::TaskEnvironment task_environment;
  MediaConstraints input;

  Vector<String> buffer(static_cast<size_t>(2u));
  buffer[0] = "foo";
  buffer[1] = "bar";

  MediaTrackConstraintSetPlatform basic;
  Vector<MediaTrackConstraintSetPlatform> advanced;
  basic.facing_mode.SetIdeal(buffer);
  input.Initialize(basic, advanced);

  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode()->IsStringSequence());
  const auto& out_buffer = output->facingMode()->GetAsStringSequence();
  EXPECT_EQ("foo", out_buffer[0]);
  EXPECT_EQ("bar", out_buffer[1]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkStringConstraint) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  MediaConstraints output;
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>("foo");
  input->setFacingMode(parameter);
  String error_message;
  output = media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_TRUE(error_message.empty());
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkComplexStringConstraint) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  MediaConstraints output;
  ConstrainDOMStringParameters* subparameter =
      ConstrainDOMStringParameters::Create();
  subparameter->setIdeal(
      MakeGarbageCollected<V8UnionStringOrStringSequence>("foo"));
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(subparameter);
  input->setFacingMode(parameter);
  String error_message;
  output = media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_TRUE(error_message.empty());
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  // Convert this back, and see that it appears as a single string.
  MediaTrackConstraints* recycled =
      media_constraints_impl::ConvertConstraints(output);
  ASSERT_TRUE(recycled->hasFacingMode());
  ASSERT_TRUE(recycled->facingMode()->IsString());
  ASSERT_EQ("foo", recycled->facingMode()->GetAsString());
}

TEST(MediaTrackConstraintsTest, NakedIsExactInAdvanced) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>("foo");
  input->setFacingMode(parameter);
  HeapVector<Member<MediaTrackConstraintSet>> advanced(
      1, MediaTrackConstraintSet::Create());
  advanced[0]->setFacingMode(parameter);
  input->setAdvanced(advanced);

  String error_message;
  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
          input, error_message);
  ASSERT_TRUE(error_message.empty());
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_FALSE(output.Basic().facing_mode.HasExact());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  ASSERT_FALSE(output.Advanced()[0].facing_mode.HasIdeal());
  ASSERT_TRUE(output.Advanced()[0].facing_mode.HasExact());
  ASSERT_EQ(1U, output.Advanced()[0].facing_mode.Exact().size());
  ASSERT_EQ("foo", output.Advanced()[0].facing_mode.Exact()[0]);
}

TEST(MediaTrackConstraintsTest, AdvancedParameterFails) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  String str(
      std::string(media_constraints_impl::kMaxConstraintStringLength + 1, 'a')
          .c_str());
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(str);
  HeapVector<Member<MediaTrackConstraintSet>> advanced(
      1, MediaTrackConstraintSet::Create());
  advanced[0]->setFacingMode(parameter);
  input->setAdvanced(advanced);

  String error_message;
  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
          input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string too long.");
}

TEST(MediaTrackConstraintsTest, IdealAndExactConvertToNaked) {
  test::TaskEnvironment task_environment;
  MediaConstraints input;
  Vector<String> buffer(static_cast<size_t>(1u));

  MediaTrackConstraintSetPlatform basic;
  MediaTrackConstraintSetPlatform advanced_element1;
  MediaTrackConstraintSetPlatform advanced_element2;
  buffer[0] = "ideal";
  basic.facing_mode.SetIdeal(buffer);
  advanced_element1.facing_mode.SetIdeal(buffer);
  buffer[0] = "exact";
  advanced_element2.facing_mode.SetExact(buffer);
  Vector<MediaTrackConstraintSetPlatform> advanced;
  advanced.push_back(advanced_element1);
  advanced.push_back(advanced_element2);
  input.Initialize(basic, advanced);

  MediaTrackConstraints* output =
      media_constraints_impl::ConvertConstraints(input);
  // The first element should return a ConstrainDOMStringParameters
  // with an "ideal" value containing a String value of "ideal".
  // The second element should return a ConstrainDOMStringParameters
  // with a String value of "exact".
  ASSERT_TRUE(output->hasAdvanced());
  ASSERT_EQ(2U, output->advanced().size());
  MediaTrackConstraintSet* element1 = output->advanced()[0];
  MediaTrackConstraintSet* element2 = output->advanced()[1];

  ASSERT_TRUE(output->hasFacingMode());
  ASSERT_TRUE(output->facingMode()->IsString());
  EXPECT_EQ("ideal", output->facingMode()->GetAsString());

  ASSERT_TRUE(element1->hasFacingMode());
  ASSERT_TRUE(element1->facingMode()->IsConstrainDOMStringParameters());
  EXPECT_EQ("ideal", element1->facingMode()
                         ->GetAsConstrainDOMStringParameters()
                         ->ideal()
                         ->GetAsString());

  ASSERT_TRUE(element2->hasFacingMode());
  ASSERT_TRUE(element2->facingMode()->IsString());
  EXPECT_EQ("exact", element2->facingMode()->GetAsString());
}

TEST(MediaTrackConstraintsTest, MaxLengthStringConstraintPasses) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  String str(
      std::string(media_constraints_impl::kMaxConstraintStringLength, 'a')
          .c_str());
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(str);
  input->setGroupId(parameter);
  String error_message;
  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
          input, error_message);
  EXPECT_TRUE(error_message.empty());
  EXPECT_EQ(*output.Basic().group_id.Ideal().begin(), str);
}

TEST(MediaTrackConstraintsTest, TooLongStringConstraintFails) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  String str(
      std::string(media_constraints_impl::kMaxConstraintStringLength + 1, 'a')
          .c_str());
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(str);
  input->setGroupId(parameter);
  String error_message;
  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
          input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string too long.");
}

TEST(MediaTrackConstraintsTest, MaxLengthStringSequenceConstraintPasses) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setGroupId(parameter);
  String error_message;
  MediaConstraints output =
      media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
          input, error_message);
  EXPECT_TRUE(error_message.empty());
  EXPECT_EQ(output.Basic().group_id.Ideal().size(),
            media_constraints_impl::kMaxConstraintStringSeqLength);
}

TEST(MediaTrackConstraintsTest, TooLongStringSequenceConstraintFails) {
  test::TaskEnvironment task_environment;
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength + 1);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setGroupId(parameter);
  String error_message;
  media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string sequence too long.");
}

TEST(MediaTrackConstraintsTest,
     TooLongStringSequenceForDeviceIdConstraintFails) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength + 1);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setDeviceId(parameter);
  String error_message;
  media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string sequence too long.");
}

TEST(MediaTrackConstraintsTest,
     TooLongStringSequenceForFacingModeConstraintFails) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength + 1);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setFacingMode(parameter);
  String error_message;
  media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string sequence too long.");
}

TEST(MediaTrackConstraintsTest,
     TooLongStringSequenceForResizeModeConstraintFails) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength + 1);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setResizeMode(parameter);
  String error_message;
  media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string sequence too long.");
}

TEST(MediaTrackConstraintsTest,
     TooLongStringSequenceForDisplaySurfaceConstraintFails) {
  MediaTrackConstraints* input = MediaTrackConstraints::Create();
  Vector<String> sequence;
  sequence.Fill("a", media_constraints_impl::kMaxConstraintStringSeqLength + 1);
  auto* parameter = MakeGarbageCollected<V8ConstrainDOMString>(sequence);
  input->setDisplaySurface(parameter);
  String error_message;
  media_constraints_impl::ConvertTrackConstraintsToMediaConstraints(
      input, error_message);
  ASSERT_FALSE(error_message.empty());
  EXPECT_EQ(error_message, "Constraint string sequence too long.");
}
}  // namespace blink
