// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <iostream>

#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::UnorderedElementsAre;

namespace blink {

// Checks that the removed header list doesn't includes on-by-default ones.
TEST(ClientHintsTest, FindClientHintsToRemoveNoLegacy) {
  std::vector<std::string> removed_headers;
  FindClientHintsToRemove(nullptr, GURL(), &removed_headers);
  EXPECT_THAT(
      removed_headers,
      UnorderedElementsAre(
          "device-memory", "dpr", "width", "viewport-width", "rtt", "downlink",
          "ect", "sec-ch-ua-arch", "sec-ch-ua-model", "sec-ch-ua-full-version",
          "sec-ch-ua-platform-version", "sec-ch-prefers-color-scheme",
          "sec-ch-prefers-reduced-motion", "sec-ch-ua-bitness",
          "sec-ch-viewport-height", "sec-ch-device-memory", "sec-ch-dpr",
          "sec-ch-width", "sec-ch-viewport-width",
          "sec-ch-ua-full-version-list", "sec-ch-ua-wow64",
          "sec-ch-ua-form-factors", "sec-ch-prefers-reduced-transparency"));
}
}  // namespace blink
