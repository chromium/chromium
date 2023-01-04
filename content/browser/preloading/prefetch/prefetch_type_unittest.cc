// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class PrefetchTypeTest : public ::testing::Test {};

TEST_F(PrefetchTypeTest, GetPrefetchTypeParams) {
  PrefetchType prefetch_type1(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type2(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type3(/*use_isolated_network_context=*/false,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kDefault);

  EXPECT_TRUE(prefetch_type1.IsIsolatedNetworkContextRequired());
  EXPECT_TRUE(prefetch_type1.IsProxyRequired());
  EXPECT_EQ(prefetch_type1.GetEagerness(),
            blink::mojom::SpeculationEagerness::kEager);

  EXPECT_TRUE(prefetch_type2.IsIsolatedNetworkContextRequired());
  EXPECT_FALSE(prefetch_type2.IsProxyRequired());
  EXPECT_EQ(prefetch_type2.GetEagerness(),
            blink::mojom::SpeculationEagerness::kEager);

  EXPECT_FALSE(prefetch_type3.IsIsolatedNetworkContextRequired());
  EXPECT_FALSE(prefetch_type3.IsProxyRequired());
  EXPECT_EQ(prefetch_type3.GetEagerness(),
            blink::mojom::SpeculationEagerness::kDefault);
}

TEST_F(PrefetchTypeTest, ComparePrefetchTypes) {
  PrefetchType prefetch_type1(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type2(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type3(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type4(/*use_isolated_network_context=*/false,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type5(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kDefault);

  // Explicitly test the == and != operators for |PrefetchType|.
  EXPECT_TRUE(prefetch_type1 == prefetch_type1);
  EXPECT_TRUE(prefetch_type1 == prefetch_type2);
  EXPECT_TRUE(prefetch_type1 != prefetch_type3);
  EXPECT_TRUE(prefetch_type1 != prefetch_type4);
  EXPECT_TRUE(prefetch_type1 != prefetch_type5);
}

TEST_F(PrefetchTypeTest, WptProxyTest) {
  PrefetchType prefetch_types[] = {
      {/*isolated*/ true, /*use_proxy*/ true,
       blink::mojom::SpeculationEagerness::kEager},
      {/*isolated*/ true, /*use_proxy*/ true,
       blink::mojom::SpeculationEagerness::kEager},
      {/*isolated*/ true, /*use_proxy*/ false,
       blink::mojom::SpeculationEagerness::kEager},
      {/*isolated*/ false, /*use_proxy*/ false,
       blink::mojom::SpeculationEagerness::kEager},
  };
  for (auto& prefetch_type : prefetch_types) {
    EXPECT_FALSE(prefetch_type.IsProxyBypassedForTesting());
    if (prefetch_type.IsProxyRequired()) {
      prefetch_type.SetProxyBypassedForTest();
      EXPECT_TRUE(prefetch_type.IsProxyBypassedForTesting());
    }
  }
}

}  // namespace
}  // namespace content
