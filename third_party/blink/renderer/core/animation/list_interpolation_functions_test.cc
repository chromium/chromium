// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class TestNonInterpolableValue final : public NonInterpolableValue {
 public:
  ~TestNonInterpolableValue() override = default;

  static scoped_refptr<TestNonInterpolableValue> Create(int value) {
    DCHECK_GE(value, 1);
    return base::AdoptRef(new TestNonInterpolableValue(value));
  }

  int GetValue() const { return value_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit TestNonInterpolableValue(int value) : value_(value) {}

  int value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(TestNonInterpolableValue);

// DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS won't work in anonymous namespaces.
inline const TestNonInterpolableValue& ToTestNonInterpolableValue(
    const NonInterpolableValue& value) {
  DCHECK_EQ(value.GetType(), TestNonInterpolableValue::static_type_);
  return static_cast<const TestNonInterpolableValue&>(value);
}

class TestUnderlyingValue : public UnderlyingValue {
  STACK_ALLOCATED();

 public:
  TestUnderlyingValue(InterpolationValue& interpolation_value)
      : interpolation_value_(interpolation_value) {}

  InterpolableValue& MutableInterpolableValue() final {
    return *interpolation_value_.interpolable_value;
  }

  void SetInterpolableValue(InterpolableValue* interpolable_value) final {
    interpolation_value_.interpolable_value = std::move(interpolable_value);
  }

  const NonInterpolableValue* GetNonInterpolableValue() const final {
    return interpolation_value_.non_interpolable_value.get();
  }

  void SetNonInterpolableValue(
      scoped_refptr<const NonInterpolableValue> non_interpolable_value) final {
    interpolation_value_.non_interpolable_value = non_interpolable_value;
  }

 private:
  InterpolationValue& interpolation_value_;
};

// Creates an InterpolationValue containing a list of interpolable and
// non-interpolable values from the pairs of input.
InterpolationValue CreateInterpolableList(
    const Vector<std::pair<double, int>>& values) {
  return ListInterpolationFunctions::CreateList(
      values.size(), [&values](wtf_size_t i) {
        return InterpolationValue(
            MakeGarbageCollected<InterpolableNumber>(values[i].first),
            TestNonInterpolableValue::Create(values[i].second));
      });
}

// Creates an InterpolationValue which contains a list of interpolable values,
// but a non-interpolable list of nullptrs.
InterpolationValue CreateInterpolableList(const Vector<double>& values) {
  return ListInterpolationFunctions::CreateList(
      values.size(), [&values](wtf_size_t i) {
        return InterpolationValue(
            MakeGarbageCollected<InterpolableNumber>(values[i]), nullptr);
      });
}

// Creates an InterpolationValue which contains a list of non-interpolable
// values, but an interpolable list of zeroes.
InterpolationValue CreateNonInterpolableList(const Vector<int>& values) {
  return ListInterpolationFunctions::CreateList(
      values.size(), [&values](wtf_size_t i) {
        return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0),
                                  TestNonInterpolableValue::Create(values[i]));
      });
}

// A simple helper to specify which InterpolableValues in an InterpolableList
// should be considered compatible.
class InterpolableValuesCompatibilityHelper {
 public:
  // The input |answers| vector must be at least as large as the
  // InterpolableList being tested, or |AreCompatible| will DCHECK.
  InterpolableValuesCompatibilityHelper(Vector<bool> answers)
      : answers_(answers), current_index_(0) {}

  // Callers should pass a reference to this function to
  // ListInterpolationFunctions::Composite.
  bool AreCompatible(const InterpolableValue*, const InterpolableValue*) {
    DCHECK(current_index_ < answers_.size());
    return answers_.at(current_index_++);
  }

 private:
  Vector<bool> answers_;
  wtf_size_t current_index_;
};

bool NonInterpolableValuesAreCompatible(const NonInterpolableValue* a,
                                        const NonInterpolableValue* b) {
  return (a ? ToTestNonInterpolableValue(*a).GetValue() : 0) ==
         (b ? ToTestNonInterpolableValue(*b).GetValue() : 0);
}

PairwiseInterpolationValue MaybeMergeSingles(InterpolationValue&& start,
                                             InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.get(),
                                          end.non_interpolable_value.get())) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value), nullptr);
}

void Composite(UnderlyingValue& underlying_value,
               double underlying_fraction,
               const InterpolableValue& interpolable_value,
               const NonInterpolableValue* non_interpolable_value) {
  DCHECK(NonInterpolableValuesAreCompatible(
      underlying_value.GetNonInterpolableValue(), non_interpolable_value));
  underlying_value.MutableInterpolableValue().ScaleAndAdd(underlying_fraction,
                                                          interpolable_value);
}

}  // namespace

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesSameLengths) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      MaybeMergeSingles);

  EXPECT_TRUE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesDifferentLengths) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({1.0, 2.0, 3.0});
  auto list2 = CreateInterpolableList({1.0, 3.0});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      MaybeMergeSingles);

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesIncompatibleValues) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 4}, {3.0, 3}});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      MaybeMergeSingles);

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesIncompatibleNullptrs) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({1, 2, 3});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      MaybeMergeSingles);

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualCompositeSameLengths) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      NonInterpolableValuesAreCompatible, Composite);

  const auto& result = To<InterpolableList>(*owner.Value().interpolable_value);

  CSSToLengthConversionData length_resolver;
  ASSERT_EQ(result.length(), 3u);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(0))->Value(length_resolver), 2.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(1))->Value(length_resolver), 4.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(2))->Value(length_resolver), 6.0);
}

