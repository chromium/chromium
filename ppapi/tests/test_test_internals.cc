// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_test_internals.h"

#include <stdint.h>

#include <vector>

namespace {

std::string CheckEqual(const std::string& expected, const std::string& actual) {
  if (expected != actual) {
    return std::string("Expected : \"") + expected + "\", got : \"" + actual +
       "\"";
  }
  PASS();
}

std::string Negate(const std::string& result) {
  if (result.empty())
    return std::string("FAIL: String was empty.");
  return std::string();
}

class CallCounter {
 public:
  CallCounter() : num_calls_(0) {}

  int return_zero() {
    ++num_calls_;
    return 0;
  }
  double return_zero_as_double() {
    ++num_calls_;
    return 0.0;
  }

  int num_calls() const { return num_calls_; }

 private:
  int num_calls_;
};

}

REGISTER_TEST_CASE(TestInternals);

bool TestTestInternals::Init() {
  return true;
}

void TestTestInternals::RunTests(const std::string& filter) {
  RUN_TEST(ToString, filter);
  RUN_TEST(PassingComparisons, filter);
  RUN_TEST(FailingComparisons, filter);
  RUN_TEST(EvaluateOnce, filter);
}

#define WRAP_LEFT_PARAM(a) \
    internal::ParameterWrapper<IS_NULL_LITERAL(a)>::WrapValue(a)
std::string TestTestInternals::TestToString() {
  // We don't use most ASSERT macros here, because they rely on ToString.
  // ASSERT_SUBTEST_SUCCESS does not use ToString.
  ASSERT_SUBTEST_SUCCESS(CheckEqual(WRAP_LEFT_PARAM(NULL).ToString(), "0"));
  ASSERT_SUBTEST_SUCCESS(CheckEqual(WRAP_LEFT_PARAM(0).ToString(), "0"));
  ASSERT_SUBTEST_SUCCESS(CheckEqual(internal::ToString(5), "5"));
  int32_t x = 5;
  ASSERT_SUBTEST_SUCCESS(CheckEqual(internal::ToString(x + 1), "6"));
  std::string str = "blah";
  ASSERT_SUBTEST_SUCCESS(CheckEqual(internal::ToString(str + "blah"),
                                    "blahblah"));
  std::vector<int> vec;
  ASSERT_SUBTEST_SUCCESS(CheckEqual(internal::ToString(vec), std::string()));

  PASS();
}

#define COMPARE_DOUBLE_EQ(a, b) \
    internal::CompareDoubleEq( \
        internal::ParameterWrapper<IS_NULL_LITERAL(a)>::WrapValue(a), \
        (b), #a, #b, __FILE__, __LINE__)
std::string TestTestInternals::TestPassingComparisons() {
  // These comparisons should all "pass", meaning they should return the empty
  // string.
  {
    const std::string* const kNull = NULL;
    const std::string* const kDeadBeef =
        reinterpret_cast<const std::string*>(0xdeadbeef);
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, NULL, kNull));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, kDeadBeef, kDeadBeef));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, NULL, kDeadBeef));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, kDeadBeef, kNull));
  } {
    const int64_t zero_int32 = 0;
    const int64_t zero_int64 = 0;
    const int32_t zero_uint32 = 0;
    const int64_t zero_uint64 = 0;
    const int32_t one_int32 = 1;
    const int64_t one_int64 = 1;
    const int32_t one_uint32 = 1;
    const int64_t one_uint64 = 1;
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 0, zero_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 0, zero_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 0, zero_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 0, zero_uint64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 1, one_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 1, one_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 1, one_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(EQ, 1, one_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 1, zero_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 1, zero_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 1, zero_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 1, zero_uint64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 0, one_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 0, one_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 0, one_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(NE, 0, one_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LT, 0, one_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LT, 0, one_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LT, 0, one_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LT, 0, one_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, zero_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, zero_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, zero_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, zero_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, one_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, one_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, one_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(LE, 0, one_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GT, 1, zero_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GT, 1, zero_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GT, 1, zero_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GT, 1, zero_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, zero_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, zero_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, zero_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, zero_uint64));

    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, one_int32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, one_uint32));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, one_int64));
    ASSERT_SUBTEST_SUCCESS(COMPARE_BINARY_INTERNAL(GE, 1, one_uint64));
  } {
    ASSERT_SUBTEST_SUCCESS(
        COMPARE_BINARY_INTERNAL(EQ, "hello", std::string("hello")));
    std::vector<int> int_vector1(10, 10);
    std::vector<int> int_vector2(int_vector1);
    ASSERT_SUBTEST_SUCCESS(
        COMPARE_BINARY_INTERNAL(EQ, int_vector1, int_vector2));
  } {
    const double kZeroDouble = 0.0;
    const double kPositiveDouble = 1.1;
    ASSERT_SUBTEST_SUCCESS(
        COMPARE_BINARY_INTERNAL(LT, kZeroDouble, kPositiveDouble));
    ASSERT_SUBTEST_SUCCESS(
        COMPARE_BINARY_INTERNAL(GT, kPositiveDouble, kZeroDouble));
    ASSERT_SUBTEST_SUCCESS(COMPARE_DOUBLE_EQ(0.0, kZeroDouble));
    ASSERT_SUBTEST_SUCCESS(COMPARE_DOUBLE_EQ(1.0 + 0.1, kPositiveDouble));
  }

  // TODO: Things that return non-empty string.
  // TODO: Test that the parameter is evaluated exactly once.
  PASS();
}

