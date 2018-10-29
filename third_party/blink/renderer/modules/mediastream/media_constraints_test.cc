// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_constraints.h"

namespace blink {

// The MediaTrackConstraintsTest group tests the types declared in
// WebKit/public/platform/WebMediaConstraints.h
TEST(MediaTrackConstraintsTest, LongConstraint) {
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
  DoubleConstraint range_constraint(nullptr);
  EXPECT_TRUE(range_constraint.IsEmpty());
  range_constraint.SetMin(5.0);
  range_constraint.SetMax(6.5);
  EXPECT_FALSE(range_constraint.IsEmpty());
  // Matching within epsilon
  EXPECT_TRUE(
      range_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      range_constraint.Matches(6.5 + DoubleConstraint::kConstraintEpsilon / 2));
  DoubleConstraint exact_constraint(nullptr);
  exact_constraint.SetExact(5.0);
  EXPECT_FALSE(range_constraint.IsEmpty());
  EXPECT_FALSE(exact_constraint.Matches(4.9));
  EXPECT_TRUE(exact_constraint.Matches(5.0));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 - DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_TRUE(
      exact_constraint.Matches(5.0 + DoubleConstraint::kConstraintEpsilon / 2));
  EXPECT_FALSE(exact_constraint.Matches(5.1));
}

TEST(MediaTrackConstraintsTest, BooleanConstraint) {
  BooleanConstraint bool_constraint(nullptr);
  EXPECT_TRUE(bool_constraint.IsEmpty());
  EXPECT_TRUE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
  bool_constraint.SetExact(false);
  EXPECT_FALSE(bool_constraint.IsEmpty());
  EXPECT_FALSE(bool_constraint.Matches(true));
  EXPECT_TRUE(bool_constraint.Matches(false));
  bool_constraint.SetExact(true);
  EXPECT_FALSE(bool_constraint.Matches(false));
  EXPECT_TRUE(bool_constraint.Matches(true));
}

TEST(MediaTrackConstraintsTest, ConstraintSetEmpty) {
  WebMediaTrackConstraintSet the_set;
  EXPECT_TRUE(the_set.IsEmpty());
  the_set.echo_cancellation.SetExact(false);
  EXPECT_FALSE(the_set.IsEmpty());
}

TEST(MediaTrackConstraintsTest, ConstraintName) {
  const char* the_name = "name";
  BooleanConstraint bool_constraint(the_name);
  EXPECT_EQ(the_name, bool_constraint.GetName());
}

TEST(MediaTrackConstraintsTest, MandatoryChecks) {
  WebMediaTrackConstraintSet the_set;
  std::string found_name;
  EXPECT_FALSE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_FALSE(the_set.width.HasMandatory());
  the_set.width.SetMax(240);
  EXPECT_TRUE(the_set.width.HasMandatory());
  EXPECT_TRUE(the_set.HasMandatory());
  EXPECT_FALSE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"height"}, found_name));
  EXPECT_EQ("width", found_name);
  the_set.goog_payload_padding.SetExact(true);
  EXPECT_TRUE(the_set.HasMandatoryOutsideSet({"width"}, found_name));
  EXPECT_EQ("googPayloadPadding", found_name);
}

TEST(MediaTrackConstraintsTest, SetToString) {
  WebMediaTrackConstraintSet the_set;
  EXPECT_EQ("", the_set.ToString());
  the_set.width.SetMax(240);
  EXPECT_EQ("width: {max: 240}", the_set.ToString().Utf8());
  the_set.echo_cancellation.SetIdeal(true);
  EXPECT_EQ("width: {max: 240}, echoCancellation: {ideal: true}",
            the_set.ToString().Utf8());
}

TEST(MediaTrackConstraintsTest, ConstraintsToString) {
  WebMediaConstraints the_constraints;
  WebMediaTrackConstraintSet basic;
  WebVector<WebMediaTrackConstraintSet> advanced(static_cast<size_t>(1));
  basic.width.SetMax(240);
  advanced[0].echo_cancellation.SetExact(true);
  the_constraints.Initialize(basic, advanced);
  EXPECT_EQ(
      "{width: {max: 240}, advanced: [{echoCancellation: {exact: true}}]}",
      the_constraints.ToString().Utf8());

  WebMediaConstraints null_constraints;
  EXPECT_EQ("", null_constraints.ToString().Utf8());
}

TEST(MediaTrackConstraintsTest, ConvertWebConstraintsBasic) {
  WebMediaConstraints input;
  MediaTrackConstraints output;

  media_constraints_impl::ConvertConstraints(input, output);
}

TEST(MediaTrackConstraintsTest, ConvertWebSingleStringConstraint) {
  WebMediaConstraints input;
  MediaTrackConstraints output;

  WebMediaTrackConstraintSet basic;
  WebVector<WebMediaTrackConstraintSet> advanced;

  basic.facing_mode.SetIdeal(WebVector<WebString>(&"foo", 1));
  input.Initialize(basic, advanced);
  media_constraints_impl::ConvertConstraints(input, output);
  ASSERT_TRUE(output.hasFacingMode());
  ASSERT_TRUE(output.facingMode().IsString());
  EXPECT_EQ("foo", output.facingMode().GetAsString());
}

