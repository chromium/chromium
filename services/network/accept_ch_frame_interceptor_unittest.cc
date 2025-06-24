// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/accept_ch_frame_interceptor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class AcceptCHFrameInterceptorTest : public testing::Test {
 public:
  void Initialize(std::optional<std::vector<mojom::WebClientHintsType>> hints) {
    interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
        mojo::NullRemote(), std::move(hints));
  }

  bool NeedsObserverCheck(const std::vector<mojom::WebClientHintsType>& hints) {
    CHECK(interceptor_.get());
    return interceptor_->NeedsObserverCheckForTesting(hints);
  }

 protected:
  void TearDown() override { interceptor_.reset(); }

  std::unique_ptr<AcceptCHFrameInterceptor> interceptor_;
};

TEST_F(AcceptCHFrameInterceptorTest, NeedsObserverCheckNullOpt) {
  Initialize(std::nullopt);
  EXPECT_TRUE(NeedsObserverCheck(std::vector<mojom::WebClientHintsType>()));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckEmptyHintsShouldBeFalse) {
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(added_hints);

  EXPECT_FALSE(NeedsObserverCheck(std::vector<mojom::WebClientHintsType>()));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckAMatchHintShouldBeFalse) {
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
  };
  Initialize(test_vector);
  EXPECT_FALSE(NeedsObserverCheck(test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckMultipleMatchHintsShouldBeFalse) {
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(test_vector);
  EXPECT_FALSE(NeedsObserverCheck(test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest, NeedsObserverCheckAMismatchShouldBeTrue) {
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  Initialize(added_hints);

  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUA,
  };
  EXPECT_TRUE(NeedsObserverCheck(test_vector));
}

TEST_F(AcceptCHFrameInterceptorTest,
       NeedsObserverCheckOneOfEntriesMismatchesShouldBeTrue) {
  std::vector<mojom::WebClientHintsType> added_hints = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };

  Initialize(added_hints);
  std::vector<mojom::WebClientHintsType> test_vector = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUA,
  };
  EXPECT_TRUE(NeedsObserverCheck(test_vector));
}

}  // namespace network
