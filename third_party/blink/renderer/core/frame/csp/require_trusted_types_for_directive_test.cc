// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/require_trusted_types_for_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSPRequireTrustedTypesForTest, Parse) {
  struct {
    const char* directive;
    network::mojom::blink::CSPRequireTrustedTypesFor result;
  } test_cases[] = {
      {"'script'", network::mojom::blink::CSPRequireTrustedTypesFor::Script},
      {"*", network::mojom::blink::CSPRequireTrustedTypesFor::None},
      {"", network::mojom::blink::CSPRequireTrustedTypesFor::None},
      {"''", network::mojom::blink::CSPRequireTrustedTypesFor::None},
      {"script", network::mojom::blink::CSPRequireTrustedTypesFor::None},
      {"'script' 'css'",
       network::mojom::blink::CSPRequireTrustedTypesFor::Script},
      {"'script' 'script'",
       network::mojom::blink::CSPRequireTrustedTypesFor::Script}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << " require-trusted-types-for "
                                    << test_case.directive << ";");
    EXPECT_EQ(
        CSPRequireTrustedTypesForParse(
            test_case.directive, MakeGarbageCollected<ContentSecurityPolicy>()),
        test_case.result);
  }
}

}  // namespace blink
