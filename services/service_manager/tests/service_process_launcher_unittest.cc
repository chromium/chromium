// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_process_launcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

const char kTestServiceName[] = "service_process_launcher_test_service";

#if defined(OS_WIN)
const base::FilePath::CharType kServiceExtension[] =
    FILE_PATH_LITERAL(".service.exe");
#else
const base::FilePath::CharType kServiceExtension[] =
    FILE_PATH_LITERAL(".service");
#endif

void ProcessReadyCallbackAdapter(bool expect_process_id_valid,
                                 base::OnceClosure callback,
                                 base::ProcessId process_id) {
  EXPECT_EQ(expect_process_id_valid, process_id != base::kNullProcessId);
  std::move(callback).Run();
}

class ServiceProcessLauncherDelegateImpl
    : public ServiceProcessLauncherDelegate {
 public:
  ServiceProcessLauncherDelegateImpl() {}
  ~ServiceProcessLauncherDelegateImpl() override {}

  size_t get_and_clear_adjust_count() {
    size_t count = 0;
    std::swap(count, adjust_count_);
    return count;
  }

 private:
  // ServiceProcessLauncherDelegate:
  void AdjustCommandLineArgumentsForTarget(
      const Identity& target,
      base::CommandLine* command_line) override {
    adjust_count_++;
  }

  size_t adjust_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

#if defined(OS_ANDROID)
// TODO(qsr): Multiprocess service manager tests are not supported on android.
#define MAYBE_StartJoin DISABLED_StartJoin
#else
#define MAYBE_StartJoin StartJoin
#endif  // defined(OS_ANDROID)
TEST(ServiceProcessLauncherTest, MAYBE_StartJoin) {
  base::test::TaskEnvironment task_environment;

  base::FilePath test_service_path;
#if defined(OS_FUCHSIA)
  // Service binaries are treated as "assets".
  base::PathService::Get(base::DIR_ASSETS, &test_service_path);
#else
  base::PathService::Get(base::DIR_EXE, &test_service_path);
#endif
  test_service_path = test_service_path.AppendASCII(kTestServiceName)
                          .AddExtension(kServiceExtension);

  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate;
  base::Optional<ServiceProcessLauncher> launcher(
      base::in_place, &service_process_launcher_delegate, test_service_path);
  base::RunLoop run_loop;
  launcher->Start(
      Identity(), SANDBOX_TYPE_NO_SANDBOX,
      base::BindOnce(&ProcessReadyCallbackAdapter,
                     true /*expect_process_id_valid*/, run_loop.QuitClosure()));
  run_loop.Run();

  launcher.reset();
  task_environment.RunUntilIdle();

  EXPECT_EQ(1u, service_process_launcher_delegate.get_and_clear_adjust_count());
}

#if !defined(OS_POSIX) || defined(OS_MACOSX)
// Verify that if ServiceProcessLauncher cannot launch a process running the
// service from the specified path, then we are able to clean up without e.g.
// double-freeing the platform-channel handle reserved for the peer.
// This test won't work as-is on POSIX platforms, where we use fork()+exec() to
// launch child processes, since we won't fail until exec(), therefore the test
// will see a valid child process-Id. We use posix_spawn() on Mac OS X.
TEST(ServiceProcessLauncherTest, FailToLaunchProcess) {
  base::test::TaskEnvironment task_environment;

  // Pick a service path that could not possibly ever exist.
  base::FilePath test_service_path(FILE_PATH_LITERAL("rockot@_rules.service"));

  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate;
  base::Optional<ServiceProcessLauncher> launcher(
      base::in_place, &service_process_launcher_delegate, test_service_path);
  base::RunLoop run_loop;
  launcher->Start(Identity(), SANDBOX_TYPE_NO_SANDBOX,
                  base::BindOnce(&ProcessReadyCallbackAdapter,
                                 false /*expect_process_id_valid*/,
                                 run_loop.QuitClosure()));
  run_loop.Run();

  launcher.reset();
  task_environment.RunUntilIdle();
}
#endif  //  !defined(OS_POSIX) || defined(OS_MACOSX)

}  // namespace
}  // namespace service_manager
