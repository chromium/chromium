// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/require_trusted_types_for_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(RequireTrustedTypesForDirectiveTest, TestSinks) {
  struct {
    const char* directive;
    const bool result;
  } test_cases[] = {{"'script'", true},
                    {"*", false},
                    {"", false},
                    {"''", false},
                    {"script", false},
                    {"'script' 'css'", true},
                    {"'script' 'script'", true}};

  for (const auto& test_case : test_cases) {
    RequireTrustedTypesForDirective directive(
        "require-trusted-types-for", test_case.directive,
        MakeGarbageCollected<ContentSecurityPolicy>());
    SCOPED_TRACE(testing::Message() << " require-trusted-types-for "
                                    << test_case.directive << ";");
    EXPECT_EQ(directive.require(), test_case.result);
  }
}
}  // namespace blink