TEST(MediaTrackConstraintsTest, ConvertWebDoubleStringConstraint) {
  WebMediaConstraints input;
  MediaTrackConstraints output;

  WebVector<WebString> buffer(static_cast<size_t>(2u));
  buffer[0] = "foo";
  buffer[1] = "bar";

  WebMediaTrackConstraintSet basic;
  std::vector<WebMediaTrackConstraintSet> advanced;
  basic.facing_mode.SetIdeal(buffer);
  input.Initialize(basic, advanced);
  media_constraints_impl::ConvertConstraints(input, output);
  ASSERT_TRUE(output.hasFacingMode());
  ASSERT_TRUE(output.facingMode().IsStringSequence());
  auto out_buffer = output.facingMode().GetAsStringSequence();
  EXPECT_EQ("foo", out_buffer[0]);
  EXPECT_EQ("bar", out_buffer[1]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkStringConstraint) {
  MediaTrackConstraints input;
  WebMediaConstraints output;
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  parameter.SetString("foo");
  input.setFacingMode(parameter);
  output = media_constraints_impl::ConvertConstraintsToWeb(input);
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);
}

TEST(MediaTrackConstraintsTest, ConvertBlinkComplexStringConstraint) {
  MediaTrackConstraints input;
  WebMediaConstraints output;
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  ConstrainDOMStringParameters subparameter;
  StringOrStringSequence inner_string;
  inner_string.SetString("foo");
  subparameter.setIdeal(inner_string);
  parameter.SetConstrainDOMStringParameters(subparameter);
  input.setFacingMode(parameter);
  output = media_constraints_impl::ConvertConstraintsToWeb(input);
  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  // Convert this back, and see that it appears as a single string.
  MediaTrackConstraints recycled;
  media_constraints_impl::ConvertConstraints(output, recycled);
  ASSERT_TRUE(recycled.hasFacingMode());
  ASSERT_TRUE(recycled.facingMode().IsString());
  ASSERT_EQ("foo", recycled.facingMode().GetAsString());
}

TEST(MediaTrackConstraintsTest, NakedIsExactInAdvanced) {
  MediaTrackConstraints input;
  WebMediaConstraints output;
  StringOrStringSequenceOrConstrainDOMStringParameters parameter;
  parameter.SetString("foo");
  input.setFacingMode(parameter);
  HeapVector<MediaTrackConstraintSet> advanced(1);
  advanced[0].setFacingMode(parameter);
  input.setAdvanced(advanced);
  output = media_constraints_impl::ConvertConstraintsToWeb(input);

  ASSERT_TRUE(output.Basic().facing_mode.HasIdeal());
  ASSERT_FALSE(output.Basic().facing_mode.HasExact());
  ASSERT_EQ(1U, output.Basic().facing_mode.Ideal().size());
  ASSERT_EQ("foo", output.Basic().facing_mode.Ideal()[0]);

  ASSERT_FALSE(output.Advanced()[0].facing_mode.HasIdeal());
  ASSERT_TRUE(output.Advanced()[0].facing_mode.HasExact());
  ASSERT_EQ(1U, output.Advanced()[0].facing_mode.Exact().size());
  ASSERT_EQ("foo", output.Advanced()[0].facing_mode.Exact()[0]);
}

TEST(MediaTrackConstraintsTest, IdealAndExactConvertToNaked) {
  WebMediaConstraints input;
  MediaTrackConstraints output;

  WebVector<WebString> buffer(static_cast<size_t>(1u));

  WebMediaTrackConstraintSet basic;
  WebMediaTrackConstraintSet advanced_element1;
  WebMediaTrackConstraintSet advanced_element2;
  buffer[0] = "ideal";
  basic.facing_mode.SetIdeal(buffer);
  advanced_element1.facing_mode.SetIdeal(buffer);
  buffer[0] = "exact";
  advanced_element2.facing_mode.SetExact(buffer);
  std::vector<WebMediaTrackConstraintSet> advanced;
  advanced.push_back(advanced_element1);
  advanced.push_back(advanced_element2);
  input.Initialize(basic, advanced);
  media_constraints_impl::ConvertConstraints(input, output);
  // The first element should return a ConstrainDOMStringParameters
  // with an "ideal" value containing a String value of "ideal".
  // The second element should return a ConstrainDOMStringParameters
  // with a String value of "exact".
  ASSERT_TRUE(output.hasAdvanced());
  ASSERT_EQ(2U, output.advanced().size());
  MediaTrackConstraintSet element1 = output.advanced()[0];
  MediaTrackConstraintSet element2 = output.advanced()[1];

  ASSERT_TRUE(output.hasFacingMode());
  ASSERT_TRUE(output.facingMode().IsString());
  EXPECT_EQ("ideal", output.facingMode().GetAsString());

  ASSERT_TRUE(element1.hasFacingMode());
  ASSERT_TRUE(element1.facingMode().IsConstrainDOMStringParameters());
  EXPECT_EQ("ideal", element1.facingMode()
                         .GetAsConstrainDOMStringParameters()
                         .ideal()
                         .GetAsString());

  ASSERT_TRUE(element2.hasFacingMode());
  ASSERT_TRUE(element2.facingMode().IsString());
  EXPECT_EQ("exact", element2.facingMode().GetAsString());
}

}  // namespace blink
