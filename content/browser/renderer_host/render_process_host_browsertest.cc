// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

namespace {

// Similar to net::test_server::DelayedHttpResponse, but the delay is resolved
// through Resolver.
class DelayedHttpResponseWithResolver final
    : public net::test_server::BasicHttpResponse {
 public:
  struct ResponseWithCallbacks final {
    net::test_server::SendBytesCallback send_callback;
    net::test_server::SendCompleteCallback done_callback;
    std::string response_string;
  };

  class Resolver final : public base::RefCountedThreadSafe<Resolver> {
   public:
    void Resolve() {
      base::AutoLock auto_lock(lock_);
      DCHECK(!resolved_);
      resolved_ = true;

      if (!task_runner_) {
        return;
      }

      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Resolver::ResolveInServerTaskRunner, this));
    }

    void Add(ResponseWithCallbacks response) {
      base::AutoLock auto_lock(lock_);

      if (resolved_) {
        response.send_callback.Run(response.response_string,
                                   std::move(response.done_callback));
        return;
      }

      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          base::ThreadTaskRunnerHandle::Get();
      if (task_runner_) {
        DCHECK_EQ(task_runner_, task_runner);
      } else {
        task_runner_ = std::move(task_runner);
      }

      responses_with_callbacks_.push_back(std::move(response));
    }

   private:
    void ResolveInServerTaskRunner() {
      auto responses_with_callbacks = std::move(responses_with_callbacks_);
      for (auto& response_with_callbacks : responses_with_callbacks) {
        response_with_callbacks.send_callback.Run(
            response_with_callbacks.response_string,
            std::move(response_with_callbacks.done_callback));
      }
    }

    friend class base::RefCountedThreadSafe<Resolver>;
    ~Resolver() = default;

    base::Lock lock_;

    std::vector<ResponseWithCallbacks> responses_with_callbacks_;
    bool resolved_ GUARDED_BY(lock_) = false;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_ GUARDED_BY(lock_);
  };

  explicit DelayedHttpResponseWithResolver(scoped_refptr<Resolver> resolver)
      : resolver_(std::move(resolver)) {}

  DelayedHttpResponseWithResolver(const DelayedHttpResponseWithResolver&) =
      delete;
  DelayedHttpResponseWithResolver& operator=(
      const DelayedHttpResponseWithResolver&) = delete;

  void SendResponse(const net::test_server::SendBytesCallback& send,
                    net::test_server::SendCompleteCallback done) override {
    resolver_->Add({send, std::move(done), ToResponseString()});
  }

 private:
  const scoped_refptr<Resolver> resolver_;
};

std::unique_ptr<net::test_server::HttpResponse> HandleBeacon(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

std::unique_ptr<net::test_server::HttpResponse> HandleHungBeacon(
    const base::RepeatingClosure& on_called,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  if (on_called) {
    on_called.Run();
  }
  return std::make_unique<net::test_server::HungResponse>();
}

std::unique_ptr<net::test_server::HttpResponse> HandleHungBeaconWithResolver(
    scoped_refptr<DelayedHttpResponseWithResolver::Resolver> resolver,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  return std::make_unique<DelayedHttpResponseWithResolver>(std::move(resolver));
}

}  // namespace

class RenderProcessHostTest : public ContentBrowserTest,
                              public RenderProcessHostObserver {
 public:
  RenderProcessHostTest()
      : process_exits_(0), host_destructions_(0), use_frame_priority_(false) {}

  void SetUp() override {
    if (use_frame_priority_) {
      feature_list_.InitAndEnableFeature(
          features::kUseFramePriorityInRenderProcessHost);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kUseFramePriorityInRenderProcessHost);
    }
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetVisibleClients(RenderProcessHost* process, int32_t visible_clients) {
    RenderProcessHostImpl* impl = static_cast<RenderProcessHostImpl*>(process);
    impl->visible_clients_ = visible_clients;
  }
 protected:
  void set_process_exit_callback(base::OnceClosure callback) {
    process_exit_callback_ = std::move(callback);
  }

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    ++process_exits_;
    if (process_exit_callback_)
      std::move(process_exit_callback_).Run();
  }
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    ++host_destructions_;
  }
  void WaitUntilProcessExits(int target) {
    while (process_exits_ < target)
      base::RunLoop().RunUntilIdle();
  }

  int process_exits_;
  int host_destructions_;
  base::OnceClosure process_exit_callback_;
  bool use_frame_priority_;
  base::test::ScopedFeatureList feature_list_;
};

