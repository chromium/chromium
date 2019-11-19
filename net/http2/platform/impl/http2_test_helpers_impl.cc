// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/http2/platform/api/http2_test_helpers.h"

namespace http2 {
namespace test {

// This is a copy of the same named method in ::testing::internal.
// TODO(jamessynge): See about getting something like VERIFY_* adopted by
// gUnit (probably a very difficult task!).
std::string GetBoolAssertionFailureMessage(
    const ::testing::AssertionResult& assertion_result,
    const char* expression_text,
    const char* actual_predicate_value,
    const char* expected_predicate_value) {
  const char* actual_message = assertion_result.message();
  ::testing::Message msg;
  msg << "Value of: " << expression_text
      << "\n  Actual: " << actual_predicate_value;
  if (actual_message[0] != '\0')
    msg << " (" << actual_message << ")";
  msg << "\nExpected: " << expected_predicate_value;
  return msg.GetString();
}

}  // namespace test
}  // namespace http2
