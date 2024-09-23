// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_type.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

class PrefetchTypeTest : public ::testing::Test {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kPrefetchBrowserInitiatedTriggers},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrefetchTypeTest, GetPrefetchTypeParams) {
  PrefetchType prefetch_type1(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type2(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type3(
      PreloadingTriggerType::kSpeculationRule,
      /*use_prefetch_proxy=*/false,
      blink::mojom::SpeculationEagerness::kConservative);

  PrefetchType prefetch_type4(PreloadingTriggerType::kEmbedder,
                              /*use_prefetch_proxy=*/true);

  PrefetchType prefetch_type5(PreloadingTriggerType::kEmbedder,
                              /*use_prefetch_proxy=*/false);

  EXPECT_TRUE(prefetch_type1.IsProxyRequiredWhenCrossOrigin());
  EXPECT_EQ(prefetch_type1.GetEagerness(),
            blink::mojom::SpeculationEagerness::kEager);

  EXPECT_FALSE(prefetch_type2.IsProxyRequiredWhenCrossOrigin());
  EXPECT_EQ(prefetch_type2.GetEagerness(),
            blink::mojom::SpeculationEagerness::kEager);

  EXPECT_FALSE(prefetch_type3.IsProxyRequiredWhenCrossOrigin());
  EXPECT_EQ(prefetch_type3.GetEagerness(),
            blink::mojom::SpeculationEagerness::kConservative);

  EXPECT_TRUE(prefetch_type4.IsProxyRequiredWhenCrossOrigin());

  EXPECT_FALSE(prefetch_type5.IsProxyRequiredWhenCrossOrigin());
}

TEST_F(PrefetchTypeTest, ComparePrefetchTypes) {
  PrefetchType prefetch_type1(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type2(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type3(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/false,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type4(
      PreloadingTriggerType::kSpeculationRule,
      /*use_prefetch_proxy=*/true,
      blink::mojom::SpeculationEagerness::kConservative);
  PrefetchType prefetch_type5(PreloadingTriggerType::kEmbedder,
                              /*use_prefetch_proxy=*/true);

  // Explicitly test the == and != operators for |PrefetchType|.
  EXPECT_TRUE(prefetch_type1 == prefetch_type1);
  EXPECT_TRUE(prefetch_type1 == prefetch_type2);
  EXPECT_TRUE(prefetch_type1 != prefetch_type3);
  EXPECT_TRUE(prefetch_type1 != prefetch_type4);
  EXPECT_TRUE(prefetch_type1 != prefetch_type5);
}

TEST_F(PrefetchTypeTest, PrefetchInitiator) {
  PrefetchType prefetch_type1(PreloadingTriggerType::kSpeculationRule,
                              /*use_prefetch_proxy=*/true,
                              blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type2(
      PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld,
      /*use_prefetch_proxy=*/true, blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type3(
      PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules,
      /*use_prefetch_proxy=*/true, blink::mojom::SpeculationEagerness::kEager);
  PrefetchType prefetch_type4(PreloadingTriggerType::kEmbedder,
                              /*use_prefetch_proxy=*/true);

  EXPECT_TRUE(prefetch_type1.IsRendererInitiated());
  EXPECT_TRUE(prefetch_type2.IsRendererInitiated());
  EXPECT_TRUE(prefetch_type3.IsRendererInitiated());
  EXPECT_FALSE(prefetch_type4.IsRendererInitiated());
}

TEST_F(PrefetchTypeTest, WptProxyTest) {
  PrefetchType prefetch_types[] = {
      {PreloadingTriggerType::kSpeculationRule, /*use_prefetch_proxy=*/true,
       blink::mojom::SpeculationEagerness::kEager},
      {PreloadingTriggerType::kSpeculationRule, /*use_prefetch_proxy=*/false,
       blink::mojom::SpeculationEagerness::kEager},
      {PreloadingTriggerType::kEmbedder, /*use_prefetch_proxy=*/true},
      {PreloadingTriggerType::kEmbedder, /*use_prefetch_proxy=*/false},
  };
  for (auto& prefetch_type : prefetch_types) {
    EXPECT_FALSE(prefetch_type.IsProxyBypassedForTesting());
    if (prefetch_type.IsProxyRequiredWhenCrossOrigin()) {
      prefetch_type.SetProxyBypassedForTest();
      EXPECT_TRUE(prefetch_type.IsProxyBypassedForTesting());
    }
  }
}

}  // namespace
}  // namespace content