// A mock ContentBrowserClient that only considers a spare renderer to be a
// suitable host.
class SpareRendererContentBrowserClient : public TestContentBrowserClient {
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
class NonSpareRendererContentBrowserClient : public TestContentBrowserClient {
 public:
  NonSpareRendererContentBrowserClient() = default;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(NonSpareRendererContentBrowserClient);
};

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       GuestsAreNotSuitableHosts) {
  // Set max renderers to 1 to force running out of processes.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetMainFrame()->GetProcess();
  // Make it believe it's a guest.
  static_cast<RenderProcessHostImpl*>(rph)->set_is_for_guests_only_for_testing(
      true);
  EXPECT_EQ(1, RenderProcessHost::GetCurrentRenderProcessCountForTesting());

  // Navigate to a different page.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL another_url = embedded_test_server()->GetURL("/simple_page.html");
  another_url = another_url.ReplaceComponents(replace_host);
  EXPECT_TRUE(NavigateToURL(CreateBrowser(), another_url));

  // Expect that we got another process (the guest renderer was not reused).
  EXPECT_EQ(2, RenderProcessHost::GetCurrentRenderProcessCountForTesting());
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareRenderProcessHostTaken) {
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
            window->web_contents()->GetMainFrame()->GetProcess());

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

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareRenderProcessHostNotTaken) {
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
            window->web_contents()->GetMainFrame()->GetProcess());

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

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareRenderProcessHostKilled) {
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());

  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  mojo::Remote<mojom::TestService> service;
  ASSERT_NE(nullptr, spare_renderer);
  spare_renderer->BindReceiver(service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  set_process_exit_callback(run_loop.QuitClosure());
  spare_renderer->AddObserver(this);  // For process_exit_callback.

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

// Test that the spare renderer works correctly when the limit on the maximum
// number of processes is small.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       SpareRendererSurpressedMaxProcesses) {
  ASSERT_TRUE(embedded_test_server()->Start());

  SpareRendererContentBrowserClient browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&browser_client);

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
            new_window->web_contents()->GetMainFrame()->GetProcess());

  // Revert to the default process limit and original ContentBrowserClient.
  RenderProcessHost::SetMaxRendererProcessCount(0);
  SetBrowserClientForTesting(old_client);
}

// Check that the spare renderer is dropped if an existing process is reused.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareRendererOnProcessReuse) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NonSpareRendererContentBrowserClient browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&browser_client);

  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_NE(nullptr, spare_renderer);

  // This should reuse the existing process.
  Shell* new_browser = CreateBrowser();
  EXPECT_EQ(shell()->web_contents()->GetMainFrame()->GetProcess(),
            new_browser->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(spare_renderer,
            new_browser->web_contents()->GetMainFrame()->GetProcess());
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

  SetBrowserClientForTesting(old_client);
}

// Verifies that the spare renderer maintained by SpareRenderProcessHostManager
// is correctly destroyed during browser shutdown.  This test is an analogue
// to the //chrome-layer FastShutdown.SpareRenderProcessHost test.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       SpareRenderProcessHostDuringShutdown) {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      shell()->web_contents()->GetBrowserContext());

  // The verification is that there are no DCHECKs anywhere during test tear
  // down.
}

// Verifies that the spare renderer maintained by SpareRenderProcessHostManager
// is correctly destroyed when closing the last content shell.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareRendererDuringClosing) {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(
      shell()->web_contents()->GetBrowserContext());
  shell()->web_contents()->Close();

  // The verification is that there are no DCHECKs or UaF anywhere during test
  // tear down.
}

// Class that simulates the fact that some //content embedders (e.g. by
// overriding ChromeContentBrowserClient::GetStoragePartitionConfigForSite) can
// cause BrowserContext::GetDefaultStoragePartition(browser_context) to differ
// from BrowserContext::GetStoragePartition(browser_context, site_instance) even
// if |site_instance| is not for guests.
class CustomStoragePartitionForSomeSites : public TestContentBrowserClient {
 public:
  explicit CustomStoragePartitionForSomeSites(const GURL& site_to_isolate)
      : site_to_isolate_(site_to_isolate) {}

  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override {
    // Override for |site_to_isolate_|.
    if (site == site_to_isolate_) {
      return StoragePartitionConfig::Create("blah_isolated_storage",
                                            "blah_isolated_storage",
                                            false /* in_memory */);
    }

    return StoragePartitionConfig::CreateDefault();
  }

  std::string GetStoragePartitionIdForSite(BrowserContext* browser_context,
                                           const GURL& site) override {
    if (site == site_to_isolate_)
      return "custom";
    return "";
  }

  bool IsValidStoragePartitionId(BrowserContext* browser_context,
                                 const std::string& partition_id) override {
    return partition_id == "" || partition_id == "custom";
  }

