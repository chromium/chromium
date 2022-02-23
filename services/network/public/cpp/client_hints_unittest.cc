// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"
#include <iostream>

#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::UnorderedElementsAre;

namespace network {

TEST(ClientHintsTest, ParseClientHintsHeader) {
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

  // Matching is case-insensitive.
  result = ParseClientHintsHeader("Device-meMory,  Rtt ");
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
                  network::mojom::WebClientHintsType::kRtt_DEPRECATED));
}

TEST(ClientHintsTest, ParseClientHintToDelegatedThirdPartiesHeader) {
  absl::optional<ClientHintToDelegatedThirdPartiesHeader> result;

  // Empty is OK.
  result = ParseClientHintToDelegatedThirdPartiesHeader(" ");
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().map.empty());
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Normal case.
  result = ParseClientHintToDelegatedThirdPartiesHeader("device-memory,  rtt ");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){})));
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Must be a list of tokens, not other things.
  result = ParseClientHintToDelegatedThirdPartiesHeader(
      "\"device-memory\", \"rtt\"");
  EXPECT_FALSE(result.has_value());

  // Parameters to the tokens are ignored, as encourageed by structured headers
  // spec.
  result = ParseClientHintToDelegatedThirdPartiesHeader(
      "device-memory;resolution=GIB, rtt");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){})));
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Unknown tokens are fine, since this meant to be extensible.
  result = ParseClientHintToDelegatedThirdPartiesHeader(
      "device-memory,  rtt , nosuchtokenwhywhywhy");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){})));
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Matching is case-insensitive.
  result = ParseClientHintToDelegatedThirdPartiesHeader("Device-meMory,  Rtt ");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){})));
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Matching can find a one or more origins.
  result = ParseClientHintToDelegatedThirdPartiesHeader(
      "device-memory=https://foo.bar,  rtt=( https://foo.bar "
      "https://baz.qux) ");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){
                  url::Origin::Create(GURL("https://foo.bar"))}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){
                             url::Origin::Create(GURL("https://foo.bar")),
                             url::Origin::Create(GURL("https://baz.qux"))})));
  EXPECT_FALSE(result.value().had_invalid_origins);

  // Matching ignores invalid domains
  result = ParseClientHintToDelegatedThirdPartiesHeader(
      "device-memory=6,  rtt=( https://foo.bar/wasd self about:blank ) ");
  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value().map,
      UnorderedElementsAre(
          std::make_pair(
              network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
              (std::vector<url::Origin>){}),
          std::make_pair(network::mojom::WebClientHintsType::kRtt_DEPRECATED,
                         (std::vector<url::Origin>){
                             url::Origin::Create(GURL("https://foo.bar"))})));
  EXPECT_TRUE(result.value().had_invalid_origins);
}

}  // namespace network
