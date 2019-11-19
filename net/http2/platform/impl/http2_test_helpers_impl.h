// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_TEST_HELPERS_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_TEST_HELPERS_IMPL_H_

// Defines VERIFY_* macros, analogous to gUnit's EXPECT_* and ASSERT_* macros,
// but these return an appropriate AssertionResult if the condition is not
// satisfied. This enables one to create a function for verifying expectations
// that are needed by multiple callers or that rely on arguments not accessible
// to the main test method. Using VERIFY_SUCCESS allows one to annotate the
// a failing AssertionResult with more context.

#include <iosfwd>
#include <sstream>

#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace http2 {
namespace test {

template <typename T>
class VerifyThatHelper {
 public:
  VerifyThatHelper(const T& value, ::testing::Matcher<T> matcher) {
    matches_ = matcher.Matches(value);
    if (!matches_) {
      printed_value_ = ::testing::PrintToString(value);

      std::ostringstream os;
      matcher.DescribeTo(&os);
      matcher_description_ = os.str();
    }
  }

  VerifyThatHelper(const VerifyThatHelper&) = delete;
  VerifyThatHelper& operator=(const VerifyThatHelper&) = delete;

  operator bool() const { return matches_; }

  const std::string& printed_value() const { return printed_value_; }
  const std::string& matcher_description() const {
    return matcher_description_;
  }

 private:
  bool matches_;
  std::string printed_value_;
  std::string matcher_description_;
};

// Constructs a failure message for Boolean assertions such as VERIFY_TRUE.
std::string GetBoolAssertionFailureMessage(
    const ::testing::AssertionResult& assertion_result,
    const char* expression_text,
    const char* actual_predicate_value,
    const char* expected_predicate_value);

namespace {
// Define HasSubstr() for Http2StringPiece arguments.
// This shadows ::testing::HasSubstr(), which only works on argument types
// that can be implicitly converted to a std::string.
inline ::testing::PolymorphicMatcher<net::test::StringPieceHasSubstrMatcher>
HasSubstr(const std::string& substring) {
  return ::testing::MakePolymorphicMatcher(
      net::test::StringPieceHasSubstrMatcher(substring));
}
}  // namespace

}  // namespace test
}  // namespace http2

// Macro for adding verification location to output stream or AssertionResult.
// Starts with a new-line because of the way that gUnit displays failures for
//    EXPECT_TRUE(CallToFunctionThatFailsToVerify()).
#define VERIFY_FAILED_LOCATION_                   \
  "\n"                                            \
      << "(VERIFY failed in " << __func__ << "\n" \
      << "               at " __FILE__ " : " << __LINE__ << ")\n"

// Implements Boolean test verifications VERIFY_TRUE and VERIFY_FALSE.
// text is a textual represenation of expression as it was passed into
// VERIFY_TRUE or VERIFY_FALSE.
// clang-format off
#define VERIFY_TEST_BOOLEAN_(condition, text, actual, expected)        \
  if (const ::testing::AssertionResult __assertion_result =            \
          ::testing::AssertionResult((condition) ? expected : actual)) \
    ;                                                                  \
  else                                                                 \
    return ::testing::AssertionFailure()                               \
         << VERIFY_FAILED_LOCATION_                                    \
         << ::http2::test::GetBoolAssertionFailureMessage(             \
                __assertion_result, text, #actual, #expected)
// clang-format on

// Boolean assertions. condition can be either a Boolean expression or an
// expression convertable to a boolean (such as a ::gtl::labs::optional).
#define VERIFY_TRUE(condition) \
  VERIFY_TEST_BOOLEAN_(condition, #condition, false, true)

#define VERIFY_FALSE(condition) \
  VERIFY_TEST_BOOLEAN_(condition, #condition, true, false)

// Convenient helper macro for writing methods that return an AssertionFailure
// that includes the tested condition in the message (in the manner of
// ASSERT_THAT and EXPECT_THAT).
//
// This macro avoids the do {} while(false) trick and putting braces around
// the if so you can do things like:
// VERIFY_THAT(foo, Lt(4)) << "foo too big in iteration " << i;
// (This parallels the implementation of CHECK().)
//
// We use an if statement with an empty true branch so that this doesn't eat
// a neighboring else when used in an unbraced if statement like:
// if (condition)
//   VERIFY_THAT(foo, Eq(bar));
// else
//   FAIL();
#define VERIFY_THAT(value, matcher)                                         \
  if (const auto& _verify_that_helper =                                     \
          ::http2::test::VerifyThatHelper<decltype(value)>(value, matcher)) \
    ;                                                                       \
  else                                                                      \
    return ::testing::AssertionFailure()                                    \
           << "Failed to verify that '" #value "' ("                        \
           << _verify_that_helper.printed_value() << ") "                   \
           << _verify_that_helper.matcher_description()                     \
           << " (on " __FILE__ ":" << __LINE__ << "). "

// Useful variants of VERIFY_THAT, similar to the corresponding EXPECT_X or
// ASSERT_X defined by gUnit.
#define VERIFY_EQ(val1, val2) VERIFY_THAT(val1, ::testing::Eq(val2))
#define VERIFY_NE(val1, val2) VERIFY_THAT(val1, ::testing::Ne(val2))
#define VERIFY_GT(val1, val2) VERIFY_THAT(val1, ::testing::Gt(val2))
#define VERIFY_LT(val1, val2) VERIFY_THAT(val1, ::testing::Lt(val2))
#define VERIFY_GE(val1, val2) VERIFY_THAT(val1, ::testing::Ge(val2))
#define VERIFY_LE(val1, val2) VERIFY_THAT(val1, ::testing::Le(val2))

// Convenience macro matching EXPECT_OK
#define VERIFY_OK(statement) VERIFY_EQ(::util::Status::OK, (statement))

// This version verifies that an expression of type AssertionResult is
// AssertionSuccess. If instead the value is an AssertionFailure, it appends
// info about the current code location to the failure's message and returns
// the failure to the caller of the current method. It permits the code site
// to append further messages to the failure message. For example:
//    VERIFY_SUCCESS(SomeCall()) << "Some more context about SomeCall";
// clang-format off
#define VERIFY_SUCCESS(expr)                                  \
  if (::testing::AssertionResult __assertion_result = (expr)) \
    ;                                                         \
  else                                                        \
    return __assertion_result << VERIFY_FAILED_LOCATION_
// clang-format on

#define VERIFY_AND_RETURN_SUCCESS(expression) \
  {                                           \
    VERIFY_SUCCESS(expression);               \
    return ::testing::AssertionSuccess();     \
  }

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_TEST_HELPERS_IMPL_H_