 private:
  GURL site_to_isolate_;

  DISALLOW_COPY_AND_ASSIGN(CustomStoragePartitionForSomeSites);
};

// This test verifies that SpareRenderProcessHostManager correctly accounts
// for StoragePartition differences when handing out the spare process.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       SpareProcessVsCustomStoragePartition) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Provide custom storage partition for test sites.
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  scoped_refptr<SiteInstance> test_site_instance =
      SiteInstance::Create(browser_context)->GetRelatedSiteInstance(test_url);
  GURL test_site = test_site_instance->GetSiteURL();
  CustomStoragePartitionForSomeSites modified_client(test_site);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&modified_client);
  StoragePartition* default_storage =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  StoragePartition* custom_storage =
      BrowserContext::GetStoragePartitionForSite(browser_context, test_site);
  EXPECT_NE(default_storage, custom_storage);

  // Open a test window - it should be associated with the default storage
  // partition.
  Shell* window = CreateBrowser();
  RenderProcessHost* old_process =
      window->web_contents()->GetMainFrame()->GetProcess();
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
      window->web_contents()->GetMainFrame()->GetProcess();
  // Requirement to use a custom storage partition should force a process swap.
  EXPECT_NE(new_process, old_process);
  // The new process should be associated with the custom storage partition.
  EXPECT_EQ(custom_storage, new_process->GetStoragePartition());
  // And consequently, the spare shouldn't have been used.
  EXPECT_NE(spare_renderer, new_process);

  // Restore the original ContentBrowserClient.
  SetBrowserClientForTesting(old_client);
}

class RenderProcessHostObserverCounter : public RenderProcessHostObserver {
 public:
  explicit RenderProcessHostObserverCounter(RenderProcessHost* host) {
    host->AddObserver(this);
    observing_ = true;
    observed_host_ = host;
  }

  ~RenderProcessHostObserverCounter() override {
    if (observing_)
      observed_host_->RemoveObserver(this);
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
  RenderProcessHost* observed_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostObserverCounter);
};

// Check that the spare renderer is properly destroyed via
// DisableKeepAliveRefCount.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareVsDisableKeepAliveRefCount) {
  RenderProcessHost::WarmupSpareRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context());
  base::RunLoop().RunUntilIdle();

  RenderProcessHost* spare_renderer =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  RenderProcessHostObserverCounter counter(spare_renderer);

  RenderProcessHostWatcher process_watcher(
      spare_renderer, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  spare_renderer->DisableKeepAliveRefCount();

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

// Check that the spare renderer is properly destroyed via
// DisableKeepAliveRefCount.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, SpareVsFastShutdown) {
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
  EXPECT_FALSE(process_watcher.did_exit_normally());

  // An important part of test verification is that UaF doesn't happen in the
  // next revolution of the message pump - without extra care in the
  // SpareRenderProcessHostManager RenderProcessHost::Cleanup could be called
  // twice leading to a crash caused by double-free flavour of UaF in
  // base::DeleteHelper<...>::DoDelete.
  base::RunLoop().RunUntilIdle();

  DCHECK_EQ(1, counter.exited_count());
  DCHECK_EQ(1, counter.destroyed_count());
}

class ShellCloser : public RenderProcessHostObserver {
 public:
  ShellCloser(Shell* shell, std::string* logging_string)
      : shell_(shell), logging_string_(logging_string) {}

 protected:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    logging_string_->append("ShellCloser::RenderProcessExited ");
    shell_->Close();
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    logging_string_->append("ShellCloser::RenderProcessHostDestroyed ");
  }

  Shell* shell_;
  std::string* logging_string_;
};

class ObserverLogger : public RenderProcessHostObserver {
 public:
  explicit ObserverLogger(std::string* logging_string)
      : logging_string_(logging_string), host_destroyed_(false) {}

  bool host_destroyed() { return host_destroyed_; }

 protected:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    logging_string_->append("ObserverLogger::RenderProcessExited ");
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    logging_string_->append("ObserverLogger::RenderProcessHostDestroyed ");
    host_destroyed_ = true;
  }

  std::string* logging_string_;
  bool host_destroyed_;
};

// Flaky on Android. http://crbug.com/759514.
#if defined(OS_ANDROID)
#define MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls \
  DISABLED_AllProcessExitedCallsBeforeAnyHostDestroyedCalls
#else
#define MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls \
  AllProcessExitedCallsBeforeAnyHostDestroyedCalls
