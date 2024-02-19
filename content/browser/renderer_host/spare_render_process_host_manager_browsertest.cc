// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/spare_render_process_host_manager.h"

#include <utility>

#include "base/callback_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SpareRenderProcessHostManagerTest : public ContentBrowserTest,
                                          public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManagerTest() = default;

 protected:
  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    // Platforms that don't isolate sites won't create spare processes and
    // the test will fail. Therefore, enforce the site isolation here.
    IsolateAllSitesForTesting(command_line);
  }

  void SetProcessExitCallback(RenderProcessHost* rph,
                              base::OnceClosure callback) {
    Observe(rph);
    process_exit_callback_ = std::move(callback);
  }

  void Observe(RenderProcessHost* rph) {
    DCHECK(!observation_.IsObserving());
    observation_.Observe(rph);
  }

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    if (process_exit_callback_) {
      std::move(process_exit_callback_).Run();
    }
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    observation_.Reset();
  }

 private:
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_{this};
  base::OnceClosure process_exit_callback_;
};

// This test verifies the creation of a deferred spare renderer. It checks two
// conditions:
//  1. A spare renderer is created successfully under standard conditions.
//  2. No spare renderer is created if the browser context is destroyed.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       DeferredSpareProcess) {
  constexpr base::TimeDelta kDelay = base::Seconds(1);

  base::HistogramTester histogram_tester;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      new base::TestMockTimeTaskRunner();
  auto& manager = SpareRenderProcessHostManager::GetInstance();

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto browser_context = std::make_unique<ShellBrowserContext>(true);

  // Check that a spare renderer is created successfully under standard
  // conditions.
  bool renderer_created = false;
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      manager.RegisterSpareRenderProcessHostChangedCallback(base::BindRepeating(
          [](base::RunLoop* run_loop, bool* renderer_created,
             RenderProcessHost* render_process_host) {
            if (render_process_host) {
              *renderer_created = true;
              run_loop->Quit();
            }
          },
          &run_loop, &renderer_created));

  manager.PrepareForFutureRequests(browser_context.get(), kDelay);
  EXPECT_EQ(manager.spare_render_process_host(), nullptr);

  // Wait until the renderer process is successfully started.
  run_loop.Run();
  // The spare renderer should be created.
  EXPECT_NE(manager.spare_render_process_host(), nullptr);
  EXPECT_TRUE(renderer_created);
  histogram_tester.ExpectTotalCount(
      "BrowserRenderProcessHost.SpareProcessStartupTime", 1);
  histogram_tester.ExpectTotalCount(
      "BrowserRenderProcessHost.SpareProcessDelayTime", 1);

  // Reset the spare renderer manager.
  manager.CleanupSpareRenderProcessHost();
  EXPECT_EQ(manager.spare_render_process_host(), nullptr);

  // Check that no spare renderer is created if the browser context is
  // destroyed.
  manager.PrepareForFutureRequests(browser_context.get(), kDelay);
  browser_context.reset();
  RunAllTasksUntilIdle();

  // The spare renderer shouldn't be created.
  EXPECT_EQ(manager.spare_render_process_host(), nullptr);
  histogram_tester.ExpectTotalCount(
      "BrowserRenderProcessHost.SpareProcessStartupTime", 1);
  histogram_tester.ExpectTotalCount(
      "BrowserRenderProcessHost.SpareProcessDelayTime", 1);
}

IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRenderProcessHostTaken) {
  ASSERT_TRUE(embedded_test_server()->Start());

  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_NE(nullptr, spare_renderer);

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  Shell* window = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(window, test_url));

  EXPECT_EQ(spare_renderer,
            window->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // The old spare render process host should no longer be available.
  EXPECT_NE(spare_renderer,
            RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());

  // Check if a fresh spare is available (depending on the operating mode).
  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    EXPECT_NE(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  } else {
    EXPECT_EQ(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRenderProcessHostNotTaken) {
  ASSERT_TRUE(embedded_test_server()->Start());

  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->off_the_record_browser_context());
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  Shell* window = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(window, test_url));

  // There should have been another process created for the navigation.
  EXPECT_NE(spare_renderer,
            window->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Check if a fresh spare is available (depending on the operating mode).
  // Note this behavior is identical to what would have happened if the
  // RenderProcessHost were taken.
  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    EXPECT_NE(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  } else {
    EXPECT_EQ(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRenderProcessHostKilled) {
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());

  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  mojo::Remote<mojom::TestService> service;
  ASSERT_NE(nullptr, spare_renderer);
  spare_renderer->BindReceiver(service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  SetProcessExitCallback(spare_renderer, run_loop.QuitClosure());

  // Should reply with a bad message and cause process death.
  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(spare_renderer);
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  // The spare RenderProcessHost should disappear when its process dies.
  EXPECT_EQ(nullptr,
            RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
}

// A mock ContentBrowserClient that only considers a spare renderer to be a
// suitable host.
class SpareRendererContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool IsSuitableHost(RenderProcessHost* process_host,
                      const GURL& site_url) override {
    if (RenderProcessHostImpl::GetSpareRenderProcessHostForTesting()) {
      return process_host ==
             RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
    }
    return true;
  }
};

// A mock ContentBrowserClient that only considers a non-spare renderer to be a
// suitable host, but otherwise tries to reuse processes.
class NonSpareRendererContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  NonSpareRendererContentBrowserClient() = default;

  NonSpareRendererContentBrowserClient(
      const NonSpareRendererContentBrowserClient&) = delete;
  NonSpareRendererContentBrowserClient& operator=(
      const NonSpareRendererContentBrowserClient&) = delete;

  bool IsSuitableHost(RenderProcessHost* process_host,
                      const GURL& site_url) override {
    return RenderProcessHostImpl::GetSpareRenderProcessHostForTesting() !=
           process_host;
  }

  bool ShouldTryToUseExistingProcessHost(BrowserContext* context,
                                         const GURL& url) override {
    return true;
  }

  bool ShouldUseSpareRenderProcessHost(BrowserContext* browser_context,
                                       const GURL& site_url) override {
    return false;
  }
};

// Test that the spare renderer works correctly when the limit on the maximum
// number of processes is small.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRendererSurpressedMaxProcesses) {
  ASSERT_TRUE(embedded_test_server()->Start());

  SpareRendererContentBrowserClient browser_client;

  RenderProcessHost::SetMaxRendererProcessCount(1);

  // A process is created with shell startup, so with a maximum of one renderer
  // process the spare RPH should not be created.
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  EXPECT_EQ(nullptr,
            RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());

  // A spare RPH should be created with a max of 2 renderer processes.
  RenderProcessHost::SetMaxRendererProcessCount(2);
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_NE(nullptr, spare_renderer);

  // Thanks to the injected SpareRendererContentBrowserClient and the limit on
  // processes, the spare RPH will always be used via GetExistingProcessHost()
  // rather than picked up via MaybeTakeSpareRenderProcessHost().
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  Shell* new_window = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_window, test_url));
  // Outside of RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes mode, the
  // spare RPH should have been dropped during CreateBrowser() and given to the
  // new window.  OTOH, even in the IsSpareProcessKeptAtAllTimes mode, the spare
  // shouldn't be created because of the low process limit.
  EXPECT_EQ(nullptr,
            RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  EXPECT_EQ(spare_renderer,
            new_window->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Revert to the default process limit and original ContentBrowserClient.
  RenderProcessHost::SetMaxRendererProcessCount(0);
}

// Check that the spare renderer is dropped if an existing process is reused.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRendererOnProcessReuse) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NonSpareRendererContentBrowserClient browser_client;

  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_NE(nullptr, spare_renderer);

  // This should reuse the existing process.
  Shell* new_browser = CreateBrowser();
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_browser->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(spare_renderer,
            new_browser->web_contents()->GetPrimaryMainFrame()->GetProcess());
  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    EXPECT_NE(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  } else {
    EXPECT_EQ(nullptr,
              RenderProcessHostImpl::GetSpareRenderProcessHostForTesting());
  }

  // The launcher thread reads state from browser_client, need to wait for it to
  // be done before resetting the browser client. crbug.com/742533.
  base::WaitableEvent launcher_thread_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce([](base::WaitableEvent* done) { done->Signal(); },
                     base::Unretained(&launcher_thread_done)));
  ASSERT_TRUE(launcher_thread_done.TimedWait(TestTimeouts::action_timeout()));
}

// Verifies that the spare renderer maintained by SpareRenderProcessHostManager
// is correctly destroyed during browser shutdown.  This test is an analogue
// to the //chrome-layer FastShutdown.SpareRenderProcessHost test.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRenderProcessHostDuringShutdown) {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      shell()->web_contents()->GetBrowserContext());

  // The verification is that there are no DCHECKs anywhere during test tear
  // down.
}

// Verifies that the spare renderer maintained by SpareRenderProcessHostManager
// is correctly destroyed when closing the last content shell.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareRendererDuringClosing) {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      shell()->web_contents()->GetBrowserContext());
  shell()->web_contents()->Close();

  // The verification is that there are no DCHECKs or UaF anywhere during test
  // tear down.
}