// Two lists of different lengths are not interpolable, so we expect the
// underlying value to be replaced.
TEST(ListInterpolationFunctionsTest, EqualCompositeDifferentLengths) {
  test::TaskEnvironment task_environment;
  auto list1 = CreateInterpolableList({1.0, 2.0, 3.0});
  auto list2 = CreateInterpolableList({4.0, 5.0});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      NonInterpolableValuesAreCompatible, Composite);

  const auto& result = To<InterpolableList>(*owner.Value().interpolable_value);

  CSSToLengthConversionData length_resolver;
  ASSERT_EQ(result.length(), 2u);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(0))->Value(length_resolver), 4.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(1))->Value(length_resolver), 5.0);
}

// If one (or more) of the element pairs are incompatible, the list as a whole
// is non-interpolable. We expect the underlying value to be replaced.
TEST(ListInterpolationFunctionsTest,
     EqualCompositeIncompatibleInterpolableValues) {
  auto list1 = CreateInterpolableList({1.0, 2.0, 3.0});
  auto list2 = CreateInterpolableList({4.0, 5.0, 6.0});

  InterpolableValuesCompatibilityHelper compatibility_helper(
      {true, false, true});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [&compatibility_helper](const InterpolableValue* a,
                              const InterpolableValue* b) {
        return compatibility_helper.AreCompatible(a, b);
      },
      NonInterpolableValuesAreCompatible, Composite);

  const auto& result = To<InterpolableList>(*owner.Value().interpolable_value);

  CSSToLengthConversionData length_resolver;
  ASSERT_EQ(result.length(), 3u);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(0))->Value(length_resolver), 4.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(1))->Value(length_resolver), 5.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(2))->Value(length_resolver), 6.0);
}

// If one (or more) of the element pairs are incompatible, the list as a whole
// is non-interpolable. We expect the underlying value to be replaced.
TEST(ListInterpolationFunctionsTest,
     EqualCompositeIncompatibleNonInterpolableValues) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{4.0, 1}, {5.0, 4}, {6.0, 3}});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      NonInterpolableValuesAreCompatible, Composite);

  const auto& result = To<InterpolableList>(*owner.Value().interpolable_value);

  CSSToLengthConversionData length_resolver;
  ASSERT_EQ(result.length(), 3u);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(0))->Value(length_resolver), 4.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(1))->Value(length_resolver), 5.0);
  EXPECT_EQ(To<InterpolableNumber>(result.Get(2))->Value(length_resolver), 6.0);
}

TEST(ListInterpolationFunctionsTest, BuilderNoModify) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_EQ(&before, &after);
  ASSERT_EQ(3u, before.length());
  EXPECT_EQ(1, ToTestNonInterpolableValue(*before.Get(0)).GetValue());
  EXPECT_EQ(2, ToTestNonInterpolableValue(*before.Get(1)).GetValue());
  EXPECT_EQ(3, ToTestNonInterpolableValue(*before.Get(2)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyFirst) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(0, TestNonInterpolableValue::Create(4));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  ASSERT_EQ(3u, after.length());
  EXPECT_EQ(4, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
  EXPECT_EQ(2, ToTestNonInterpolableValue(*after.Get(1)).GetValue());
  EXPECT_EQ(3, ToTestNonInterpolableValue(*after.Get(2)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyMiddle) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(1, TestNonInterpolableValue::Create(4));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  ASSERT_EQ(3u, after.length());
  EXPECT_EQ(1, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
  EXPECT_EQ(4, ToTestNonInterpolableValue(*after.Get(1)).GetValue());
  EXPECT_EQ(3, ToTestNonInterpolableValue(*after.Get(2)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyLast) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(2, TestNonInterpolableValue::Create(4));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  ASSERT_EQ(3u, after.length());
  EXPECT_EQ(1, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
  EXPECT_EQ(2, ToTestNonInterpolableValue(*after.Get(1)).GetValue());
  EXPECT_EQ(4, ToTestNonInterpolableValue(*after.Get(2)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyAll) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(0, TestNonInterpolableValue::Create(4));
    builder.Set(1, TestNonInterpolableValue::Create(5));
    builder.Set(2, TestNonInterpolableValue::Create(6));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  ASSERT_EQ(3u, after.length());
  EXPECT_EQ(4, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
  EXPECT_EQ(5, ToTestNonInterpolableValue(*after.Get(1)).GetValue());
  EXPECT_EQ(6, ToTestNonInterpolableValue(*after.Get(2)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyReverse) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1, 2, 3, 4, 5});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(3, TestNonInterpolableValue::Create(6));
    builder.Set(1, TestNonInterpolableValue::Create(7));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  ASSERT_EQ(5u, after.length());
  EXPECT_EQ(1, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
  EXPECT_EQ(7, ToTestNonInterpolableValue(*after.Get(1)).GetValue());
  EXPECT_EQ(3, ToTestNonInterpolableValue(*after.Get(2)).GetValue());
  EXPECT_EQ(6, ToTestNonInterpolableValue(*after.Get(3)).GetValue());
  EXPECT_EQ(5, ToTestNonInterpolableValue(*after.Get(4)).GetValue());
}

TEST(ListInterpolationFunctionsTest, BuilderModifyListWithOneItem) {
  test::TaskEnvironment task_environment;
  auto list = CreateNonInterpolableList({1});
  auto& before = To<NonInterpolableList>(*list.non_interpolable_value);

  {
    TestUnderlyingValue underlying_value(list);
    NonInterpolableList::AutoBuilder builder(underlying_value);
    builder.Set(0, TestNonInterpolableValue::Create(4));
  }

  auto& after = To<NonInterpolableList>(*list.non_interpolable_value);

  EXPECT_NE(&before, &after);
  EXPECT_EQ(4, ToTestNonInterpolableValue(*after.Get(0)).GetValue());
}

}  // namespace blink
