// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/accept_ch_frame_interceptor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"
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

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AcceptCHFrameInterceptor> interceptor_;
  base::test::ScopedFeatureList feature_list_{
      features::kOffloadAcceptCHFrameCheck};
};

namespace {

class HoldingAcceptCHFrameObserver : public mojom::AcceptCHFrameObserver {
 public:
  HoldingAcceptCHFrameObserver() = default;
  HoldingAcceptCHFrameObserver(const HoldingAcceptCHFrameObserver&) = delete;
  HoldingAcceptCHFrameObserver& operator=(const HoldingAcceptCHFrameObserver&) =
      delete;
  ~HoldingAcceptCHFrameObserver() override = default;

  mojo::PendingRemote<mojom::AcceptCHFrameObserver> BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &HoldingAcceptCHFrameObserver::OnDisconnected, base::Unretained(this)));
    return remote;
  }

  void CloseReceiver() {
    receiver_.reset();
    held_callback_.Reset();
  }

  void Reply(int status) {
    ASSERT_TRUE(held_callback_);
    std::move(held_callback_).Run(status);
  }

  bool is_disconnected() const { return disconnected_; }

  void WaitForCall() {
    if (held_callback_) {
      return;
    }
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();
  }

 private:
  void OnAcceptCHFrameReceived(
      const url::Origin& origin,
      const std::vector<mojom::WebClientHintsType>& accept_ch_frame,
      OnAcceptCHFrameReceivedCallback callback) override {
    held_callback_ = std::move(callback);
    if (wait_loop_) {
      wait_loop_->Quit();
    }
  }

  void Clone(
      mojo::PendingReceiver<mojom::AcceptCHFrameObserver> listener) override {}

  void OnDisconnected() { disconnected_ = true; }

  OnAcceptCHFrameReceivedCallback held_callback_;
  std::unique_ptr<base::RunLoop> wait_loop_;
  bool disconnected_ = false;
  mojo::Receiver<mojom::AcceptCHFrameObserver> receiver_{this};
};

}  // namespace

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

TEST_F(AcceptCHFrameInterceptorTest,
       ObserverDisconnectedBeforeOnConnectedReturnsOk) {
  HoldingAcceptCHFrameObserver observer;
  interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
      observer.BindNewPipeAndPassRemote(), std::nullopt);

  observer.CloseReceiver();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !interceptor_->IsObserverConnectedForTesting(); }));

  bool callback_called = false;
  net::Error result = interceptor_->OnConnected(
      GURL("https://a.test/"), "Sec-CH-UA-Arch", net::HttpRequestHeaders(),
      base::BindOnce([](bool* called, int) { *called = true; },
                     &callback_called));
  EXPECT_EQ(result, net::OK);
  EXPECT_FALSE(callback_called);
}

TEST_F(AcceptCHFrameInterceptorTest,
       ObserverDisconnectedDuringOnConnectedRunsCallbackWithOk) {
  HoldingAcceptCHFrameObserver observer;
  interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
      observer.BindNewPipeAndPassRemote(), std::nullopt);

  base::test::TestFuture<int> completion_future;
  net::Error result = interceptor_->OnConnected(
      GURL("https://a.test/"), "Sec-CH-UA-Arch", net::HttpRequestHeaders(),
      completion_future.GetCallback());
  ASSERT_EQ(result, net::ERR_IO_PENDING);

  observer.WaitForCall();
  observer.CloseReceiver();
  EXPECT_EQ(completion_future.Get(), net::OK);
}

TEST_F(AcceptCHFrameInterceptorTest,
       DestroyedWhileOnConnectedPendingDoesNotRunCallback) {
  HoldingAcceptCHFrameObserver observer;
  interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
      observer.BindNewPipeAndPassRemote(), std::nullopt);

  bool callback_called = false;
  net::Error result = interceptor_->OnConnected(
      GURL("https://a.test/"), "Sec-CH-UA-Arch", net::HttpRequestHeaders(),
      base::BindOnce([](bool* called, int) { *called = true; },
                     &callback_called));
  ASSERT_EQ(result, net::ERR_IO_PENDING);

  observer.WaitForCall();
  interceptor_.reset();
  ASSERT_TRUE(base::test::RunUntil([&] { return observer.is_disconnected(); }));
  observer.CloseReceiver();
  EXPECT_FALSE(callback_called);
}

TEST_F(AcceptCHFrameInterceptorTest,
       ObserverReplyRunsCallbackAndAllowsNextOnConnected) {
  HoldingAcceptCHFrameObserver observer;
  interceptor_ = AcceptCHFrameInterceptor::CreateForTesting(
      observer.BindNewPipeAndPassRemote(), std::nullopt);

  base::test::TestFuture<int> first_future;
  ASSERT_EQ(interceptor_->OnConnected(GURL("https://a.test/"), "Sec-CH-UA-Arch",
                                      net::HttpRequestHeaders(),
                                      first_future.GetCallback()),
            net::ERR_IO_PENDING);
  observer.WaitForCall();
  ASSERT_NO_FATAL_FAILURE(observer.Reply(net::OK));
  EXPECT_EQ(first_future.Get(), net::OK);

  base::test::TestFuture<int> second_future;
  ASSERT_EQ(interceptor_->OnConnected(GURL("https://a.test/"), "Sec-CH-UA-Arch",
                                      net::HttpRequestHeaders(),
                                      second_future.GetCallback()),
            net::ERR_IO_PENDING);
  observer.WaitForCall();
  ASSERT_NO_FATAL_FAILURE(observer.Reply(net::ERR_ABORTED));
  EXPECT_EQ(second_future.Get(), net::ERR_ABORTED);
}

}  // namespace network
