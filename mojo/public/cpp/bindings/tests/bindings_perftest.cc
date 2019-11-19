// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const double kMojoTicksPerSecond = 1000000.0;

const char kTestInterfaceName[] = "TestInterface";

double MojoTicksToSeconds(MojoTimeTicks ticks) {
  return ticks / kMojoTicksPerSecond;
}

class PingServiceImpl : public test::PingService {
 public:
  PingServiceImpl() = default;
  ~PingServiceImpl() override = default;

  // |PingService| methods:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingServiceImpl);
};

class PingPongTest {
 public:
  explicit PingPongTest(PendingRemote<test::PingService> remote)
      : remote_(std::move(remote)) {}

  void Run(unsigned int iterations) {
    iterations_to_run_ = iterations;
    current_iterations_ = 0;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    remote_->Ping(
        base::BindOnce(&PingPongTest::OnPingDone, base::Unretained(this)));
    run_loop.Run();
  }

 private:
  void OnPingDone() {
    current_iterations_++;
    if (current_iterations_ >= iterations_to_run_) {
      std::move(quit_closure_).Run();
      return;
    }

    remote_->Ping(
        base::BindOnce(&PingPongTest::OnPingDone, base::Unretained(this)));
  }

  Remote<test::PingService> remote_;
  unsigned int iterations_to_run_;
  unsigned int current_iterations_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(PingPongTest);
};

struct BoundPingService {
  BoundPingService() : receiver(&impl, remote.BindNewPipeAndPassReceiver()) {}

  PingServiceImpl impl;
  Remote<test::PingService> remote;
  Receiver<test::PingService> receiver;
};

class MojoBindingsPerftest : public testing::Test {
 public:
  MojoBindingsPerftest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MojoBindingsPerftest, InProcessPingPong) {
  PendingRemote<test::PingService> remote;
  PingServiceImpl impl;
  Receiver<test::PingService> receiver(&impl,
                                       remote.InitWithNewPipeAndPassReceiver());
  PingPongTest test(std::move(remote));

  {
    const unsigned int kIterations = 100000;
    const MojoTimeTicks start_time = MojoGetTimeTicksNow();
    test.Run(kIterations);
    const MojoTimeTicks end_time = MojoGetTimeTicksNow();
    test::LogPerfResult(
        "InProcessPingPong", "0_Inactive",
        kIterations / MojoTicksToSeconds(end_time - start_time),
        "pings/second");
  }

  {
    const size_t kNumInactiveServices = 1000;
    BoundPingService* inactive_services =
        new BoundPingService[kNumInactiveServices];

    const unsigned int kIterations = 10000;
    const MojoTimeTicks start_time = MojoGetTimeTicksNow();
    test.Run(kIterations);
    const MojoTimeTicks end_time = MojoGetTimeTicksNow();
    test::LogPerfResult(
        "InProcessPingPong", "1000_Inactive",
        kIterations / MojoTicksToSeconds(end_time - start_time),
        "pings/second");

    delete[] inactive_services;
  }
}

class PingPongPaddle : public MessageReceiverWithResponderStatus {
 public:
  PingPongPaddle(MessageReceiver* sender) : sender_(sender) {}

  void set_sender(MessageReceiver* sender) { sender_ = sender; }

  bool Accept(Message* message) override {
    uint32_t count = message->header()->name;
    if (!quit_closure_.is_null()) {
      count++;
      if (count >= expected_count_) {
        end_time_ = base::TimeTicks::Now();
        quit_closure_.Run();
        return true;
      }
    }

    Message reply(count, 0, 0, 0, nullptr);
    bool result = sender_->Accept(&reply);
    DCHECK(result);
    return true;
  }

  bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) override {
    NOTREACHED();
    return true;
  }

  base::TimeDelta Serve(uint32_t expected_count) {
    base::RunLoop run_loop;

    expected_count_ = expected_count;
    quit_closure_ = run_loop.QuitClosure();

    start_time_ = base::TimeTicks::Now();
    Message message(0, 0, 0, 0, nullptr);
    bool result = sender_->Accept(&message);
    DCHECK(result);

    run_loop.Run();

    return end_time_ - start_time_;
  }

 private:
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  uint32_t expected_count_ = 0;
  MessageReceiver* sender_;
  base::RepeatingClosure quit_closure_;
};

TEST_F(MojoBindingsPerftest, MultiplexRouterPingPong) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router0(
      new internal::MultiplexRouter(std::move(pipe.handle0),
                                    internal::MultiplexRouter::SINGLE_INTERFACE,
                                    true, base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<internal::MultiplexRouter> router1(
      new internal::MultiplexRouter(
          std::move(pipe.handle1), internal::MultiplexRouter::SINGLE_INTERFACE,
          false, base::ThreadTaskRunnerHandle::Get()));

  PingPongPaddle paddle0(nullptr);
  PingPongPaddle paddle1(nullptr);

  InterfaceEndpointClient client0(
      router0->CreateLocalEndpointHandle(kMasterInterfaceId), &paddle0, nullptr,
      false, base::ThreadTaskRunnerHandle::Get(), 0u, kTestInterfaceName);
  InterfaceEndpointClient client1(
      router1->CreateLocalEndpointHandle(kMasterInterfaceId), &paddle1, nullptr,
      false, base::ThreadTaskRunnerHandle::Get(), 0u, kTestInterfaceName);

  paddle0.set_sender(&client0);
  paddle1.set_sender(&client1);

  static const uint32_t kWarmUpIterations = 1000;
  static const uint32_t kTestIterations = 1000000;

  paddle0.Serve(kWarmUpIterations);

  base::TimeDelta duration = paddle0.Serve(kTestIterations);

  test::LogPerfResult("MultiplexRouterPingPong", nullptr,
                      kTestIterations / duration.InSecondsF(), "pings/second");
}

class CounterReceiver : public MessageReceiverWithResponderStatus {
 public:
  bool Accept(Message* message) override {
    counter_++;
    return true;
  }

  bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) override {
    NOTREACHED();
    return true;
  }

  uint32_t counter() const { return counter_; }

  void Reset() { counter_ = 0; }

 private:
  uint32_t counter_ = 0;
};

TEST_F(MojoBindingsPerftest, MultiplexRouterDispatchCost) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router(new internal::MultiplexRouter(
      std::move(pipe.handle0), internal::MultiplexRouter::SINGLE_INTERFACE,
      true, base::ThreadTaskRunnerHandle::Get()));
  CounterReceiver receiver;
  InterfaceEndpointClient client(
      router->CreateLocalEndpointHandle(kMasterInterfaceId), &receiver, nullptr,
      false, base::ThreadTaskRunnerHandle::Get(), 0u, kTestInterfaceName);

  static const uint32_t kIterations[] = {1000, 3000000};

  for (size_t i = 0; i < 2; ++i) {
    receiver.Reset();
    base::TimeTicks start_time = base::TimeTicks::Now();
    for (size_t j = 0; j < kIterations[i]; ++j) {
      Message message(0, 0, 8, 0, nullptr);
      bool result = router->SimulateReceivingMessageForTesting(&message);
      DCHECK(result);
    }

    base::TimeTicks end_time = base::TimeTicks::Now();
    base::TimeDelta duration = end_time - start_time;
    CHECK_EQ(kIterations[i], receiver.counter());

    if (i == 1) {
      test::LogPerfResult("MultiplexRouterDispatchCost", nullptr,
                          kIterations[i] / duration.InSecondsF(),
                          "times/second");
    }
  }
}

}  // namespace
}  // namespace mojo