#define ASSERT_SUBTEST_FAILURE(param) ASSERT_SUBTEST_SUCCESS(Negate(param))
std::string TestTestInternals::TestFailingComparisons() {
  // Note, we don't really worry about the content of failure strings here.
  // That's mostly covered by the ToString test above. This test just makes
  // sure that comparisons which should return a non-empty string do so.
  {
    const std::string* const kNull = NULL;
    const std::string* const kDeadBeef =
        reinterpret_cast<const std::string*>(0xdeadbeef);
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, NULL, kNull));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, kDeadBeef, kDeadBeef));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, NULL, kDeadBeef));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, kDeadBeef, kNull));
  }

  // Now, just make sure we get any non-empty string at all, which will indicate
  // test failure. We mostly rely on the ToString test to get the formats right.
  {
    const int64_t zero_int32 = 0;
    const int64_t zero_int64 = 0;
    const int32_t zero_uint32 = 0;
    const int64_t zero_uint64 = 0;
    const int32_t one_int32 = 1;
    const int64_t one_int64 = 1;
    const int32_t one_uint32 = 1;
    const int64_t one_uint64 = 1;
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 1, zero_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 1, zero_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 1, zero_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 1, zero_uint64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 0, one_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 0, one_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 0, one_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(EQ, 0, one_uint64));

    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 0, zero_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 0, zero_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 0, zero_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 0, zero_uint64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 1, one_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 1, one_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 1, one_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(NE, 1, one_uint64));

    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LT, 1, one_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LT, 1, one_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LT, 1, one_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LT, 1, one_uint64));

    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LE, 1, zero_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LE, 1, zero_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LE, 1, zero_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(LE, 1, zero_uint64));

    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GT, 0, zero_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GT, 0, zero_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GT, 0, zero_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GT, 0, zero_uint64));

    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GE, 0, one_int32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GE, 0, one_uint32));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GE, 0, one_int64));
    ASSERT_SUBTEST_FAILURE(COMPARE_BINARY_INTERNAL(GE, 0, one_uint64));
  } {
    ASSERT_SUBTEST_FAILURE(
        COMPARE_BINARY_INTERNAL(EQ, "goodbye", std::string("hello")));
    std::vector<int> int_vector1(10, 10);
    std::vector<int> int_vector2;
    ASSERT_SUBTEST_FAILURE(
        COMPARE_BINARY_INTERNAL(EQ, int_vector1, int_vector2));
  } {
    const double kZeroDouble = 0.0;
    const double kPositiveDouble = 1.1;
    ASSERT_SUBTEST_FAILURE(
        COMPARE_BINARY_INTERNAL(GT, kZeroDouble, kPositiveDouble));
    ASSERT_SUBTEST_FAILURE(
        COMPARE_BINARY_INTERNAL(LT, kPositiveDouble, kZeroDouble));
    ASSERT_SUBTEST_FAILURE(COMPARE_DOUBLE_EQ(1.1, kZeroDouble));
    ASSERT_SUBTEST_FAILURE(COMPARE_DOUBLE_EQ(0.0, kPositiveDouble));
  }

  // TODO: Test that the parameter is evaluated exactly once.
  PASS();
}
#undef COMPARE
#undef COMPARE_DOUBLE_EQ

std::string TestTestInternals::TestEvaluateOnce() {
  // Make sure that the ASSERT macros only evaluate each parameter once.
  {
    CallCounter call_counter1;
    CallCounter call_counter2;
    ASSERT_EQ(call_counter1.return_zero(), call_counter2.return_zero());
    assert(call_counter1.num_calls() == 1);
    assert(call_counter2.num_calls() == 1);
  } {
    CallCounter call_counter1;
    CallCounter call_counter2;
    ASSERT_DOUBLE_EQ(call_counter1.return_zero_as_double(),
                     call_counter2.return_zero_as_double());
    assert(call_counter1.num_calls() == 1);
    assert(call_counter2.num_calls() == 1);
  }
  PASS();
}

