// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host.h"

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/wait.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/win/src/sandbox_types.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#endif

namespace content {

namespace {

const char kTestProcessName[] = "test_process";

constexpr std::string_view kTestMessage{"hello from shared memory"};

}  // namespace

class UtilityProcessHostBrowserTest : public BrowserChildProcessObserver,
                                      public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Add(this);

    host_ = new UtilityProcessHost();  // Owned by a global list.
    host_->SetName(u"TestProcess");
    host_->SetMetricsName(kTestProcessName);
  }

  void TearDownOnMainThread() override {
    // `host_` is about to be deleted during BrowserMainRunnerImpl::Shutdown().
    host_ = nullptr;
  }

  void SetExpectFailLaunch() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    expect_failed_launch_ = true;

#if BUILDFLAG(IS_WIN)
      // The Windows sandbox does not like the child process being a different
      // process, so launch unsandboxed for the purpose of this test.
    host_->SetSandboxType(sandbox::mojom::Sandbox::kNoSandbox);
#endif
    // Simulate a catastrophic launch failure for all child processes by
    // making the path to the process non-existent.
    base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
        switches::kBrowserSubprocessPath,
        base::FilePath(FILE_PATH_LITERAL("non_existent_path")));
  }

  void SetElevated() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
    host_->SetSandboxType(
        sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  }

  // After `service_` is bound, `run_test` is invoked, and then the RunLoop will
  // run.
  void RunUtilityProcess(base::OnceClosure run_test) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    done_closure_ =
        base::BindOnce(&UtilityProcessHostBrowserTest::DoneRunning,
                       base::Unretained(this), run_loop.QuitClosure());

    EXPECT_TRUE(host_->Start());

    host_->GetChildProcess()->BindServiceInterface(
        service_.BindNewPipeAndPassReceiver());

    std::move(run_test).Run();
    run_loop.Run();
  }

  void RunCrashImmediatelyTest() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    expect_crashed_ = true;
    service_->DoCrashImmediately(base::BindOnce(
        &UtilityProcessHostBrowserTest::OnSomething, base::Unretained(this)));
  }

  void RunSharedMemoryHandleTest() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // Verify that shared memory handles can be transferred to and from the
    // elevated process. This is only supported with MojoIpcz enabled.
    DCHECK(mojo::core::IsMojoIpczEnabled());
    auto region = base::WritableSharedMemoryRegion::Create(kTestMessage.size());
    auto mapping = region.Map();
    memcpy(mapping.memory(), kTestMessage.data(), kTestMessage.size());
    service_->CloneSharedMemoryContents(
        base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region)),
        base::BindOnce(&UtilityProcessHostBrowserTest::OnMemoryCloneReceived,
                       base::Unretained(this)));
  }

  void RunBasicPingPongTest() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    service_->DoSomething(base::BindOnce(
        &UtilityProcessHostBrowserTest::OnSomething, base::Unretained(this)));
  }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  void RunFileDescriptorStoreTest(base::ScopedFD read_fd) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    service_->WriteToPreloadedPipe();
    char buf[4];
    ASSERT_TRUE(base::ReadFromFD(read_fd.get(), buf));
    std::string_view msg(buf, sizeof(buf));
    ASSERT_EQ(msg, "test");
    OnSomething();
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

 protected:
  void DoneRunning(base::OnceClosure quit_closure) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Remove(this);
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kBrowserSubprocessPath);
    EXPECT_EQ(expect_crashed_, has_crashed_);
    EXPECT_EQ(expect_failed_launch_, has_failed_launch_);
    std::move(quit_closure).Run();
  }

  void ResetService() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    service_.reset();
  }

  void OnSomething() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If service crashes then this never gets called.
    ASSERT_EQ(false, expect_crashed_);
    ResetService();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  void OnMemoryCloneReceived(base::UnsafeSharedMemoryRegion region) {
    auto mapping = region.Map();
    ASSERT_EQ(kTestMessage.size(), mapping.size());
    EXPECT_EQ(kTestMessage,
              std::string_view(static_cast<const char*>(mapping.memory()),
                               kTestMessage.size()));
    ResetService();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  raw_ptr<UtilityProcessHost, AcrossTasksDanglingUntriaged> host_;
  mojo::Remote<mojom::TestService> service_;
  base::OnceClosure done_closure_;
  bool expect_crashed_ = false;
  bool expect_failed_launch_ = false;

  // Access on UI thread.
  bool has_crashed_ = false;
  bool has_failed_launch_ = false;

 private:
  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessKilled(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
    // Android does not send crash notifications but sends kills. See comment in
    // browser_child_process_observer.h.
    BrowserChildProcessCrashed(data, info);
#else
    FAIL() << "Killed notifications should only happen on Android.";
#endif
  }

  void BrowserChildProcessCrashed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_WIN)
// See crbug.com/40861868#comment17. There are two implementations of the
// DoCrashImmediately mojo interface, which causes official build to return
// a different exit_code.
#if defined(OFFICIAL_BUILD)
    EXPECT_EQ(STATUS_STACK_BUFFER_OVERRUN, static_cast<DWORD>(info.exit_code));
#else
    EXPECT_EQ(EXCEPTION_BREAKPOINT, static_cast<DWORD>(info.exit_code));
#endif  // defined(OFFICIAL_BUILD)
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_TRUE(WIFSIGNALED(info.exit_code));
#if defined(OFFICIAL_BUILD)
    EXPECT_EQ(SIGTRAP, WTERMSIG(info.exit_code));
