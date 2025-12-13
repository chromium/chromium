// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/accept_ch_frame_interceptor.h"

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class AcceptCHFrameInterceptorTest : public testing::Test {
 public:
  void Initialize(
      std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
          enabled_client_hints) {
    interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
        mojo::NullRemote(), std::move(enabled_client_hints));
  }

  ResourceRequest::TrustedParams::EnabledClientHints CreateEnabledClientHints(
      const url::Origin& origin,
      const std::vector<mojom::WebClientHintsType>& hints,
      bool is_outermost_main_frame = true) {
    ResourceRequest::TrustedParams::EnabledClientHints enabled_client_hints;
    enabled_client_hints.origin = origin;
    enabled_client_hints.hints = hints;
    enabled_client_hints.is_outermost_main_frame = is_outermost_main_frame;
    return enabled_client_hints;
  }

  AcceptCHFrameInterceptor::NeedsObserverCheckReason NeedsObserverCheck(
      const url::Origin& origin,
      const std::vector<mojom::WebClientHintsType>& hints) {
    CHECK(interceptor_.get());
    return interceptor_->NeedsObserverCheckForTesting(origin, hints);
  }

 protected:
  void TearDown() override { interceptor_.reset(); }

  std::unique_ptr<AcceptCHFrameInterceptor> interceptor_;
  base::test::ScopedFeatureList feature_list_{
      features::kOffloadAcceptCHFrameCheck};
};

TEST_F(AcceptCHFrameInterceptorTest, NeedsObserverCheckNullOpt) {
  const GURL kUrl("https://a.test");
  Initialize(std::nullopt);
  EXPECT_EQ(NeedsObserverCheck(url::Origin::Create(kUrl),
                               std::vector<mojom::WebClientHintsType>()),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::
                kNoEnabledClientHints);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckEmptyHintsShouldBeFalse) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, added_hints));

  EXPECT_EQ(
      NeedsObserverCheck(kOrigin, std::vector<mojom::WebClientHintsType>()),
      AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckAMatchHintShouldBeFalse) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  EXPECT_EQ(NeedsObserverCheck(kOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckMultipleMatchHintsShouldBeFalse) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  EXPECT_EQ(NeedsObserverCheck(kOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);
}

TEST_F(AcceptCHFrameInterceptorTest, NeedsObserverCheckAMismatchShouldBeTrue) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, added_hints));

  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUA,
  };
  EXPECT_EQ(
      NeedsObserverCheck(kOrigin, test_vector),
      AcceptCHFrameInterceptor::NeedsObserverCheckReason::kHintNotEnabled);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckOneOfEntriesMismatchesShouldBeTrue) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };

  Initialize(CreateEnabledClientHints(kOrigin, added_hints));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUA,
  };
  EXPECT_EQ(
      NeedsObserverCheck(kOrigin, test_vector),
      AcceptCHFrameInterceptor::NeedsObserverCheckReason::kHintNotEnabled);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckDifferentOriginShouldBeTrue) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  const GURL kOther("https://b.test");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_EQ(NeedsObserverCheck(kOtherOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::
                kMainFrameOriginMismatch);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckNotOutermostMainFrameShouldBeTrue) {
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  EXPECT_EQ(NeedsObserverCheck(kOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::
                kSubframeFeatureDisabled);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckSubframeOriginMismatchWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadForSubframe", "false"}});
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  const GURL kOther("https://b.test");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_EQ(NeedsObserverCheck(kOtherOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::
                kSubframeFeatureDisabled);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckSubframeOriginMismatchWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadForSubframe", "true"}});
  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  const GURL kOther("https://b.test");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_EQ(NeedsObserverCheck(kOtherOrigin, test_vector),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckNotAllowedHintWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHFrameOffloadNotAllowedHints", "true"}});

  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  auto enabled_client_hints = CreateEnabledClientHints(
      kOrigin, {network::mojom::WebClientHintsType::kUAArch});
  enabled_client_hints.not_allowed_hints = {
      network::mojom::WebClientHintsType::kUAPlatform};
  Initialize(std::move(enabled_client_hints));

  // A hint that is only in `not_allowed_hints` should be considered enabled.
  EXPECT_EQ(NeedsObserverCheck(
                kOrigin, {network::mojom::WebClientHintsType::kUAPlatform}),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);

  // A mix of hints from both lists should also be considered enabled.
  EXPECT_EQ(NeedsObserverCheck(
                kOrigin, {network::mojom::WebClientHintsType::kUAArch,
                          network::mojom::WebClientHintsType::kUAPlatform}),
            AcceptCHFrameInterceptor::NeedsObserverCheckReason::kNotNeeded);
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckNotAllowedHintWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHFrameOffloadNotAllowedHints", "false"}});

  const GURL kUrl("https://a.test");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  auto enabled_client_hints = CreateEnabledClientHints(
      kOrigin, {network::mojom::WebClientHintsType::kUAArch});
  enabled_client_hints.not_allowed_hints = {
      network::mojom::WebClientHintsType::kUAPlatform};
  Initialize(std::move(enabled_client_hints));

  // A hint that is only in `not_allowed_hints` should be a mismatch.
  EXPECT_EQ(
      NeedsObserverCheck(kOrigin,
                         {network::mojom::WebClientHintsType::kUAPlatform}),
      AcceptCHFrameInterceptor::NeedsObserverCheckReason::kHintNotEnabled);
}

}  // namespace network
