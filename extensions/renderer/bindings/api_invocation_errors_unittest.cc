// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_invocation_errors.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api_errors {

// Tests chaining errors for more complicated errors. More of a set of example
// strings than a test of the logic itself (which is pretty simple).
TEST(APIInvocationErrors, ChainedErrors) {
  EXPECT_EQ("Error at index 0: Invalid type: expected string, found integer.",
            IndexError(0, InvalidType(kTypeString, kTypeInteger)));
  EXPECT_EQ(
      "Error at property 'foo': Invalid type: expected string, found integer.",
      PropertyError("foo", InvalidType(kTypeString, kTypeInteger)));
  EXPECT_EQ(
      "Error at property 'foo': Error at index 1: "
      "Invalid type: expected string, found integer.",
      PropertyError("foo",
                    IndexError(1, InvalidType(kTypeString, kTypeInteger))));

  EXPECT_EQ(
      "Error at parameter 'foo': Invalid type: expected string, found integer.",
      ArgumentError("foo", InvalidType(kTypeString, kTypeInteger)));
  EXPECT_EQ(
      "Error at parameter 'foo': Error at index 0: "
      "Invalid type: expected string, found integer.",
      ArgumentError("foo",
                    IndexError(0, InvalidType(kTypeString, kTypeInteger))));

  EXPECT_EQ(
      "Error in invocation of tabs.query("
      "object details, function callback): "
      "Error at parameter 'details': Unexpected property: 'foo'.",
      InvocationError("tabs.query", "object details, function callback",
                      ArgumentError("details", UnexpectedProperty("foo"))));
}

}  // namespace api_errors
}  // namespace extensions
