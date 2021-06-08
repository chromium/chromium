// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <sys/wait.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif  // OS_WIN

namespace content {

namespace {

const char kTestProcessName[] = "test_process";

}  // namespace

class UtilityProcessHostBrowserTest : public BrowserChildProcessObserver,
                                      public ContentBrowserTest {
 public:
  void RunUtilityProcess(bool elevated, bool crash) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Add(this);
    has_crashed = false;
    base::RunLoop run_loop;
    done_closure_ =
        base::BindOnce(&UtilityProcessHostBrowserTest::DoneRunning,
                       base::Unretained(this), run_loop.QuitClosure(), crash);
    auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                           ? content::GetUIThreadTaskRunner({})
                           : content::GetIOThreadTaskRunner({});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &UtilityProcessHostBrowserTest::RunUtilityProcessOnProcessThread,
            base::Unretained(this), elevated, crash));
    run_loop.Run();
  }

 protected:
  void DoneRunning(base::OnceClosure quit_closure, bool expect_crashed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserChildProcessObserver::Remove(this);
    EXPECT_EQ(expect_crashed, has_crashed);
    std::move(quit_closure).Run();
  }

  void RunUtilityProcessOnProcessThread(bool elevated, bool crash) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    UtilityProcessHost* host = new UtilityProcessHost();
    host->SetName(u"TestProcess");
    host->SetMetricsName(kTestProcessName);
#if defined(OS_WIN)
    if (elevated)
      host->SetSandboxType(
          sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges);
#endif
    EXPECT_TRUE(host->Start());

    host->GetChildProcess()->BindReceiver(
        service_.BindNewPipeAndPassReceiver());
    if (crash) {
      service_->DoCrashImmediately(base::BindOnce(
          &UtilityProcessHostBrowserTest::OnSomethingOnProcessThread,
          base::Unretained(this), crash));
    } else {
      service_->DoSomething(base::BindOnce(
          &UtilityProcessHostBrowserTest::OnSomethingOnProcessThread,
          base::Unretained(this), crash));
    }
  }

  void ResetServiceOnProcessThread() {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    service_.reset();
  }

  void OnSomethingOnProcessThread(bool expect_crash) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    // If service crashes then this never gets called.
    ASSERT_EQ(false, expect_crash);
    ResetServiceOnProcessThread();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  mojo::Remote<mojom::TestService> service_;
  base::OnceClosure done_closure_;

  // Access on UI thread.
  bool has_crashed;

 private:
  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessKilled(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
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
#if defined(OS_WIN)
    EXPECT_EQ(EXCEPTION_BREAKPOINT, static_cast<DWORD>(info.exit_code));
#elif defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    EXPECT_TRUE(WIFSIGNALED(info.exit_code));
    EXPECT_EQ(SIGTRAP, WTERMSIG(info.exit_code));
#endif
    EXPECT_EQ(kTestProcessName, data.metrics_name);
    EXPECT_EQ(false, has_crashed);
    has_crashed = true;
    auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                           ? content::GetUIThreadTaskRunner({})
                           : content::GetIOThreadTaskRunner({});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &UtilityProcessHostBrowserTest::ResetServiceOnProcessThread,
            base::Unretained(this)));
    std::move(done_closure_).Run();
  }
};

IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchProcess) {
  RunUtilityProcess(false, false);
}

IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest, LaunchProcessAndCrash) {
  RunUtilityProcess(false, true);
}

#if defined(OS_WIN)
// Times out. crbug.com/927298.
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       DISABLED_LaunchElevatedProcess) {
  RunUtilityProcess(true, false);
}

// Disabled because currently this causes a WER dialog to appear.
IN_PROC_BROWSER_TEST_F(UtilityProcessHostBrowserTest,
                       DISABLED_LaunchElevatedProcessAndCrash) {
  RunUtilityProcess(true, true);
}
#endif

}  // namespace content