#endif
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  std::string logging_string;
  ShellCloser shell_closer(shell(), &logging_string);
  ObserverLogger observer_logger(&logging_string);
  RenderProcessHost* rph =
      shell()->web_contents()->GetMainFrame()->GetProcess();

  // Ensure that the ShellCloser observer is first, so that it will have first
  // dibs on the ProcessExited callback.
  rph->AddObserver(&shell_closer);
  rph->AddObserver(&observer_logger);

  // This will crash the render process, and start all the callbacks.
  // We can't use NavigateToURL here since it accesses the shell() after
  // navigating, which the shell_closer deletes.
  ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GURL(kChromeUICrashURL), 1);

  // The key here is that all the RenderProcessExited callbacks precede all the
  // RenderProcessHostDestroyed callbacks.
  EXPECT_EQ("ShellCloser::RenderProcessExited "
            "ObserverLogger::RenderProcessExited "
            "ShellCloser::RenderProcessHostDestroyed "
            "ObserverLogger::RenderProcessHostDestroyed ", logging_string);

  // If the test fails, and somehow the RPH is still alive somehow, at least
  // deregister the observers so that the test fails and doesn't also crash.
  if (!observer_logger.host_destroyed()) {
    rph->RemoveObserver(&shell_closer);
    rph->RemoveObserver(&observer_logger);
  }
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, KillProcessOnBadMojoMessage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetMainFrame()->GetProcess();

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  set_process_exit_callback(run_loop.QuitClosure());

  // Should reply with a bad message and cause process death.
  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

// Observes a WebContents and a specific frame within it, and waits until they
// both indicate that they are audible.
class AudioStartObserver : public WebContentsObserver {
 public:
  AudioStartObserver(WebContents* web_contents,
                     RenderFrameHost* render_frame_host,
                     base::OnceClosure audible_closure)
      : WebContentsObserver(web_contents),
        render_frame_host_(
            static_cast<RenderFrameHostImpl*>(render_frame_host)),
        contents_audible_(web_contents->IsCurrentlyAudible()),
        frame_audible_(render_frame_host_->is_audible()),
        audible_closure_(std::move(audible_closure)) {
    MaybeFireClosure();
  }
  ~AudioStartObserver() override = default;

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    DCHECK_NE(audible, contents_audible_);
    contents_audible_ = audible;
    MaybeFireClosure();
  }
  void OnFrameAudioStateChanged(RenderFrameHost* render_frame_host,
                                bool audible) override {
    if (render_frame_host_ != render_frame_host)
      return;
    DCHECK_NE(frame_audible_, audible);
    frame_audible_ = audible;
    MaybeFireClosure();
  }

 private:
  void MaybeFireClosure() {
    if (contents_audible_ && frame_audible_)
      std::move(audible_closure_).Run();
  }

  RenderFrameHostImpl* render_frame_host_ = nullptr;
  bool contents_audible_ = false;
  bool frame_audible_ = false;
  base::OnceClosure audible_closure_;
};

// Tests that audio stream counts (used for process priority calculations) are
// properly set and cleared during media playback and renderer terminations.
//
// Note: This test can't run when the Mojo Renderer is used since it does not
// create audio streams through the normal audio pathways; at present this is
// only used by Chromecast.
//
// crbug.com/864476: flaky on Android for unclear reasons.
#if BUILDFLAG(ENABLE_MOJO_RENDERER) || defined(OS_ANDROID)
#define KillProcessZerosAudioStreams DISABLED_KillProcessZerosAudioStreams
#endif
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, KillProcessZerosAudioStreams) {
  // TODO(maxmorin): This test only uses an output stream. There should be a
  // similar test for input streams.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/webaudio_oscillator.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());

  {
    // Start audio and wait for it to become audible, in both the frame *and*
    // the page.
    base::RunLoop run_loop;
    AudioStartObserver observer(shell()->web_contents(),
                                shell()->web_contents()->GetMainFrame(),
                                run_loop.QuitClosure());

    std::string result;
    EXPECT_TRUE(
        ExecuteScriptAndExtractString(shell(), "StartOscillator();", &result))
        << "Failed to execute javascript.";
    run_loop.Run();

    // No point in running the rest of the test if this is wrong.
    ASSERT_EQ(1, rph->get_media_stream_count_for_testing());
  }

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    // Note: We post task the QuitClosure since RenderProcessExited() is called
    // before destroying BrowserMessageFilters; and the next portion of the test
    // must run after these notifications have been delivered.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    set_process_exit_callback(media::BindToCurrentLoop(run_loop.QuitClosure()));
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, media::BindToCurrentLoop(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Verify shutdown went as expected.
  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

// Test class instance to run specific setup steps for capture streams.
class CaptureStreamRenderProcessHostTest : public RenderProcessHostTest {
 public:
  void SetUp() override {
    // Pixel output is needed when digging pixel values out of video tags for
    // verification in VideoCaptureStream tests.
    EnablePixelOutput();
    RenderProcessHostTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // These flags are necessary to emulate camera input for getUserMedia()
    // tests.
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    RenderProcessHostTest::SetUpCommandLine(command_line);
  }
};

// Tests that video capture stream count increments when getUserMedia() is
// called.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       GetUserMediaIncrementsVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "getUserMediaAndExpectSuccess({video: true});", &result))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());
}

// Tests that video capture stream count resets when getUserMedia() is called
// and stopped.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       StopResetsVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "getUserMediaAndStop({video: true});", &result))
      << "Failed to execute javascript.";
  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
}

