// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_test_helpers_impl.h"

namespace http2::test {

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

}  // namespace http2::test