#else   // defined(OFFICIAL_BUILD)
    EXPECT_EQ(SIGABRT, WTERMSIG(info.exit_code));
#endif  // defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(kTestProcessName, data.metrics_name);
    EXPECT_EQ(false, has_crashed_);
    has_crashed_ = true;
    ResetService();
    std::move(done_closure_).Run();
  }

  void BrowserChildProcessLaunchFailed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_EQ(info.status, base::TERMINATION_STATUS_LAUNCH_FAILED);
#if BUILDFLAG(IS_WIN)
    // On Windows, the sandbox code handles all non-elevated process launches.
    EXPECT_EQ(sandbox::SBOX_ERROR_CANNOT_LAUNCH_UNSANDBOXED_PROCESS,
              info.exit_code);
    // File not found because subprocess called 'non_existent_path.exe' does not
    // exist.
    EXPECT_EQ(DWORD{ERROR_FILE_NOT_FOUND}, info.last_error);
#else
    EXPECT_EQ(LAUNCH_RESULT_FAILURE, info.exit_code);
#endif
    EXPECT_EQ(kTestProcessName, data.metrics_name);
    has_failed_launch_ = true;
    ResetService();
    std::move(done_closure_).Run();
  }
};

IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchProcess) {
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunBasicPingPongTest,
                     base::Unretained(this)));
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

// TODO(crbug.com/40253015): Re-enable this test on Android when
// `files_to_preload` is actually fixed there.
// TODO(crbug.com/41484083): Re-enable this test on ChromeOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FileDescriptorStore DISABLED_FileDescriptorStore
#else
#define MAYBE_FileDescriptorStore FileDescriptorStore
#endif
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       MAYBE_FileDescriptorStore) {
  // Tests whether base::FileDescriptorStore works in content by passing it a
  // file descriptor for a pipe on launch. This test ensures the process is
  // launched without a zygote.
#if BUILDFLAG(USE_ZYGOTE)
  host_->SetZygoteForTesting(nullptr);
#endif

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));
  host_->AddFileToPreload(mojom::kTestPipeKey, std::move(write_fd));
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunFileDescriptorStoreTest,
                     base::Unretained(this), std::move(read_fd)));
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && BUILDFLAG(USE_ZYGOTE)
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       FileDescriptorStoreWithUnsandboxedZygote) {
  // Tests whether base::FileDescriptorStore works in content by passing it a
  // file descriptor for a pipe on launch. This test ensures the process is
  // launched with the unsandboxed zygote.
  host_->SetZygoteForTesting(GetUnsandboxedZygote());

  base::ScopedFD read_fd, write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));
  host_->AddFileToPreload(mojom::kTestPipeKey, std::move(write_fd));
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunFileDescriptorStoreTest,
                     base::Unretained(this), std::move(read_fd)));
}

IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       FileDescriptorStoreWithGenericZygote) {
  // Tests whether base::FileDescriptorStore works in content by passing it a
  // file descriptor for a pipe on launch. This test ensures the process is
  // launched with the generic zygote.
  host_->SetZygoteForTesting(GetGenericZygote());

  base::ScopedFD read_fd, write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));
  host_->AddFileToPreload(mojom::kTestPipeKey, std::move(write_fd));
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunFileDescriptorStoreTest,
                     base::Unretained(this), std::move(read_fd)));
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) &&
        // BUILDFLAG(USE_ZYGOTE)

// Disabled because it crashes on android-arm64-tests:
// https://crbug.com/1358585.
// TODO(crbug.com/41484083): Re-enable this test on ChromeOS.
#if !(BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64))
#if (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_X86_64)) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LaunchProcessAndCrash DISABLED_LaunchProcessAndCrash
#else
#define MAYBE_LaunchProcessAndCrash LaunchProcessAndCrash
#endif
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       MAYBE_LaunchProcessAndCrash) {
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunCrashImmediatelyTest,
                     base::Unretained(this)));
}
#endif

// This test won't work as-is on POSIX platforms, where fork()+exec() is used to
// launch child processes, failure does not happen until exec(), therefore the
// test will see a valid child process followed by a
// TERMINATION_STATUS_ABNORMAL_TERMINATION of the forked process. However,
// posix_spawn() is used on macOS.
// See also ServiceProcessLauncherTest.FailToLaunchProcess.
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, FailToLaunchProcess) {
  SetExpectFailLaunch();
  // If the ping-pong test completes, the test will fail because that means the
  // process did not fail to launch.
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunBasicPingPongTest,
                     base::Unretained(this)));
}
#endif  // !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchElevatedProcess) {
  SetElevated();
  RunUtilityProcess(
      mojo::core::IsMojoIpczEnabled()
          ? base::BindOnce(
                &UtilityProcessHostBrowserTest::RunSharedMemoryHandleTest,
                base::Unretained(this))
          : base::BindOnce(&UtilityProcessHostBrowserTest::RunBasicPingPongTest,
                           base::Unretained(this)));
}

// Disabled because currently this causes a WER dialog to appear.
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       DISABLED_LaunchElevatedProcessAndCrash) {
  SetElevated();
  RunUtilityProcess(
      base::BindOnce(&UtilityProcessHostBrowserTest::RunCrashImmediatelyTest,
                     base::Unretained(this)));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
