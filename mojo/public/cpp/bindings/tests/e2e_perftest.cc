// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/perf_time_logger.h"
#include "base/test/task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class EchoServiceImpl : public test::EchoService {
 public:
  EchoServiceImpl(PendingReceiver<test::EchoService> receiver,
                  base::OnceClosure quit_closure)
      : receiver_(this, std::move(receiver)),
        quit_closure_(std::move(quit_closure)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&EchoServiceImpl::OnDisconnect, base::Unretained(this)));
  }

  ~EchoServiceImpl() override { std::move(quit_closure_).Run(); }

  // EchoService:
  void Echo(const std::string& test_data, EchoCallback callback) override {
    std::move(callback).Run(test_data);
  }

 private:
  void OnDisconnect() { delete this; }

  Receiver<test::EchoService> receiver_;
  base::OnceClosure quit_closure_;
};

class PingPongTest {
 public:
  explicit PingPongTest(PendingRemote<test::EchoService> service)
      : service_(std::move(service)),
        ping_done_callback_(base::BindRepeating(&PingPongTest::OnPingDone,
                                                base::Unretained(this))) {}

  void RunTest(int iterations, int batch_size, int message_size);

 private:
  void DoPing();
  void OnPingDone(const std::string& reply);

  Remote<test::EchoService> service_;
  const base::RepeatingCallback<void(const std::string&)> ping_done_callback_;

  int iterations_;
  int batch_size_;
  std::string message_;

  int current_iterations_;
  int calls_outstanding_;

  base::OnceClosure quit_closure_;
};

void PingPongTest::RunTest(int iterations, int batch_size, int message_size) {
  iterations_ = iterations;
  batch_size_ = batch_size;
  message_ = std::string(message_size, 'a');
  current_iterations_ = 0;
  calls_outstanding_ = 0;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PingPongTest::DoPing, base::Unretained(this)));
  run_loop.Run();
}

void PingPongTest::DoPing() {
  DCHECK_EQ(0, calls_outstanding_);
  current_iterations_++;
  if (current_iterations_ > iterations_) {
    std::move(quit_closure_).Run();
    return;
  }

  calls_outstanding_ = batch_size_;
  for (int i = 0; i < batch_size_; i++) {
    service_->Echo(message_, ping_done_callback_);
  }
}

void PingPongTest::OnPingDone(const std::string& reply) {
  DCHECK_GT(calls_outstanding_, 0);
  calls_outstanding_--;

  if (!calls_outstanding_)
    DoPing();
}

class MojoE2EPerftest : public core::test::MojoTestBase {
 public:
  void RunTestOnTaskRunner(base::TaskRunner* runner,
                           MojoHandle client_mp,
                           const std::string& test_name) {
    if (runner == base::SingleThreadTaskRunner::GetCurrentDefault().get()) {
      RunTests(client_mp, test_name);
    } else {
      base::RunLoop run_loop;
      runner->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&MojoE2EPerftest::RunTests, base::Unretained(this),
                         client_mp, test_name),
          run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  void RunTests(MojoHandle client_mp, const std::string& test_name) {
    const int kMessages = 10000;
    const int kBatchSizes[] = {1, 10, 100};
    const int kMessageSizes[] = {8, 64, 512, 4096, 65536};

    PendingRemote<test::EchoService> service(
        ScopedMessagePipeHandle(MessagePipeHandle(client_mp)), 0);
    PingPongTest test(std::move(service));

    for (int batch_size : kBatchSizes) {
      for (int message_size : kMessageSizes) {
        int num_messages = kMessages;
        if (message_size == 65536)
          num_messages /= 10;
        std::string sub_test_name = base::StringPrintf(
            "%s/%dx%d/%dbytes", test_name.c_str(), num_messages / batch_size,
            batch_size, message_size);
        base::PerfTimeLogger timer(sub_test_name.c_str());
        test.RunTest(num_messages / batch_size, batch_size, message_size);
      }
    }
  }
};

void CreateAndRunService(PendingReceiver<test::EchoService> receiver,
                         base::OnceClosure quit_closure) {
  // Self-owned.
  new EchoServiceImpl(std::move(receiver), std::move(quit_closure));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(PingService, MojoE2EPerftest, mp) {
  MojoHandle service_mp;
  EXPECT_EQ("hello", ReadMessageWithHandles(mp, &service_mp, 1));

  auto receiver = PendingReceiver<test::EchoService>(
      ScopedMessagePipeHandle(MessagePipeHandle(service_mp)));
  base::RunLoop run_loop;
  core::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateAndRunService, std::move(receiver),
          base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                         task_environment_.GetMainThreadTaskRunner(), FROM_HERE,
                         run_loop.QuitClosure())));
  run_loop.Run();
}

TEST_F(MojoE2EPerftest, MultiProcessEchoMainThread) {
  RunTestClient("PingService", [&](MojoHandle mp) {
    MojoHandle client_mp, service_mp;
    CreateMessagePipe(&client_mp, &service_mp);
    WriteMessageWithHandles(mp, "hello", &service_mp, 1);
    RunTestOnTaskRunner(task_environment_.GetMainThreadTaskRunner().get(),
                        client_mp, "MultiProcessEchoMainThread");
  });
}

TEST_F(MojoE2EPerftest, MultiProcessEchoIoThread) {
  RunTestClient("PingService", [&](MojoHandle mp) {
    MojoHandle client_mp, service_mp;
    CreateMessagePipe(&client_mp, &service_mp);
    WriteMessageWithHandles(mp, "hello", &service_mp, 1);
    RunTestOnTaskRunner(core::GetIOTaskRunner().get(), client_mp,
                        "MultiProcessEchoIoThread");
  });
}

}  // namespace
}  // namespace mojo
