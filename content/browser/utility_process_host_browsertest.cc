// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
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

namespace content {

namespace {

const char kTestProcessName[] = "test_process";

constexpr base::StringPiece kTestMessage{"hello from shared memory"};

}  // namespace

class UtilityProcessHostBrowserTest : public BrowserChildProcessObserver,
                                      public ContentBrowserTest {
 public:
  void RunUtilityProcess(bool elevated, bool crash, bool fail_launch) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Add(this);
    has_crashed = false;
    has_failed_launch = false;
    base::RunLoop run_loop;
    done_closure_ = base::BindOnce(&UtilityProcessHostBrowserTest::DoneRunning,
                                   base::Unretained(this),
                                   run_loop.QuitClosure(), crash, fail_launch);

    UtilityProcessHost* host = new UtilityProcessHost();
    host->SetName(u"TestProcess");
    host->SetMetricsName(kTestProcessName);
    if (fail_launch) {
#if BUILDFLAG(IS_WIN)
      // The Windows sandbox does not like the child process being a different
      // process, so launch unsandboxed for the purpose of this test.
      host->SetSandboxType(sandbox::mojom::Sandbox::kNoSandbox);
#endif
      // Simulate a catastrophic launch failure for all child processes by
      // making the path to the process non-existent.
      base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
          switches::kBrowserSubprocessPath,
          base::FilePath(FILE_PATH_LITERAL("non_existent_path")));
    }
#if BUILDFLAG(IS_WIN)
    if (elevated) {
      host->SetSandboxType(
          sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges);
    }
#endif
    EXPECT_TRUE(host->Start());

    host->GetChildProcess()->BindServiceInterface(
        service_.BindNewPipeAndPassReceiver());
    if (crash) {
      service_->DoCrashImmediately(
          base::BindOnce(&UtilityProcessHostBrowserTest::OnSomething,
                         base::Unretained(this), crash));
    } else if (elevated && mojo::core::IsMojoIpczEnabled()) {
      // Verify that shared memory handles can be transferred to and from the
      // elevated process. This is only supported with MojoIpcz enabled.
      auto region =
          base::WritableSharedMemoryRegion::Create(kTestMessage.size());
      auto mapping = region.Map();
      memcpy(mapping.memory(), kTestMessage.data(), kTestMessage.size());
      service_->CloneSharedMemoryContents(
          base::WritableSharedMemoryRegion::ConvertToReadOnly(
              std::move(region)),
          base::BindOnce(&UtilityProcessHostBrowserTest::OnMemoryCloneReceived,
                         base::Unretained(this)));
    } else {
      service_->DoSomething(
          base::BindOnce(&UtilityProcessHostBrowserTest::OnSomething,
                         base::Unretained(this), crash));
    }

    run_loop.Run();
  }

 protected:
  void DoneRunning(base::OnceClosure quit_closure,
                   bool expect_crashed,
                   bool expect_failed_launch) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Remove(this);
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kBrowserSubprocessPath);
    EXPECT_EQ(expect_crashed, has_crashed);
    EXPECT_EQ(expect_failed_launch, has_failed_launch);
    std::move(quit_closure).Run();
  }

  void ResetService() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    service_.reset();
  }

  void OnSomething(bool expect_crash) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If service crashes then this never gets called.
    ASSERT_EQ(false, expect_crash);
    ResetService();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  void OnMemoryCloneReceived(base::UnsafeSharedMemoryRegion region) {
    auto mapping = region.Map();
    ASSERT_EQ(kTestMessage.size(), mapping.size());
    EXPECT_EQ(kTestMessage,
              base::StringPiece(static_cast<const char*>(mapping.memory()),
                                kTestMessage.size()));
    ResetService();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  mojo::Remote<mojom::TestService> service_;
  base::OnceClosure done_closure_;

  // Access on UI thread.
  bool has_crashed;
  bool has_failed_launch;

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
    EXPECT_EQ(EXCEPTION_BREAKPOINT, static_cast<DWORD>(info.exit_code));
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_TRUE(WIFSIGNALED(info.exit_code));
    EXPECT_EQ(SIGTRAP, WTERMSIG(info.exit_code));
#endif
    EXPECT_EQ(kTestProcessName, data.metrics_name);
    EXPECT_EQ(false, has_crashed);
    has_crashed = true;
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
    has_failed_launch = true;
    ResetService();
    std::move(done_closure_).Run();
  }
};

IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchProcess) {
  RunUtilityProcess(/*elevated=*/false, /*crash=*/false, /*fail_launch=*/false);
}

// Disabled because it crashes on android-arm64-tests:
// https://crbug.com/1358585.
#if !(BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64))
#if BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_X86_64)
#define MAYBE_LaunchProcessAndCrash DISABLED_LaunchProcessAndCrash
#else
#define MAYBE_LaunchProcessAndCrash LaunchProcessAndCrash
#endif
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       MAYBE_LaunchProcessAndCrash) {
  RunUtilityProcess(/*elevated=*/false, /*crash=*/true, /*fail_launch=*/false);
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
  RunUtilityProcess(/*elevated=*/false, /*crash=*/false, /*fail_launch=*/true);
}
#endif  // !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchElevatedProcess) {
  RunUtilityProcess(/*elevated=*/true, /*crash=*/false, /*fail_launch=*/false);
}

// Disabled because currently this causes a WER dialog to appear.
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       DISABLED_LaunchElevatedProcessAndCrash) {
  RunUtilityProcess(/*elevated=*/true, /*crash=*/true, /*fail_launch=*/false);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
