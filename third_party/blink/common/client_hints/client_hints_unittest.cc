// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <iostream>

#include "base/test/scoped_feature_list.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using testing::UnorderedElementsAre;

namespace blink {

TEST(ClientHintsTest, SerializeLangClientHint) {
  std::string header = SerializeLangClientHint("");
  EXPECT_TRUE(header.empty());

  header = SerializeLangClientHint("es");
  EXPECT_EQ(std::string("\"es\""), header);

  header = SerializeLangClientHint("en-US,fr,de");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\""), header);

  header = SerializeLangClientHint("en-US,fr,de,ko,zh-CN,ja");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\", \"ko\", \"zh-CN\", \"ja\""),
            header);
}

TEST(ClientHintsTest, FilterAcceptCH) {
  EXPECT_FALSE(FilterAcceptCH(absl::nullopt, /*permit_lang_hints=*/true,
                              /*permit_ua_hints=*/true,
                              /*permit_prefers_color_scheme_hints=*/true)
                   .has_value());

  absl::optional<std::vector<network::mojom::WebClientHintsType>> result;

  result =
      FilterAcceptCH(std::vector<network::mojom::WebClientHintsType>(
                         {network::mojom::WebClientHintsType::kDeviceMemory,
                          network::mojom::WebClientHintsType::kRtt,
                          network::mojom::WebClientHintsType::kUA}),
                     /*permit_lang_hints=*/false,
                     /*permit_ua_hints=*/true,
                     /*permit_prefers_color_scheme_hints=*/false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(
      result.value(),
      UnorderedElementsAre(network::mojom::WebClientHintsType::kDeviceMemory,
                           network::mojom::WebClientHintsType::kRtt,
                           network::mojom::WebClientHintsType::kUA));

  result = FilterAcceptCH(
      std::vector<network::mojom::WebClientHintsType>(
          {network::mojom::WebClientHintsType::kDeviceMemory,
           network::mojom::WebClientHintsType::kRtt,
           network::mojom::WebClientHintsType::kPrefersColorScheme}),
      /*permit_lang_hints=*/false,
      /*permit_ua_hints=*/false,
      /*permit_prefers_color_scheme_hints=*/true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kDeviceMemory,
                  network::mojom::WebClientHintsType::kRtt,
                  network::mojom::WebClientHintsType::kPrefersColorScheme));

  std::vector<network::mojom::WebClientHintsType> in{
      network::mojom::WebClientHintsType::kRtt,
      network::mojom::WebClientHintsType::kLang,
      network::mojom::WebClientHintsType::kUA,
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAPlatform,
      network::mojom::WebClientHintsType::kUAPlatformVersion,
      network::mojom::WebClientHintsType::kUAModel,
      network::mojom::WebClientHintsType::kUAMobile,
      network::mojom::WebClientHintsType::kUAFullVersion,
      network::mojom::WebClientHintsType::kPrefersColorScheme};

  result = FilterAcceptCH(in,
                          /*permit_lang_hints=*/true,
                          /*permit_ua_hints=*/false,
                          /*permit_prefers_color_scheme_hints=*/false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(network::mojom::WebClientHintsType::kRtt,
                                   network::mojom::WebClientHintsType::kLang));

  result = FilterAcceptCH(in,
                          /*permit_lang_hints=*/true,
                          /*permit_ua_hints=*/true,
                          /*permit_prefers_color_scheme_hints=*/true);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(
                  network::mojom::WebClientHintsType::kRtt,
                  network::mojom::WebClientHintsType::kLang,
                  network::mojom::WebClientHintsType::kUA,
                  network::mojom::WebClientHintsType::kUAArch,
                  network::mojom::WebClientHintsType::kUAPlatform,
                  network::mojom::WebClientHintsType::kUAPlatformVersion,
                  network::mojom::WebClientHintsType::kUAModel,
                  network::mojom::WebClientHintsType::kUAMobile,
                  network::mojom::WebClientHintsType::kUAFullVersion,
                  network::mojom::WebClientHintsType::kPrefersColorScheme));

  result = FilterAcceptCH(in,
                          /*permit_lang_hints=*/false,
                          /*permit_ua_hints=*/false,
                          /*permit_prefers_color_scheme_hints=*/false);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(network::mojom::WebClientHintsType::kRtt));
}

// Checks that the removed header list doesn't include legacy headers nor the
// on-by-default ones, when the kAllowClientHintsToThirdParty flag is on.
TEST(ClientHintsTest, FindClientHintsToRemoveLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAllowClientHintsToThirdParty);
  std::vector<std::string> removed_headers;
  FindClientHintsToRemove(nullptr, GURL(), &removed_headers);
  EXPECT_THAT(removed_headers,
              UnorderedElementsAre("rtt", "downlink", "ect", "sec-ch-lang",
                                   "sec-ch-ua-arch", "sec-ch-ua-platform",
                                   "sec-ch-ua-model", "sec-ch-ua-full-version",
                                   "sec-ch-ua-platform-version",
                                   "sec-ch-prefers-color-scheme"));
}

// Checks that the removed header list includes legacy headers but not the
// on-by-default ones, when the kAllowClientHintsToThirdParty flag is off.
TEST(ClientHintsTest, FindClientHintsToRemoveNoLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAllowClientHintsToThirdParty);
  std::vector<std::string> removed_headers;
  FindClientHintsToRemove(nullptr, GURL(), &removed_headers);
  EXPECT_THAT(
      removed_headers,
      UnorderedElementsAre(
          "device-memory", "dpr", "width", "viewport-width", "rtt", "downlink",
          "ect", "sec-ch-lang", "sec-ch-ua-arch", "sec-ch-ua-platform",
          "sec-ch-ua-model", "sec-ch-ua-full-version",
          "sec-ch-ua-platform-version", "sec-ch-prefers-color-scheme"));
}
}  // namespace blink
