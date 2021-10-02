// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"
#include <iostream>

#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace network {

TEST(ClientHintsTest, ParseAcceptCH) {
  absl::optional<std::vector<network::mojom::WebClientHintsType>> result;

  // Empty is OK.
  result = ParseClientHintsHeader(" ");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());

  // Normal case.
  result = ParseClientHintsHeader("device-memory,  rtt ");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));

  // Must be a list of tokens, not other things.
  result = ParseClientHintsHeader("\"device-memory\", \"rtt\"");
  EXPECT_FALSE(result.has_value());

  // Parameters to the tokens are ignored, as encourageed by structured headers
  // spec.
  result = ParseClientHintsHeader("device-memory;resolution=GIB, rtt");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));

  // Unknown tokens are fine, since this meant to be extensible.
  result = ParseClientHintsHeader("device-memory,  rtt , nosuchtokenwhywhywhy");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));
}

TEST(ClientHintsTest, ParseAcceptCHCaseInsensitive) {
  absl::optional<std::vector<network::mojom::WebClientHintsType>> result;

  // Matching is case-insensitive.
  result = ParseClientHintsHeader("Device-meMory,  Rtt ");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));
}

TEST(ClientHintsTest, ParseAcceptCHLifetime) {
  EXPECT_EQ(base::TimeDelta(), ParseAcceptCHLifetime(""));
  EXPECT_EQ(base::TimeDelta(), ParseAcceptCHLifetime("-1000"));
  EXPECT_EQ(base::TimeDelta(), ParseAcceptCHLifetime("1000s"));
  EXPECT_EQ(base::TimeDelta(), ParseAcceptCHLifetime("1000.5"));
  EXPECT_EQ(base::Seconds(1000), ParseAcceptCHLifetime("1000"));
}

}  // namespace network
