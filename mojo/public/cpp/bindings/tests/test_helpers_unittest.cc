// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom-test-utils.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestHelperTest : public testing::Test {
 public:
  TestHelperTest() = default;
  ~TestHelperTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(TestHelperTest);
};

class PingImpl : public test::PingService {
 public:
  explicit PingImpl(PendingReceiver<test::PingService> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~PingImpl() override = default;

  bool pinged() const { return pinged_; }

  // test::PingService:
  void Ping(PingCallback callback) override {
    pinged_ = true;
    std::move(callback).Run();
  }

 private:
  bool pinged_ = false;
  Receiver<test::PingService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(PingImpl);
};

class EchoImpl : public test::EchoService {
 public:
  explicit EchoImpl(PendingReceiver<test::EchoService> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~EchoImpl() override = default;

  // test::EchoService:
  void Echo(const std::string& message, EchoCallback callback) override {
    std::move(callback).Run(message);
  }

 private:
  Receiver<test::EchoService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(EchoImpl);
};

class TrampolineImpl : public test::HandleTrampoline {
 public:
  explicit TrampolineImpl(PendingReceiver<test::HandleTrampoline> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TrampolineImpl() override = default;

  // test::HandleTrampoline:
  void BounceOne(ScopedMessagePipeHandle one,
                 BounceOneCallback callback) override {
    std::move(callback).Run(std::move(one));
  }

  void BounceTwo(ScopedMessagePipeHandle one,
                 ScopedMessagePipeHandle two,
                 BounceTwoCallback callback) override {
    std::move(callback).Run(std::move(one), std::move(two));
  }

 private:
  Receiver<test::HandleTrampoline> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TrampolineImpl);
};

TEST_F(TestHelperTest, AsyncWaiter) {
  Remote<test::PingService> ping;
  PingImpl ping_impl(ping.BindNewPipeAndPassReceiver());

  test::PingServiceAsyncWaiter wait_for_ping(ping.get());
  EXPECT_FALSE(ping_impl.pinged());
  wait_for_ping.Ping();
  EXPECT_TRUE(ping_impl.pinged());

  Remote<test::EchoService> echo;
  EchoImpl echo_impl(echo.BindNewPipeAndPassReceiver());

  test::EchoServiceAsyncWaiter wait_for_echo(echo.get());
  const std::string kTestString = "a machine that goes 'ping'";
  std::string response;
  wait_for_echo.Echo(kTestString, &response);
  EXPECT_EQ(kTestString, response);

  Remote<test::HandleTrampoline> trampoline;
  TrampolineImpl trampoline_impl(trampoline.BindNewPipeAndPassReceiver());

  test::HandleTrampolineAsyncWaiter wait_for_trampoline(trampoline.get());
  MessagePipe pipe;
  ScopedMessagePipeHandle handle0, handle1;
  WriteMessageRaw(pipe.handle0.get(), kTestString.data(), kTestString.size(),
                  nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  wait_for_trampoline.BounceOne(std::move(pipe.handle0), &handle0);
  wait_for_trampoline.BounceTwo(std::move(handle0), std::move(pipe.handle1),
                                &handle0, &handle1);

  // Verify that our pipe handles are the same as the original pipe.
  Wait(handle1.get(), MOJO_HANDLE_SIGNAL_READABLE);
  std::vector<uint8_t> payload;
  ReadMessageRaw(handle1.get(), &payload, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  std::string original_message(payload.begin(), payload.end());
  EXPECT_EQ(kTestString, original_message);
}

}  // namespace
}  // namespace mojo