// Tests that video capture stream counts (used for process priority
// calculations) are properly set and cleared during media playback and renderer
// terminations.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       KillProcessZerosVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "getUserMediaAndExpectSuccess({video: true});", &result))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    set_process_exit_callback(media::BindToCurrentLoop(run_loop.QuitClosure()));
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, media::BindToCurrentLoop(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

// Tests that media stream count increments when getUserMedia() is
// called with audio only.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       GetUserMediaAudioOnlyIncrementsMediaStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "getUserMediaAndExpectSuccess({video: false, audio: true});",
      &result))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());
}

// Tests that media stream counts (used for process priority
// calculations) are properly set and cleared during media playback and renderer
// terminations for audio only streams.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       KillProcessZerosAudioCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "getUserMediaAndExpectSuccess({video: false, audio: true});",
      &result))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    set_process_exit_callback(media::BindToCurrentLoop(run_loop.QuitClosure()));
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, media::BindToCurrentLoop(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, KeepAliveRendererProcess) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleBeacon));
  ASSERT_TRUE(embedded_test_server()->Start());

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed that navigations to this site
    // will be in a different process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/send-beacon.html")));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);
  rfh->SetKeepAliveTimeoutForTesting(base::TimeDelta::FromSeconds(30));

  // Navigate to a site that will be in a different process.
  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));

  WaitUntilProcessExits(1);

  EXPECT_LT(base::TimeTicks::Now() - start, base::TimeDelta::FromSeconds(30));
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       KeepAliveRendererProcessWithServiceWorker) {
  base::RunLoop run_loop;
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, run_loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  // 1 for the service worker.
  EXPECT_EQ(rph->keep_alive_ref_count(), 1u);

  // We use /workers/send-beacon.html, not send-beacon.html, due to the
  // service worker scope rule.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/workers/send-beacon.html")));

  run_loop.Run();
  // We are still using the same process.
  ASSERT_EQ(shell()->web_contents()->GetMainFrame()->GetProcess(), rph);
  // 1 for the service worker, 1 for the keepalive fetch.
  EXPECT_EQ(rph->keep_alive_ref_count(), 2u);
}

// Test is flaky on Android builders: https://crbug.com/875179
#if defined(OS_ANDROID)
#define MAYBE_KeepAliveRendererProcess_Hung \
  DISABLED_KeepAliveRendererProcess_Hung
#else
#define MAYBE_KeepAliveRendererProcess_Hung KeepAliveRendererProcess_Hung
#endif
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       MAYBE_KeepAliveRendererProcess_Hung) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/send-beacon.html")));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);
  rfh->SetKeepAliveTimeoutForTesting(base::TimeDelta::FromSeconds(1));

  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>hello</p>")));

  WaitUntilProcessExits(1);

  EXPECT_GE(base::TimeTicks::Now() - start, base::TimeDelta::FromSeconds(1));
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

// Test is flaky on Android builders: https://crbug.com/875179
#if defined(OS_ANDROID)
#define MAYBE_FetchKeepAliveRendererProcess_Hung \
  DISABLED_FetchKeepAliveRendererProcess_Hung
#else
#define MAYBE_FetchKeepAliveRendererProcess_Hung \
  FetchKeepAliveRendererProcess_Hung
