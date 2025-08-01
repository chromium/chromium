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

  bool NeedsObserverCheck(const url::Origin& origin,
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
  const GURL kUrl("https://a.com");
  Initialize(std::nullopt);
  EXPECT_TRUE(NeedsObserverCheck(url::Origin::Create(kUrl),
                                 std::vector<mojom::WebClientHintsType>()));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckEmptyHintsShouldBeFalse) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, added_hints));

  EXPECT_FALSE(
      NeedsObserverCheck(kOrigin, std::vector<mojom::WebClientHintsType>()));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckAMatchHintShouldBeFalse) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  EXPECT_FALSE(NeedsObserverCheck(kOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckMultipleMatchHintsShouldBeFalse) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  EXPECT_FALSE(NeedsObserverCheck(kOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest, NeedsObserverCheckAMismatchShouldBeTrue) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(CreateEnabledClientHints(kOrigin, added_hints));

  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUA,
  };
  EXPECT_TRUE(NeedsObserverCheck(kOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckOneOfEntriesMismatchesShouldBeTrue) {
  const GURL kUrl("https://a.com");
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
  EXPECT_TRUE(NeedsObserverCheck(kOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckDifferentOriginShouldBeTrue) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector));
  const GURL kOther("https://b.com");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_TRUE(NeedsObserverCheck(kOtherOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckNotOutermostMainFrameShouldBeTrue) {
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  EXPECT_TRUE(NeedsObserverCheck(kOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckSubframeOriginMismatchWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadForSubframe", "false"}});
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  const GURL kOther("https://b.com");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_TRUE(NeedsObserverCheck(kOtherOrigin, test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckSubframeOriginMismatchWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOffloadAcceptCHFrameCheck,
      {{"AcceptCHOffloadForSubframe", "true"}});
  const GURL kUrl("https://a.com");
  const url::Origin kOrigin(url::Origin::Create(kUrl));
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(CreateEnabledClientHints(kOrigin, test_vector,
                                      /*is_outermost_main_frame=*/false));
  const GURL kOther("https://b.com");
  const url::Origin kOtherOrigin(url::Origin::Create(kOther));
  EXPECT_FALSE(NeedsObserverCheck(kOtherOrigin, test_vector));
}

}  // namespace network
