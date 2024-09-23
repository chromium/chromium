// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/binder_exchange.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/android/binder.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace mojo {
namespace {

class BinderExchangeTest : public testing::Test {
 public:
  void SetUp() override {
    if (!base::android::IsNativeBinderAvailable()) {
      GTEST_SKIP() << "This test is only valid with native Binder support (Q+)";
    }
  }

  base::Process LaunchChild(const char* name, base::android::BinderRef binder) {
    base::CommandLine cmd = base::GetMultiProcessTestChildBaseCommandLine();
    base::LaunchOptions options;
    options.binders.push_back(std::move(binder));
    return base::SpawnMultiProcessTestChild(name, cmd, options);
  }

  bool JoinChild(const base::Process& child) {
    int child_exit_code = -1;
    base::WaitForMultiprocessTestChildExit(
        child, TestTimeouts::action_timeout(), &child_exit_code);
    return child_exit_code == 0;
  }
};

DEFINE_BINDER_CLASS(TestChannelInterface);

// Bidirectional channel interface to exercise binder exchange.
class TestChannel : public base::android::SupportsBinder<TestChannelInterface> {
 public:
  static constexpr transaction_code_t kReceiveInt = 1234;

  explicit TestChannel() = default;

  void set_exchange_failure_handler(base::RepeatingClosure callback) {
    exchange_failure_handler_ = std::move(callback);
  }

  void set_disconnect_handler(base::RepeatingClosure callback) {
    disconnect_handler_ = std::move(callback);
  }

  bool DoExchange(base::android::BinderRef exchange) {
    return ExchangeBinders(exchange, GetBinder(),
                           base::BindOnce(&TestChannel::OnPeerReceived, this))
        .has_value();
  }

  bool SendInt(int32_t value) {
    connected_.Wait();
    const auto result = peer_->TransactOneWay(
        kReceiveInt, [value](const auto& w) { return w.WriteInt32(value); });
    return result.has_value();
  }

  int32_t WaitToReceiveInt() {
    received_.Wait();
    return *received_int_;
  }

  void Close() {
    connected_.Wait();
    peer_.reset();
  }

 private:
  ~TestChannel() override = default;

  void OnPeerReceived(base::android::BinderRef peer) {
    if (!peer) {
      if (exchange_failure_handler_) {
        exchange_failure_handler_.Run();
      }
      return;
    }

    peer_ = TestChannelInterface::AdoptBinderRef(std::move(peer));
    connected_.Signal();
  }

  // base::android::SupportsBinder<TestChannelInterface>:
  base::android::BinderStatusOr<void> OnBinderTransaction(
      transaction_code_t code,
      const base::android::ParcelReader& in,
      const base::android::ParcelWriter& out) override {
    if (code != kReceiveInt) {
      return base::unexpected(STATUS_UNKNOWN_TRANSACTION);
    }

    received_int_ = *in.ReadInt32();
    received_.Signal();
    return base::ok();
  }

  void OnBinderDestroyed() override {
    if (disconnect_handler_) {
      disconnect_handler_.Run();
    }
  }

  base::WaitableEvent connected_;
  std::optional<TestChannelInterface::BinderRef> peer_;

  base::WaitableEvent received_;
  std::optional<int32_t> received_int_;

  base::RepeatingClosure exchange_failure_handler_;
  base::RepeatingClosure disconnect_handler_;
};

TEST_F(BinderExchangeTest, LocalExchange) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  auto channel0 = base::MakeRefCounted<TestChannel>();
  auto channel1 = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel0->DoExchange(std::move(exchange0)));
  EXPECT_TRUE(channel1->DoExchange(std::move(exchange1)));
  EXPECT_TRUE(channel0->SendInt(5));
  EXPECT_TRUE(channel1->SendInt(99));
  EXPECT_EQ(99, channel0->WaitToReceiveInt());
  EXPECT_EQ(5, channel1->WaitToReceiveInt());
}

TEST_F(BinderExchangeTest, ExchangeWithChild) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  auto channel = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel->DoExchange(std::move(exchange0)));
  base::Process child =
      LaunchChild("ExchangeWithChild_Child", std::move(exchange1));
  EXPECT_TRUE(channel->SendInt(7));
  EXPECT_EQ(3, channel->WaitToReceiveInt());
  EXPECT_TRUE(JoinChild(child));
}

#define RETURN_FROM_CHILD() return ::testing::Test::HasFailure() ? 1 : 0

MULTIPROCESS_TEST_MAIN(ExchangeWithChild_Child) {
  auto channel = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel->DoExchange(base::android::TakeBinderFromParent(0)));
  EXPECT_TRUE(channel->SendInt(3));
  EXPECT_EQ(7, channel->WaitToReceiveInt());
  RETURN_FROM_CHILD();
}

TEST_F(BinderExchangeTest, ExchangeBetweenChildren) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  base::Process child0 =
      LaunchChild("ExchangeBetweenChildren_Child0", std::move(exchange0));
  base::Process child1 =
      LaunchChild("ExchangeBetweenChildren_Child1", std::move(exchange1));
  EXPECT_TRUE(JoinChild(child0));
  EXPECT_TRUE(JoinChild(child1));
}