#endif
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       MAYBE_FetchKeepAliveRendererProcess_Hung) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/fetch-keepalive.html")));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  host_destructions_ = 0;
  process_exits_ = 0;
  rph->AddObserver(this);
  rfh->SetKeepAliveTimeoutForTesting(base::TimeDelta::FromSeconds(1));

  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>hello</p>")));

  WaitUntilProcessExits(1);

  EXPECT_GE(base::TimeTicks::Now() - start, base::TimeDelta::FromSeconds(1));
  if (!host_destructions_)
    rph->RemoveObserver(this);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ManyKeepaliveRequests) {
  auto resolver =
      base::MakeRefCounted<DelayedHttpResponseWithResolver::Resolver>();
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeaconWithResolver, resolver));
  const base::string16 title = base::ASCIIToUTF16("Resolved");
  const base::string16 waiting = base::ASCIIToUTF16("Waiting");

  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/fetch-keepalive.html?requests=256")));

  {
    // Wait for the page to make all the keepalive requests.
    TitleWatcher watcher(shell()->web_contents(), waiting);
    EXPECT_EQ(waiting, watcher.WaitAndGetTitle());
  }

  resolver->Resolve();

  {
    TitleWatcher watcher(shell()->web_contents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, TooManyKeepaliveRequests) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  const base::string16 title = base::ASCIIToUTF16("Rejected");

  TitleWatcher watcher(shell()->web_contents(), title);

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/fetch-keepalive.html?requests=257")));

  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, LowPriorityFramesDisabled) {
  // RenderProcessHostImpl::UpdateProcessPriority has an early check of
  // run_renderer_in_process and exits for RenderProcessHosts without a child
  // process launcher.  In order to skip initializing that here and the layer of
  // indirection, we explicitly run in-process, which we must also disable once
  // the test has finished to prevent crashing on exit.
  RenderProcessHost::SetRunRendererInProcess(true);
  RenderProcessHostImpl* process = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateRenderProcessHost(
          ShellContentBrowserClient::Get()->browser_context(), nullptr));
  // It starts off as normal priority.
  EXPECT_FALSE(process->IsProcessBackgrounded());
  // With the feature off it stays low priority when adding low priority frames.
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  RenderProcessHost::SetRunRendererInProcess(false);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, PriorityOverride) {
  // RenderProcessHostImpl::UpdateProcessPriority has an early check of
  // run_renderer_in_process and exits for RenderProcessHosts without a child
  // process launcher.  In order to skip initializing that here and the layer of
  // indirection, we explicitly run in-process, which we must also disable once
  // the test has finished to prevent crashing on exit.
  RenderProcessHost::SetRunRendererInProcess(true);
  RenderProcessHostImpl* process = static_cast<RenderProcessHostImpl*>(
      RenderProcessHostImpl::CreateRenderProcessHost(
          ShellContentBrowserClient::Get()->browser_context(), nullptr));

  // It starts off as normal priority with no override.
  EXPECT_FALSE(process->HasPriorityOverride());
  EXPECT_FALSE(process->IsProcessBackgrounded());

  process->SetPriorityOverride(false /* foreground */);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_TRUE(process->IsProcessBackgrounded());

  process->SetPriorityOverride(true /* foreground */);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_FALSE(process->IsProcessBackgrounded());

  process->SetPriorityOverride(false /* foreground */);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_TRUE(process->IsProcessBackgrounded());

  // Add a pending view, and expect the process to *stay* backgrounded.
  process->AddPendingView();
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_TRUE(process->IsProcessBackgrounded());

  // Clear the override. The pending view should cause the process to go back to
  // being foregrounded.
  process->ClearPriorityOverride();
  EXPECT_FALSE(process->HasPriorityOverride());
  EXPECT_FALSE(process->IsProcessBackgrounded());

  // Clear the pending view so the test doesn't explode.
  process->RemovePendingView();
  EXPECT_FALSE(process->HasPriorityOverride());
  EXPECT_TRUE(process->IsProcessBackgrounded());

  RenderProcessHost::SetRunRendererInProcess(false);
}

// This test verifies properties of RenderProcessHostImpl *before* Init method
// is called.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ConstructedButNotInitializedYet) {
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);

  // Just verifying that the arguments of CreateRenderProcessHost got processed
  // correctly.
  EXPECT_EQ(ShellContentBrowserClient::Get()->browser_context(),
            process->GetBrowserContext());
  EXPECT_FALSE(process->IsForGuestsOnly());

  // There should be no OS process before Init() method is called.
  EXPECT_FALSE(process->IsInitializedAndNotDead());
  EXPECT_FALSE(process->IsReady());
  EXPECT_FALSE(process->GetProcess().IsValid());
  EXPECT_EQ(base::kNullProcessHandle, process->GetProcess().Handle());

  // TODO(lukasza): https://crbug.com/813045: RenderProcessHost shouldn't have
  // an associated IPC channel (and shouldn't accumulate IPC messages) unless
  // the Init() method was called and the RPH either has connection to an actual
  // OS process or is currently attempting to spawn the OS process.  After this
  // bug is fixed the 1st test assertion below should be reversed (unsure about
  // the 2nd one).
  EXPECT_TRUE(process->GetChannel());
  EXPECT_TRUE(process->GetRendererInterface());

  // Cleanup the resources acquired by the test.
  process->Cleanup();
}

