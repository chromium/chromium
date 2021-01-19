// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "services/tracing/perfetto/system_test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/system_tracing_service.h"

namespace tracing {

const char* kProducerSockEnvName = "PERFETTO_PRODUCER_SOCK_NAME";

namespace {

class SystemTracingServiceTest : public testing::Test {
 public:
  SystemTracingServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    // The test connects to the mock system service.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    system_service_ = std::make_unique<MockSystemService>(temp_dir_);

    // Override the default system producer socket.
    saved_producer_sock_env_ = getenv(kProducerSockEnvName);
    ASSERT_EQ(0, setenv(kProducerSockEnvName,
                        system_service_->producer().c_str(), 1));

    // Use the current thread as the Perfetto task runner.
    tracing::PerfettoTracedProcess::ResetTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    // Restore the value of Perfetto producer socket name env variable.
    if (saved_producer_sock_env_) {
      ASSERT_EQ(0,
                setenv(kProducerSockEnvName, saved_producer_sock_env_, true));
    } else {
      ASSERT_EQ(0, unsetenv(kProducerSockEnvName));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<MockSystemService> system_service_;
  const char* saved_producer_sock_env_ = nullptr;
};

// Test the OpenProducerSocket implementation. Expect a valid socket file
// descriptor returned in the callback.
TEST_F(SystemTracingServiceTest, OpenProducerSocket) {
  auto sts = std::make_unique<SystemTracingService>();
  bool callback_called = false;

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](base::File file) {
    callback_called = true;
    ASSERT_TRUE(file.IsValid());
    run_loop.Quit();
  });

  sts->OpenProducerSocket(std::move(callback));
  ASSERT_FALSE(callback_called);
  run_loop.Run();
  ASSERT_TRUE(callback_called);
}

// Test the OpenProducerSocket implementation with an nonexistent socket. Expect
// an invalid socket file descriptor returned in the callback.
TEST_F(SystemTracingServiceTest, OpenProducerSocket_Nonexistent) {
  auto sts = std::make_unique<SystemTracingService>();
  bool callback_called = false;

  // Set the producer socket name to a nonexistent path.
  saved_producer_sock_env_ = getenv(kProducerSockEnvName);
  ASSERT_EQ(0, setenv(kProducerSockEnvName, "nonexistent_socket", 1));

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](base::File file) {
    callback_called = true;
    // OpenProducerSocket fails and returns an invalid socket file descriptor.
    ASSERT_FALSE(file.IsValid());
    run_loop.Quit();
  });

  sts->OpenProducerSocket(std::move(callback));
  ASSERT_FALSE(callback_called);
  run_loop.Run();
  ASSERT_TRUE(callback_called);
}

// Test the OpenProducerSocket implementation through mojo. Expect that the
// callback runs when invoked through mojo.
TEST_F(SystemTracingServiceTest, BindAndPassPendingRemote) {
  auto sts = std::make_unique<SystemTracingService>();
  bool callback_called = false;

  // Bind the pending remote on the current thread.
  mojo::Remote<mojom::SystemTracingService> remote;
  remote.Bind(sts->BindAndPassPendingRemote(), nullptr);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](base::File file) {
    callback_called = true;
    ASSERT_TRUE(file.IsValid());
    run_loop.Quit();
  });

  remote->OpenProducerSocket(std::move(callback));
  ASSERT_FALSE(callback_called);
  run_loop.Run();
  ASSERT_TRUE(callback_called);
}

}  // namespace
}  // namespace tracing
