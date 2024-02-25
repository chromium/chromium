// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/xpath_functions.h"

#include <cmath>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/xml/xpath_expression_node.h"  // EvaluationContext
#include "third_party/blink/renderer/core/xml/xpath_predicate.h"  // Number, StringExpression
#include "third_party/blink/renderer/core/xml/xpath_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"  // HeapVector, Member, etc.
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

class XPathContext {
  STACK_ALLOCATED();

 public:
  XPathContext()
      : document_(
            Document::CreateForTest(execution_context_.GetExecutionContext())),
        context_(*document_, had_type_conversion_error_) {}

  xpath::EvaluationContext& Context() { return context_; }
  Document& GetDocument() { return *document_; }

 private:
  ScopedNullExecutionContext execution_context_;
  Document* const document_;
  bool had_type_conversion_error_ = false;
  xpath::EvaluationContext context_;
};

using XPathArguments = HeapVector<Member<xpath::Expression>>;

static String Substring(XPathArguments& args) {
  XPathContext xpath;
  xpath::Expression* call = xpath::CreateFunction("substring", args);
  xpath::Value result = call->Evaluate(xpath.Context());
  return result.ToString();
}

static String Substring(const char* string, double pos) {
  XPathArguments args;
  args.push_back(MakeGarbageCollected<xpath::StringExpression>(string));
  args.push_back(MakeGarbageCollected<xpath::Number>(pos));
  return Substring(args);
}

static String Substring(const char* string, double pos, double len) {
  XPathArguments args;
  args.push_back(MakeGarbageCollected<xpath::StringExpression>(string));
  args.push_back(MakeGarbageCollected<xpath::Number>(pos));
  args.push_back(MakeGarbageCollected<xpath::Number>(len));
  return Substring(args);
}

}  // namespace

TEST(XPathFunctionsTest, substring_specExamples) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(" car", Substring("motor car", 6.0))
      << "should select characters staring at position 6 to the end";
  EXPECT_EQ("ada", Substring("metadata", 4.0, 3.0))
      << "should select characters at 4 <= position < 7";
  EXPECT_EQ("234", Substring("123456", 1.5, 2.6))
      << "should select characters at 2 <= position < 5";
  EXPECT_EQ("12", Substring("12345", 0.0, 3.0))
      << "should select characters at 0 <= position < 3; note the first "
         "position is 1 so this is characters in position 1 and 2";
  EXPECT_EQ("", Substring("12345", 5.0, -3.0))
      << "no characters should have 5 <= position < 2";
  EXPECT_EQ("1", Substring("12345", -3.0, 5.0))
      << "should select characters at -3 <= position < 2; since the first "
         "position is 1, this is the character at position 1";
  EXPECT_EQ("", Substring("12345", NAN, 3.0))
      << "should select no characters since NaN <= position is always false";
  EXPECT_EQ("", Substring("12345", 1.0, NAN))
      << "should select no characters since position < 1. + NaN is always "
         "false";
  EXPECT_EQ("12345",
            Substring("12345", -42, std::numeric_limits<double>::infinity()))
      << "should select characters at -42 <= position < Infinity, which is all "
         "of them";
  EXPECT_EQ("", Substring("12345", -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::infinity()))
      << "since -Inf+Inf is NaN, should select no characters since position "
         "< NaN is always false";
}

TEST(XPathFunctionsTest, substring_emptyString) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("", Substring("", 0.0, 1.0))
      << "substring of an empty string should be the empty string";
}

TEST(XPathFunctionsTest, substring) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("hello", Substring("well hello there", 6.0, 5.0));
}

TEST(XPathFunctionsTest, substring_negativePosition) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("hello", Substring("hello, world!", -4.0, 10.0))
      << "negative start positions should impinge on the result length";
  // Try to underflow the length adjustment for negative positions.
  EXPECT_EQ("",
            Substring("hello", std::numeric_limits<int32_t>::min() + 1, 1.0));
}

TEST(XPathFunctionsTest, substring_negativeLength) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("", Substring("hello, world!", 1.0, -3.0))
      << "negative lengths should result in an empty string";

  EXPECT_EQ("", Substring("foo", std::numeric_limits<int32_t>::min(), 1.0))
      << "large (but long representable) negative position should result in "
      << "an empty string";
}

TEST(XPathFunctionsTest, substring_extremePositionLength) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("", Substring("no way", 1e100, 7.0))
      << "extremely large positions should result in the empty string";

  EXPECT_EQ("no way", Substring("no way", -1e200, 1e300))
      << "although these indices are not representable as long, this should "
      << "produce the string because indices are computed as doubles";
}

}  // namespace blink