// This test verifies that SpareRenderProcessHostManager correctly accounts
// for StoragePartition differences when handing out the spare process.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareProcessVsCustomStoragePartition) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Provide custom storage partition for test sites.
  GURL test_url = embedded_test_server()->GetURL("a.com", "/simple_page.html");
  CustomStoragePartitionBrowserClient modified_client(GURL("http://a.com/"));

  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  scoped_refptr<SiteInstance> test_site_instance =
      SiteInstance::CreateForURL(browser_context, test_url);
  StoragePartition* default_storage =
      browser_context->GetDefaultStoragePartition();
  StoragePartition* custom_storage =
      browser_context->GetStoragePartition(test_site_instance.get());
  EXPECT_NE(default_storage, custom_storage);

  // Open a test window - it should be associated with the default storage
  // partition.
  Shell* window = CreateBrowser();
  RenderProcessHost* old_process =
      window->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(default_storage, old_process->GetStoragePartition());

  // Warm up the spare process - it should be associated with the default
  // storage partition.
  RenderProcessHost::WarmupSpareRenderProcessHost(browser_context);
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  ASSERT_TRUE(spare_renderer);
  EXPECT_EQ(default_storage, spare_renderer->GetStoragePartition());

  // Navigate to a URL that requires a custom storage partition.
  EXPECT_TRUE(NavigateToURL(window, test_url));
  RenderProcessHost* new_process =
      window->web_contents()->GetPrimaryMainFrame()->GetProcess();
  // Requirement to use a custom storage partition should force a process swap.
  EXPECT_NE(new_process, old_process);
  // The new process should be associated with the custom storage partition.
  EXPECT_EQ(custom_storage, new_process->GetStoragePartition());
  // And consequently, the spare shouldn't have been used.
  EXPECT_NE(spare_renderer, new_process);
}

class RenderProcessHostObserverCounter : public RenderProcessHostObserver {
 public:
  explicit RenderProcessHostObserverCounter(RenderProcessHost* host) {
    host->AddObserver(this);
    observing_ = true;
    observed_host_ = host;
  }

  RenderProcessHostObserverCounter(const RenderProcessHostObserverCounter&) =
      delete;
  RenderProcessHostObserverCounter& operator=(
      const RenderProcessHostObserverCounter&) = delete;

  ~RenderProcessHostObserverCounter() override {
    if (observing_) {
      observed_host_->RemoveObserver(this);
    }
  }

  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    DCHECK(observing_);
    DCHECK_EQ(host, observed_host_);
    exited_count_++;
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK(observing_);
    DCHECK_EQ(host, observed_host_);
    destroyed_count_++;

    host->RemoveObserver(this);
    observing_ = false;
    observed_host_ = nullptr;
  }

  int exited_count() const { return exited_count_; }
  int destroyed_count() const { return destroyed_count_; }

 private:
  int exited_count_ = 0;
  int destroyed_count_ = 0;
  bool observing_ = false;
  raw_ptr<RenderProcessHost> observed_host_ = nullptr;
};

// Check that the spare renderer is properly destroyed via DisableRefCounts().
// Note: DisableRefCounts() used to be called DisableKeepAliveRefCount();
// the name if this test is left unchanged to avoid disrupt any tracking
// tools (e.g. flakiness) that might reference the old name.
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest,
                       SpareVsDisableKeepAliveRefCount) {
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  base::RunLoop().RunUntilIdle();

  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  RenderProcessHostObserverCounter counter(spare_renderer);

  RenderProcessHostWatcher process_watcher(
      spare_renderer, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  spare_renderer->DisableRefCounts();

  process_watcher.Wait();
  EXPECT_TRUE(process_watcher.did_exit_normally());

  // An important part of test verification is that UaF doesn't happen in the
  // next revolution of the message pump - without extra care in the
  // SpareRenderProcessHostManager RenderProcessHost::Cleanup could be called
  // twice leading to a crash caused by double-free flavour of UaF in
  // base::DeleteHelper<...>::DoDelete.
  base::RunLoop().RunUntilIdle();

  DCHECK_EQ(1, counter.exited_count());
  DCHECK_EQ(1, counter.destroyed_count());
}

// Check that the spare renderer is properly destroyed via DisableRefCounts().
IN_PROC_BROWSER_TEST_F(SpareRenderProcessHostManagerTest, SpareVsFastShutdown) {
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  base::RunLoop().RunUntilIdle();

  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  RenderProcessHostObserverCounter counter(spare_renderer);

  RenderProcessHostWatcher process_watcher(
      spare_renderer, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  spare_renderer->FastShutdownIfPossible();

  process_watcher.Wait();
  EXPECT_TRUE(process_watcher.did_exit_normally());

  // An important part of test verification is that UaF doesn't happen in the
  // next revolution of the message pump - without extra care in the
  // SpareRenderProcessHostManager RenderProcessHost::Cleanup could be called
  // twice leading to a crash caused by double-free flavour of UaF in
  // base::DeleteHelper<...>::DoDelete.
  base::RunLoop().RunUntilIdle();

  DCHECK_EQ(1, counter.exited_count());
  DCHECK_EQ(1, counter.destroyed_count());
}

}  // namespace content
