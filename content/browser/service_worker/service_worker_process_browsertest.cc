// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

// This file has tests involving render process selection for service workers.

namespace content {

// An observer that waits for the service worker to be running.
class WorkerRunningStatusObserver : public ServiceWorkerContextObserver {
 public:
  explicit WorkerRunningStatusObserver(ServiceWorkerContext* context) {
    scoped_context_observation_.Observe(context);
  }

  WorkerRunningStatusObserver(const WorkerRunningStatusObserver&) = delete;
  WorkerRunningStatusObserver& operator=(const WorkerRunningStatusObserver&) =
      delete;

  ~WorkerRunningStatusObserver() override = default;

  int64_t version_id() { return version_id_; }

  void WaitUntilRunning() {
    if (version_id_ == blink::mojom::kInvalidServiceWorkerVersionId) {
      run_loop_.Run();
    }
  }

  void OnVersionStartedRunning(
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) override {
    version_id_ = version_id;

    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ServiceWorkerContext, ServiceWorkerContextObserver>
      scoped_context_observation_{this};
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
};

class ServiceWorkerProcessBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ServiceWorkerProcessBrowserTest() = default;
  ~ServiceWorkerProcessBrowserTest() override = default;

  ServiceWorkerProcessBrowserTest(const ServiceWorkerProcessBrowserTest&) =
      delete;
  ServiceWorkerProcessBrowserTest& operator=(
      const ServiceWorkerProcessBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (SitePerProcess()) {
      command_line->AppendSwitch(switches::kSitePerProcess);
    } else {
      command_line->RemoveSwitch(switches::kSitePerProcess);
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
    }
  }

 protected:
  bool SitePerProcess() const { return GetParam(); }

  // Registers a service worker and then tears down the process it used, for a
  // clean slate going forward.
  void RegisterServiceWorker() {
    // Load a page that registers a service worker.
    Shell* start_shell = CreateBrowser();
    ASSERT_TRUE(NavigateToURL(
        start_shell, embedded_test_server()->GetURL(
                         "/service_worker/create_service_worker.html")));
    ASSERT_EQ("DONE",
              EvalJs(start_shell, "register('fetch_event_pass_through.js');"));

    auto* host = RenderProcessHost::FromID(GetServiceWorkerProcessId());
    ASSERT_TRUE(host);
    RenderProcessHostWatcher exit_watcher(
        host, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

    // Tear down the page.
    start_shell->Close();

    // Stop the service worker. The process should exit.
    base::RunLoop loop;
    wrapper()->StopAllServiceWorkers(loop.QuitClosure());
    loop.Run();
    exit_watcher.Wait();
  }

  // Returns the number of running service workers.
  size_t GetRunningServiceWorkerCount() {
    return wrapper()->GetRunningServiceWorkerInfos().size();
  }

  // Returns the process id of the running service worker. There must be exactly
  // one service worker running.
  int GetServiceWorkerProcessId() {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        wrapper()->GetRunningServiceWorkerInfos();
    DCHECK_EQ(infos.size(), 1u);
    const ServiceWorkerRunningInfo& info = infos.begin()->second;
    return info.render_process_id;
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Tests that a service worker started due to a navigation shares the same
// process as the navigation.
// Flaky on Android; see https://crbug.com/1320972.
// Flaky on TSan Linux; see https://crbug.com/349316554.
#if BUILDFLAG(IS_ANDROID) ||                            \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(THREAD_SANITIZER))
#define MAYBE_ServiceWorkerAndPageShareProcess \
  DISABLED_ServiceWorkerAndPageShareProcess
#else
#define MAYBE_ServiceWorkerAndPageShareProcess ServiceWorkerAndPageShareProcess
#endif
IN_PROC_BROWSER_TEST_P(ServiceWorkerProcessBrowserTest,
                       MAYBE_ServiceWorkerAndPageShareProcess) {
  // Register the service worker.
  RegisterServiceWorker();

  // Navigate to a page in the service worker's scope.
  WorkerRunningStatusObserver observer(public_context());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  observer.WaitUntilRunning();

  // The page and service worker should be in the same process.
  int page_process_id = current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(page_process_id, ChildProcessHost::kInvalidUniqueID);
  ASSERT_EQ(GetRunningServiceWorkerCount(), 1u);
  int worker_process_id = GetServiceWorkerProcessId();
  EXPECT_EQ(page_process_id, worker_process_id);
}

// Tests whether a service worker and navigation share the same process in the
// special case where the service worker starts before the navigation starts,
// and the navigation transitions out of a page with no site URL. This special
// case happens in real life when doing a search from the omnibox while on the
// Android native NTP page: the service worker starts first due to the
// navigation hint from the omnibox, and the native page has no site URL. See
// https://crbug.com/1012143.
IN_PROC_BROWSER_TEST_P(ServiceWorkerProcessBrowserTest,
                       NavigateFromUnassignedSiteInstance) {
  // Set up an empty page scheme whose URLs will have no site assigned. This
  // requires setting it as an empty document scheme.
  url::ScopedSchemeRegistryForTests scheme_registry;
  url::AddEmptyDocumentScheme("siteless");

  GURL empty_site_url = GURL("siteless://test");
  EXPECT_FALSE(SiteInstance::ShouldAssignSiteForURL(empty_site_url));

  // Register the service worker.
  RegisterServiceWorker();

  // Navigate to the empty site instance page.
  ASSERT_TRUE(NavigateToURL(shell(), empty_site_url));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), empty_site_url);
  scoped_refptr<SiteInstanceImpl> site_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_EQ(GURL(), site_instance->GetSiteURL());
  int page_process_id = current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(page_process_id, ChildProcessHost::kInvalidUniqueID);

  // Start the service worker.
  base::RunLoop loop;
  GURL scope = embedded_test_server()->GetURL("/service_worker/");
  int worker_process_id;
  wrapper()->ServiceWorkerContextWrapper::StartWorkerForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindLambdaForTesting(
          [&](int64_t version_id, int process_id, int thread_id) {
            worker_process_id = process_id;
            loop.Quit();
          }),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status_code) {
            ASSERT_FALSE(true) << "start worker failed";
            loop.Quit();
          }));
  loop.Run();

  // The page and service worker are in different processes. (This is not
  // necessarily the desired behavior, but the current one of the
  // implementation.)
  EXPECT_NE(page_process_id, worker_process_id);

  // Navigate to a page in the service worker's scope. It should be in the
  // same process as the original page.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  EXPECT_EQ(page_process_id, current_frame_host()->GetProcess()->GetID());
}

// Toggle Site Isolation.
INSTANTIATE_TEST_SUITE_P(All, ServiceWorkerProcessBrowserTest, testing::Bool());

}  // namespace content