// This test verifies that a fast shutdown is possible for a starting process.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, FastShutdownForStartingProcess) {
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  process->Init();
  EXPECT_TRUE(process->FastShutdownIfPossible());
  process->Cleanup();
}

class RenderProcessHostFramePriorityTest : public RenderProcessHostTest {
 public:
  // Initialize the clock and set the fixture to use frame priority feature.
  RenderProcessHostFramePriorityTest() {
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    use_frame_priority_ = true;
  }

  // Check the histograms at TearDown.
  void TearDown() override {
    RenderProcessHostTest::TearDown();
    // TODO(crbug/1045958): Check the histograms directly in the tests.
    CheckHistograms();
  }

  // Set the expected frame priorities seen and set TearDown check variable.
  void SetFramePrioritiesExpected(bool low, bool normal) {
    low_priority_frames_expected_ = low;
    normal_priority_frames_expected_ = normal;
    check_frame_priorities_ = true;
  }

  // Set the times expected at low priority and overall.
  void SetPriorityTimesExpected(base::TimeDelta low_time,
                                base::TimeDelta total_time) {
    low_priority_time_expected_ = low_time;
    total_time_expected_ = total_time;
  }

  // Advance the internal clock for the RenderProcessHost.
  void AdvanceClock(base::TimeDelta time) { clock_->Advance(time); }

  // Create the process with the test clock, initialize, and return it.
  RenderProcessHostImpl* CreateAndInitializeProcess() {
    // RenderProcessHostImpl::UpdateProcessPriority has an early check of
    // run_renderer_in_process and exits for RenderProcessHosts without a child
    // process launcher.  In order to skip initializing that here and the layer
    // of indirection, we explicitly run in-process, which we must also disable
    // once the test has finished to prevent crashing on exit.
    RenderProcessHostImpl::SetRunRendererInProcess(true);
    // Initialize the clock's value to the current time.
    clock_->SetNowTicks(base::TimeTicks::Now());
    // Create the process itself.
    process_ = static_cast<RenderProcessHostImpl*>(
        RenderProcessHostImpl::CreateRenderProcessHost(
            ShellContentBrowserClient::Get()->browser_context(), nullptr));
    // For these tests, assume something is always visible.
    SetVisibleClients(process_, 1);
    // Any advancement before Init is ignored.
    AdvanceClock(base::TimeDelta::FromSeconds(10));
    // Don't start an actual process, just initialize times.
    process_->SetClockForTesting(clock_.get());
    // When no frames are attached, it's not low priority.
    EXPECT_FALSE(process_->IsProcessBackgrounded());
    // Return it so it can be used by tests.
    return process_;
  }

  // Shut down the created process.
  void ShutDownAndCleanUpProcess() {
    RenderProcessHostImpl::SetRunRendererInProcess(false);
  }

 private:
  // Check the histograms related to frame priority and backgrounding.
  void CheckHistograms() {
    if (total_time_expected_ != base::TimeDelta()) {
      histogram_tester_.ExpectUniqueSample(
          "BrowserRenderProcessHost.TotalTime",
          total_time_expected_.InMilliseconds(), 1);
      histogram_tester_.ExpectUniqueSample(
          "BrowserRenderProcessHost.BackgroundTime",
          low_priority_time_expected_.InMilliseconds(), 1);
    }

    if (!check_frame_priorities_)
      return;

    if (low_priority_frames_expected_ && normal_priority_frames_expected_) {
      histogram_tester_.ExpectBucketCount(
          "BrowserRenderProcessHost.FramePrioritiesSeen",
          RenderProcessHostImpl::FramePrioritiesSeen::kMixedPrioritiesSeen, 1);
    } else if (low_priority_frames_expected_) {
      histogram_tester_.ExpectBucketCount(
          "BrowserRenderProcessHost.FramePrioritiesSeen",
          RenderProcessHostImpl::FramePrioritiesSeen::kOnlyLowPrioritiesSeen,
          1);
    } else if (normal_priority_frames_expected_) {
      histogram_tester_.ExpectBucketCount(
          "BrowserRenderProcessHost.FramePrioritiesSeen",
          RenderProcessHostImpl::FramePrioritiesSeen::kOnlyNormalPrioritiesSeen,
          1);
    } else {
      // Two are expected here because tests using this create their own
      // process, meaning the default process is never used/has no frames
      // attached.
      histogram_tester_.ExpectBucketCount(
          "BrowserRenderProcessHost.FramePrioritiesSeen",
          RenderProcessHostImpl::FramePrioritiesSeen::kNoFramesSeen, 2);
    }
  }

