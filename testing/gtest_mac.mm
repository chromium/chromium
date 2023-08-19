// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "gtest_mac.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/googletest/src/googletest/include/gtest/internal/gtest-port.h"
#include "third_party/googletest/src/googletest/include/gtest/internal/gtest-string.h"

// /!\ WARNING!
//
// Chromium compiles this file as ARC, but other dependencies pull it in and
// compile it as non-ARC. Be sure that this file compiles correctly with either
// build setting.
//
// /!\ WARNING!

#ifdef GTEST_OS_MAC

#import <Foundation/Foundation.h>

namespace testing::internal {

static std::string StringFromNSString(NSString* string) {
  // Note that -[NSString UTF8String] is banned in Chromium code because
  // base::SysNSStringToUTF8() is safer, but //testing isn't allowed to depend
  // on //base, so deliberately ignore that function ban.
  const char* utf_string = string.UTF8String;
  return utf_string ? std::string(utf_string) : std::string("(nil nsstring)");
}

// Handles nil values for |obj| properly by using safe printing of %@ in
// -stringWithFormat:.
std::string StringDescription(id<NSObject> obj) {
  return StringFromNSString([NSString stringWithFormat:@"%@", obj]);
}

// This overloaded version allows comparison between ObjC objects that conform
// to the NSObject protocol. Used to implement {ASSERT|EXPECT}_EQ().
GTEST_API_ AssertionResult CmpHelperNSEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         id<NSObject> expected,
                                         id<NSObject> actual) {
  if (expected == actual || [expected isEqual:actual]) {
    return AssertionSuccess();
  }
  return EqFailure(expected_expression, actual_expression,
                   StringDescription(expected), StringDescription(actual),
                   false);
}

// This overloaded version allows comparison between ObjC objects that conform
// to the NSObject protocol. Used to implement {ASSERT|EXPECT}_NE().
GTEST_API_ AssertionResult CmpHelperNSNE(const char* expected_expression,
                                         const char* actual_expression,
                                         id<NSObject> expected,
                                         id<NSObject> actual) {
  if (expected != actual && ![expected isEqual:actual]) {
    return AssertionSuccess();
  }
  Message msg;
  msg << "Expected: (" << expected_expression << ") != (" << actual_expression
      << "), actual: " << StringDescription(expected)
      << " vs " << StringDescription(actual);
  return AssertionFailure(msg);
}

#if !defined(GTEST_OS_IOS)

GTEST_API_ AssertionResult CmpHelperNSEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSRect& expected,
                                         const NSRect& actual) {
  if (NSEqualRects(expected, actual)) {
    return AssertionSuccess();
  }
  return EqFailure(expected_expression, actual_expression,
                   StringFromNSString(NSStringFromRect(expected)),
                   StringFromNSString(NSStringFromRect(actual)), false);
}

GTEST_API_ AssertionResult CmpHelperNSNE(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSRect& expected,
                                         const NSRect& actual) {
  if (!NSEqualRects(expected, actual)) {
    return AssertionSuccess();
  }
  Message msg;
  msg << "Expected: (" << expected_expression << ") != (" << actual_expression
      << "), actual: " << StringFromNSString(NSStringFromRect(expected))
      << " vs " << StringFromNSString(NSStringFromRect(actual));
  return AssertionFailure(msg);

}

GTEST_API_ AssertionResult CmpHelperNSEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSPoint& expected,
                                         const NSPoint& actual) {
  if (NSEqualPoints(expected, actual)) {
    return AssertionSuccess();
  }
  return EqFailure(expected_expression, actual_expression,
                   StringFromNSString(NSStringFromPoint(expected)),
                   StringFromNSString(NSStringFromPoint(actual)), false);
}

GTEST_API_ AssertionResult CmpHelperNSNE(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSPoint& expected,
                                         const NSPoint& actual) {
  if (!NSEqualPoints(expected, actual)) {
    return AssertionSuccess();
  }
  Message msg;
  msg << "Expected: (" << expected_expression << ") != (" << actual_expression
      << "), actual: " << StringFromNSString(NSStringFromPoint(expected))
      << " vs " << StringFromNSString(NSStringFromPoint(actual));
  return AssertionFailure(msg);
}

GTEST_API_ AssertionResult CmpHelperNSEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSRange& expected,
                                         const NSRange& actual) {
  if (NSEqualRanges(expected, actual)) {
    return AssertionSuccess();
  }
  return EqFailure(expected_expression, actual_expression,
                   StringFromNSString(NSStringFromRange(expected)),
                   StringFromNSString(NSStringFromRange(actual)), false);
}

GTEST_API_ AssertionResult CmpHelperNSNE(const char* expected_expression,
                                         const char* actual_expression,
                                         const NSRange& expected,
                                         const NSRange& actual) {
  if (!NSEqualRanges(expected, actual)) {
    return AssertionSuccess();
  }
  Message msg;
  msg << "Expected: (" << expected_expression << ") != (" << actual_expression
      << "), actual: " << StringFromNSString(NSStringFromRange(expected))
      << " vs " << StringFromNSString(NSStringFromRange(actual));
  return AssertionFailure(msg);
}

#endif  // !GTEST_OS_IOS

}  // namespace testing::internal

#endif  // GTEST_OS_MAC