MULTIPROCESS_TEST_MAIN(ExchangeBetweenChildren_Child0) {
  auto channel = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel->DoExchange(base::android::TakeBinderFromParent(0)));
  EXPECT_TRUE(channel->SendInt(42));
  EXPECT_EQ(9001, channel->WaitToReceiveInt());
  RETURN_FROM_CHILD();
}

MULTIPROCESS_TEST_MAIN(ExchangeBetweenChildren_Child1) {
  auto channel = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel->DoExchange(base::android::TakeBinderFromParent(0)));
  EXPECT_TRUE(channel->SendInt(9001));
  EXPECT_EQ(42, channel->WaitToReceiveInt());
  RETURN_FROM_CHILD();
}

TEST_F(BinderExchangeTest, DisconnectBeforeExchange) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  auto channel = base::MakeRefCounted<TestChannel>();
  base::WaitableEvent failure;
  channel->set_exchange_failure_handler(
      base::BindLambdaForTesting([&] { failure.Signal(); }));
  exchange1.reset();
  EXPECT_TRUE(channel->DoExchange(std::move(exchange0)));
  failure.Wait();
}

TEST_F(BinderExchangeTest, DisconnectDuringExchange) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  auto channel = base::MakeRefCounted<TestChannel>();
  base::WaitableEvent failure;
  channel->set_exchange_failure_handler(
      base::BindLambdaForTesting([&] { failure.Signal(); }));
  EXPECT_TRUE(channel->DoExchange(std::move(exchange0)));
  exchange1.reset();
  failure.Wait();
}

TEST_F(BinderExchangeTest, DisconnectAfterExchange) {
  auto [exchange0, exchange1] = CreateBinderExchange();
  auto channel0 = base::MakeRefCounted<TestChannel>();
  auto channel1 = base::MakeRefCounted<TestChannel>();
  EXPECT_TRUE(channel0->DoExchange(std::move(exchange0)));
  EXPECT_TRUE(channel1->DoExchange(std::move(exchange1)));
  EXPECT_TRUE(channel0->SendInt(1));
  EXPECT_TRUE(channel1->SendInt(2));
  EXPECT_EQ(2, channel0->WaitToReceiveInt());
  EXPECT_EQ(1, channel1->WaitToReceiveInt());
  base::WaitableEvent disconnect;
  channel0->set_disconnect_handler(
      base::BindLambdaForTesting([&] { disconnect.Signal(); }));
  channel1->Close();
  disconnect.Wait();
}

TEST_F(BinderExchangeTest, InvalidExchange) {
  // Null exchange binder.
  auto channel0 = base::MakeRefCounted<TestChannel>();
  EXPECT_FALSE(ExchangeBinders(base::android::BinderRef(),
                               channel0->GetBinder(), base::DoNothing())
                   .has_value());

  // Null endpoint binder.
  auto [exchange0, exchange1] = CreateBinderExchange();
  EXPECT_FALSE(
      ExchangeBinders(exchange0, base::android::BinderRef(), base::DoNothing())
          .has_value());

  // Duplicate exchange binder usage.
  EXPECT_TRUE(
      ExchangeBinders(exchange1, channel0->GetBinder(), base::DoNothing())
          .has_value());
  auto channel1 = base::MakeRefCounted<TestChannel>();
  EXPECT_FALSE(
      ExchangeBinders(exchange1, channel1->GetBinder(), base::DoNothing())
          .has_value());
}

TEST_F(BinderExchangeTest, PlatformChannelPassing) {
  auto channel = base::MakeRefCounted<TestChannel>();
  auto [exchange0, exchange1] = CreateBinderExchange();
  PlatformChannel platform_channel(
      PlatformChannelEndpoint(PlatformHandle(std::move(exchange0))),
      PlatformChannelEndpoint(PlatformHandle(std::move(exchange1))));
  EXPECT_TRUE(channel->DoExchange(
      platform_channel.TakeLocalEndpoint().TakePlatformHandle().TakeBinder()));
  base::CommandLine cmd = base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions options;
  platform_channel.PrepareToPassRemoteEndpoint(&options, &cmd);
  auto child = base::SpawnMultiProcessTestChild("PlatformChannelPassing_Child",
                                                cmd, options);
  EXPECT_TRUE(channel->SendInt(999));
  EXPECT_EQ(99999, channel->WaitToReceiveInt());
  EXPECT_TRUE(JoinChild(child));
}

MULTIPROCESS_TEST_MAIN(PlatformChannelPassing_Child) {
  auto channel = base::MakeRefCounted<TestChannel>();
  auto endpoint = PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
  EXPECT_TRUE(channel->DoExchange(endpoint.TakePlatformHandle().TakeBinder()));
  EXPECT_TRUE(channel->SendInt(99999));
  EXPECT_EQ(999, channel->WaitToReceiveInt());
  RETURN_FROM_CHILD();
}

}  // namespace
}  // namespace mojo