  // The process that is created to add frames with various priorities.
  RenderProcessHostImpl* process_;
  // The histogram tester, to check backgrounding and priority histograms.
  base::HistogramTester histogram_tester_;
  // Whether frames with low priority have been attached to the process.
  bool low_priority_frames_expected_ = false;
  // Whether frames with normal priority have been attached to the process.
  bool normal_priority_frames_expected_ = false;
  // Whether we should check the frame priority histograms.
  bool check_frame_priorities_ = false;
  // The expected time a process hosts only low priority frames.
  base::TimeDelta low_priority_time_expected_;
  // The expected time a process hosts frames overall.
  base::TimeDelta total_time_expected_;
  // The clock that is used by the RenderProcessHost.
  std::unique_ptr<base::SimpleTestTickClock> clock_;
};

IN_PROC_BROWSER_TEST_F(RenderProcessHostFramePriorityTest,
                       NoFramesSeenPriorityTest) {
  RenderProcessHostImpl* process = CreateAndInitializeProcess();
  DCHECK(process);
  ShutDownAndCleanUpProcess();
  SetFramePrioritiesExpected(false, false);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostFramePriorityTest,
                       LowPriorityFramesEnabled) {
  // When the process is first created with no frames, it's not low priority.
  RenderProcessHostImpl* process = CreateAndInitializeProcess();
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // When all frames added are low priority, it's low priority.
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_TRUE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(2));
  // When all the low priority frames are removed, it's not low priority.
  process->UpdateFrameWithPriority(RenderProcessHostImpl::FramePriority::kLow,
                                   base::nullopt);
  process->UpdateFrameWithPriority(RenderProcessHostImpl::FramePriority::kLow,
                                   base::nullopt);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // When a low priority frame is added back in, it's low priority.
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_TRUE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // As soon as a non-low priority frame is added, it's not low priority.
  process->UpdateFrameWithPriority(
      base::nullopt, RenderProcessHostImpl::FramePriority::kNormal);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // It remains not low priority even if we add more low priority frames.
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // As soon as the non-low priority frame is removed, it becomes low priority.
  process->UpdateFrameWithPriority(
      RenderProcessHostImpl::FramePriority::kNormal, base::nullopt);
  EXPECT_TRUE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // Add a non-low priority frame, but then transition it to low, the process
  // should go from unbackgrounded to backgrounded.
  process->UpdateFrameWithPriority(
      base::nullopt, RenderProcessHostImpl::FramePriority::kNormal);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  process->UpdateFrameWithPriority(
      RenderProcessHostImpl::FramePriority::kNormal,
      RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_TRUE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(2));
  // Transition the frame back to normal priority, it becomes normal priority.
  process->UpdateFrameWithPriority(
      RenderProcessHostImpl::FramePriority::kLow,
      RenderProcessHostImpl::FramePriority::kNormal);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(1));

  // This process has had low and normal priority frames attached to it.
  ShutDownAndCleanUpProcess();
  SetFramePrioritiesExpected(true, true);
  SetPriorityTimesExpected(base::TimeDelta::FromSeconds(6),
                           base::TimeDelta::FromSeconds(12));
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostFramePriorityTest,
                       LowPriorityFramesEnabledOnlyLowPriority) {
  RenderProcessHostImpl* process = CreateAndInitializeProcess();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // When all frames added are low priority, it's low priority.
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  process->UpdateFrameWithPriority(base::nullopt,
                                   RenderProcessHostImpl::FramePriority::kLow);
  EXPECT_TRUE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(2));

  // This process has had only low priority frames attached to it.
  ShutDownAndCleanUpProcess();
  SetFramePrioritiesExpected(true, false);
  SetPriorityTimesExpected(base::TimeDelta::FromSeconds(2),
                           base::TimeDelta::FromSeconds(3));
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostFramePriorityTest,
                       LowPriorityFramesEnabledOnlyNormalPriority) {
  RenderProcessHostImpl* process = CreateAndInitializeProcess();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // When all frames added are normal priority, it's not low priority.
  process->UpdateFrameWithPriority(
      base::nullopt, RenderProcessHostImpl::FramePriority::kNormal);
  process->UpdateFrameWithPriority(
      base::nullopt, RenderProcessHostImpl::FramePriority::kNormal);
  EXPECT_FALSE(process->IsProcessBackgrounded());
  AdvanceClock(base::TimeDelta::FromSeconds(2));

  // This process has had only normal priority frames attached to it.
  ShutDownAndCleanUpProcess();
  SetFramePrioritiesExpected(false, true);
  SetPriorityTimesExpected(base::TimeDelta(), base::TimeDelta::FromSeconds(3));
}

}  // namespace content
