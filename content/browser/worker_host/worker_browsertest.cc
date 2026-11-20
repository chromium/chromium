// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_size.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kSameSiteLaxCookie[] =
    "same-site-lax-cookie=same-site-lax-cookie-value";
const char kSameSiteStrictCookie[] =
    "same-site-strict-cookie=same-site-strict-cookie-value";
const char kSameSiteNoneCookie[] =
    "same-site-none-cookie=same-site-none-cookie-value";
const char kFirstPartyPartitionedCookie[] =
    "first-party-partitioned=partitioned-cookie-value";
const char kCrossSitePartitionedCookie[] =
    "cross-site-partitioned=partitioned-cookie-value";

// Used by both the embedded test server when a header specified by
// "/echoheader" is missing, and by the test fixture when there's no cookie
// present.
const char kNoCookie[] = "None";

bool SupportsSharedWorker() {
  return base::FeatureList::IsEnabled(blink::features::kSharedWorker);
}

// Writes a worker script response with the given `url_list` populated in the
// service worker URL list of the response head.
void WriteWorkerScriptResponseWithServiceWorkerUrlList(
    network::mojom::URLLoaderClient* client,
    const std::vector<GURL>& url_list) {
  static constexpr char kBody[] = "postMessage('done');";
  auto response = network::mojom::URLResponseHead::New();
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n");
  response->mime_type = "text/javascript";
  response->was_fetched_via_service_worker = true;
  response->url_list_via_service_worker = url_list;
  response->parsed_headers = network::mojom::ParsedHeaders::New();

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  EXPECT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  if (producer_handle.is_valid()) {
    EXPECT_EQ(
        producer_handle->WriteAllData(base::byte_span_from_cstring(kBody)),
        MOJO_RESULT_OK);
  }
  producer_handle.reset();

  client->OnReceiveResponse(std::move(response), std::move(consumer_handle),
                            std::nullopt);
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  status.decoded_body_length = base::ByteSize(sizeof(kBody) - 1);
  client->OnComplete(status);
}

// Records final response URLs reported for newly created workers.
class WorkerFinalResponseURLObserver : public DedicatedWorkerService::Observer,
                                       public SharedWorkerService::Observer {
 public:
  explicit WorkerFinalResponseURLObserver(StoragePartition* storage_partition) {
    dedicated_worker_observation_.Observe(
        storage_partition->GetDedicatedWorkerService());
    shared_worker_observation_.Observe(
        storage_partition->GetSharedWorkerService());
  }

  const std::vector<GURL>& final_response_urls() const {
    return final_response_urls_;
  }

  // DedicatedWorkerService::Observer:
  void OnWorkerCreated(const blink::DedicatedWorkerToken&,
                       ChildProcessId,
                       const url::Origin&,
                       DedicatedWorkerCreator) override {}
  void OnBeforeWorkerDestroyed(const blink::DedicatedWorkerToken&,
                               DedicatedWorkerCreator) override {}
  void OnFinalResponseURLDetermined(const blink::DedicatedWorkerToken&,
                                    const GURL& url) override {
    final_response_urls_.push_back(url);
  }

  // SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken&,
                       ChildProcessId,
                       const url::Origin&,
                       const base::UnguessableToken&) override {}
  void OnBeforeWorkerDestroyed(const blink::SharedWorkerToken&) override {}
  void OnFinalResponseURLDetermined(const blink::SharedWorkerToken&,
                                    const GURL& url) override {
    final_response_urls_.push_back(url);
  }
  void OnClientAdded(const blink::SharedWorkerToken&,
                     GlobalRenderFrameHostId) override {}
  void OnClientRemoved(const blink::SharedWorkerToken&,
                       GlobalRenderFrameHostId) override {}

 private:
  std::vector<GURL> final_response_urls_;
  base::ScopedObservation<DedicatedWorkerService,
                          DedicatedWorkerService::Observer>
      dedicated_worker_observation_{this};
  base::ScopedObservation<SharedWorkerService, SharedWorkerService::Observer>
      shared_worker_observation_{this};
};

}  // namespace

class WorkerTest : public ContentBrowserTest {
 public:
  WorkerTest() : select_certificate_count_(0) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ShellContentBrowserClient::Get()->set_select_client_certificate_callback(
        base::BindOnce(&WorkerTest::OnSelectClientCertificate,
                       base::Unretained(this)));
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.RegisterRequestHandler(base::BindRepeating(
        &WorkerTest::MonitorRequestCookies, base::Unretained(this)));
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  int select_certificate_count() const { return select_certificate_count_; }

  GURL GetTestFileURL(const std::string& test_case) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("workers").AppendASCII(test_case);
    return net::FilePathToFileURL(path);
  }

  GURL GetTestURL(const std::string& test_case, const std::string& query) {
    std::string url_string = "/workers/" + test_case + "?" + query;
    return ssl_server_.GetURL("a.test", url_string);
  }

  void RunTest(Shell* window, const GURL& url, bool expect_failure = false) {
    const std::u16string ok_title = u"OK";
    const std::u16string fail_title = u"FAIL";
    TitleWatcher title_watcher(window->web_contents(), ok_title);
    title_watcher.AlsoWaitForTitle(fail_title);
    EXPECT_TRUE(NavigateToURL(window, url));
    std::u16string final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expect_failure ? fail_title : ok_title, final_title);
  }

  void RunTest(const GURL& url, bool expect_failure = false) {
    RunTest(shell(), url, expect_failure);
  }

  static void QuitUIMessageLoop(
      base::OnceClosure callback,
      bool is_primary_main_frame_navigation /* unused */,
      bool is_navigation /* unused */) {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }

  void NavigateAndWaitForAuth(const GURL& url) {
    ShellContentBrowserClient* browser_client =
        ShellContentBrowserClient::Get();
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    browser_client->set_login_request_callback(
        base::BindOnce(&QuitUIMessageLoop, runner->QuitClosure()));
    shell()->LoadURL(url);
    runner->Run();
  }

  void SetSameSiteLaxCookie(std::string_view host) {
    ASSERT_TRUE(SetCookie(
        shell()->web_contents()->GetBrowserContext(),
        ssl_server_.GetURL(host, "/"),
        base::StrCat({kSameSiteLaxCookie, "; SameSite=Lax; Secure"})));
  }

  void SetPartitionedCookie(
      std::string_view host,
      std::string_view cookie_name_value,
      net::CookiePartitionKey::AncestorChainBit ancestor_chain_bit) {
    GURL cookie_url = ssl_server_.GetURL(host, "/");
    net::CookiePartitionKey partition_key = net::CookiePartitionKey::FromWire(
        net::SchemefulSite(cookie_url), ancestor_chain_bit);
    ASSERT_TRUE(
        SetCookie(shell()->web_contents()->GetBrowserContext(), cookie_url,
                  base::StrCat({cookie_name_value,
                                "; SameSite=None; Secure; Partitioned"}),
                  net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                  partition_key));
  }

  // Returns the cookie received with the request for the specified path. If the
  // path was requested but no cookie was received, return kNoCookie. Waits for
  // the path to be requested if it hasn't been requested already.
  std::string GetReceivedCookie(const std::string& path) {
    {
      base::AutoLock auto_lock(path_cookie_map_lock_);
      DCHECK(path_to_wait_for_.empty());
      DCHECK(!path_wait_loop_);
      if (path_cookie_map_.find(path) != path_cookie_map_.end())
        return path_cookie_map_[path];
      path_to_wait_for_ = path;
      path_wait_loop_ = std::make_unique<base::RunLoop>();
    }

    path_wait_loop_->Run();

    base::AutoLock auto_lock(path_cookie_map_lock_);
    path_to_wait_for_.clear();
    path_wait_loop_.reset();
    return path_cookie_map_[path];
  }

  void ClearReceivedCookies() {
    base::AutoLock auto_lock(path_cookie_map_lock_);
    path_cookie_map_.clear();
  }

  SharedWorkerHost* GetSharedWorkerHost(const GURL& url) {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    DCHECK(partition);
    auto* service = static_cast<SharedWorkerServiceImpl*>(
        partition->GetSharedWorkerService());
    return service->FindMatchingSharedWorkerHost(
        url, "", blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        blink::mojom::SharedWorkerSameSiteCookies::kAll);
  }

  net::test_server::EmbeddedTestServer* ssl_server() { return &ssl_server_; }

 private:
  base::OnceClosure OnSelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) {
    select_certificate_count_++;
    return base::OnceClosure();
  }

  std::unique_ptr<net::test_server::HttpResponse> MonitorRequestCookies(
      const net::test_server::HttpRequest& request) {
    // Ignore every host but "a.test", to help catch cases of sending requests
    // to the wrong host.
    auto host_header = request.headers.find("Host");
    if (host_header == request.headers.end() ||
        !base::StartsWith(host_header->second,
                          "a.test:", base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

    base::AutoLock auto_lock(path_cookie_map_lock_);

    if (path_cookie_map_.find(request.relative_url) != path_cookie_map_.end()) {
      path_cookie_map_[request.relative_url] = "path requested multiple times";
      return nullptr;
    }

    auto cookie_header = request.headers.find("Cookie");
    if (cookie_header == request.headers.end()) {
      path_cookie_map_[request.relative_url] = kNoCookie;
    } else {
      path_cookie_map_[request.relative_url] = cookie_header->second;
    }
    if (path_to_wait_for_ == request.relative_url) {
      path_wait_loop_->Quit();
    }
    return nullptr;
  }

  // Mapping of paths requested from "a.test" to cookies they were requested
  // with. Paths may only be requested once without clearing the map.
  std::map<std::string, std::string> path_cookie_map_
      GUARDED_BY(path_cookie_map_lock_);
  // If non-empty, path to wait for the test server to see a request for on the
  // "a.test" server.
  std::string path_to_wait_for_ GUARDED_BY(path_cookie_map_lock_);
  // If non-null, quit when a request for |path_to_wait_for_| is observed. May
  // only be created or dereferenced off of the UI thread while holding
  // |path_cookie_map_lock_|, its run method must be called while not holding
  // the lock.
  std::unique_ptr<base::RunLoop> path_wait_loop_;
  // Lock that must be held while modifying |path_cookie_map_|, as it's used on
  // both the test server's thread and the UI thread.
  base::Lock path_cookie_map_lock_;

  // The cookie tests require an SSL server, since SameSite None cookies can
  // only be set on secure origins. Most other tests use this, too, to keep
  // things simpler, though they could use an HTTP server instead.
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  int select_certificate_count_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorker) {
  RunTest(GetTestURL("single_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"), true /* expect_failure */);
}

class WorkerTestWithAllowFileAccessFromFiles : public WorkerTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WorkerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
  }
};

IN_PROC_BROWSER_TEST_F(WorkerTestWithAllowFileAccessFromFiles,
                       SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, HttpPageCantCreateFileWorker) {
  GURL url = GetTestURL(
      "single_worker.html",
      "workerUrl=" + base::EscapeQueryParamValue(
                         GetTestFileURL("worker_common.js").spec(), true));
  RunTest(url, /*expect_failure=*/true);
}

// Tests that a dedicated worker main script response whose service worker
// supplied URL list resolves to a URL that the worker process cannot commit
// is rejected.
IN_PROC_BROWSER_TEST_F(WorkerTest,
                       DedicatedWorkerRejectsNonCommittableFinalResponseUrl) {
  const GURL main_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url = ssl_server()->GetURL("a.test", "/workers/worker.js");
  const GURL non_committable_url("file:///non_committable_path");

  WorkerFinalResponseURLObserver observer(shell()
                                              ->web_contents()
                                              ->GetBrowserContext()
                                              ->GetDefaultStoragePartition());

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != worker_url) {
          return false;
        }
        WriteWorkerScriptResponseWithServiceWorkerUrlList(
            params->client.get(), {non_committable_url});
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanCommitURL(
      shell()
          ->web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess()
          ->GetDeprecatedID(),
      non_committable_url));

  EXPECT_EQ("error", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker = new Worker('/workers/worker.js');
      worker.onerror = () => resolve('error');
      worker.onmessage = e => resolve(e.data);
    })
  )"));

  // The worker process is not allowed to commit `non_committable_url`, so it
  // must not have been adopted as the worker's final response URL.
  EXPECT_TRUE(observer.final_response_urls().empty())
      << "unexpected final response URL: "
      << observer.final_response_urls().front();
}

// Same as DedicatedWorkerRejectsNonCommittableFinalResponseUrl, but for shared
// workers.
IN_PROC_BROWSER_TEST_F(WorkerTest,
                       SharedWorkerRejectsNonCommittableFinalResponseUrl) {
  if (!SupportsSharedWorker()) {
    return;
  }

  const GURL main_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url = ssl_server()->GetURL("a.test", "/workers/worker.js");
  const GURL non_committable_url("file:///non_committable_path");

  WorkerFinalResponseURLObserver observer(shell()
                                              ->web_contents()
                                              ->GetBrowserContext()
                                              ->GetDefaultStoragePartition());

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != worker_url) {
          return false;
        }
        WriteWorkerScriptResponseWithServiceWorkerUrlList(
            params->client.get(), {non_committable_url});
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_EQ("error", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker = new SharedWorker('/workers/worker.js');
      worker.onerror = () => resolve('error');
      worker.port.onmessage = e => resolve(e.data);
    })
  )"));

  // The worker process is not allowed to commit `non_committable_url`, so it
  // must not have been adopted as the worker's final response URL.
  EXPECT_TRUE(observer.final_response_urls().empty())
      << "unexpected final response URL: "
      << observer.final_response_urls().front();
  EXPECT_FALSE(GetSharedWorkerHost(worker_url));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(GetTestURL("multi_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("single_worker.html", "shared=true"));
}

// A frame committed as PDF content (like the PDF viewer's content frame) runs
// in a process whose SiteInfo has `is_pdf` set. PDF documents never run
// script that could create dedicated workers, so the browser must refuse to
// bind blink.mojom.DedicatedWorkerHostFactory for such processes and
// terminate them instead.
IN_PROC_BROWSER_TEST_F(WorkerTest, DedicatedWorkerBlockedForPdfProcess) {
  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());
  const GURL url = ssl_server()->GetURL("a.test", "/title1.html");

  // Commit `url` as PDF content so that the resulting frame runs in a process
  // whose SiteInfo has `is_pdf` set.
  ASSERT_TRUE(NavigateToURLWithPdf(tab, url));

  RenderFrameHostImpl* frame = tab->GetPrimaryMainFrame();

  // Attempting to create a dedicated worker from a PDF frame triggers the
  // renderer to request blink.mojom.DedicatedWorkerHostFactory from
  // RenderFrameHost. The browser must reject this and terminate the process.
  if (base::FeatureList::IsEnabled(features::kPdfOpaqueOrigin)) {
    // With an opaque origin, the worker script URL is cross-origin to the
    // document, so Blink throws a SecurityError before the renderer ever asks
    // the browser for a DedicatedWorkerHostFactory. Nothing to terminate.
    EXPECT_EQ("SecurityError", EvalJs(frame, R"(
      (() => {
        try {
          new Worker('/workers/worker_common.js');
          return 'no error';
        } catch (e) {
          return e.name;
        }
      })()
    )"));
    EXPECT_TRUE(frame->IsRenderFrameLive());
  } else {
    RenderProcessHostBadIpcMessageWaiter kill_waiter(frame->GetProcess());
    ExecuteScriptAsync(frame, "new Worker('/workers/worker_common.js');");
    EXPECT_EQ(
        bad_message::RFH_DEDICATED_WORKER_HOST_FACTORY_PDF_PROCESS_BLOCKED,
        kill_waiter.Wait());
  }

  // Control: the same page committed normally can create dedicated workers.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_FALSE(
      tab->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteInfo().is_pdf());
  EXPECT_EQ("pong", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker = new Worker('/workers/worker_common.js');
      worker.onmessage = e => resolve(e.data);
      worker.postMessage('ping');
    })
  )"));
}

// A WebContents created with `disallow_shared_workers` must not be able to
// connect to a shared worker (which would otherwise bridge its process to
// ordinary content of the same origin, since shared worker matching ignores
// SiteInstance). An ordinary WebContents at the same URL still can.
IN_PROC_BROWSER_TEST_F(WorkerTest, PrivilegedWebContentsCannotUseSharedWorker) {
  if (!SupportsSharedWorker()) {
    return;
  }

  const GURL page_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url =
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js");
  static constexpr char kConnectSharedWorker[] = R"(
    new Promise(resolve => {
      const worker = new SharedWorker("/workers/messageport_worker.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )";

  // A privileged WebContents that disallows shared workers does not expose the
  // SharedWorker constructor in Blink.
  WebContents::CreateParams privileged_params(
      shell()->web_contents()->GetBrowserContext());
  WebContents::PrivilegedParams marker;
  marker.feature_id = 42;
  marker.disallow_shared_workers = true;
  privileged_params.privileged_params = marker;
  std::unique_ptr<WebContents> privileged(
      WebContents::Create(privileged_params));
  ASSERT_TRUE(NavigateToURL(privileged.get(), page_url));
  EXPECT_EQ(false, EvalJs(privileged.get(), "'SharedWorker' in window"));

  // Even if a renderer still has the SharedWorker interface enabled (e.g. a
  // compromised renderer), the browser process blocks the connection and no
  // shared worker host is created for it.
  class ForceEnableSharedWorkerObserver : public WebContentsObserver {
   public:
    explicit ForceEnableSharedWorkerObserver(WebContents* web_contents)
        : WebContentsObserver(web_contents) {}
    void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
      navigation_handle->GetMutableRuntimeFeatureStateContext()
          .SetSharedWorkerEnabled(true);
    }
  } force_enable_observer(privileged.get());
  ASSERT_TRUE(NavigateToURL(privileged.get(), page_url));
  EXPECT_EQ("Worker blocked.", EvalJs(privileged.get(), kConnectSharedWorker));
  EXPECT_FALSE(GetSharedWorkerHost(worker_url));

  // Control: an ordinary WebContents at the same URL exposes SharedWorker and
  // connects normally.
  ASSERT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(true, EvalJs(shell(), "'SharedWorker' in window"));
  EXPECT_EQ("Worker connected.", EvalJs(shell(), kConnectSharedWorker));
  EXPECT_TRUE(GetSharedWorkerHost(worker_url));
}

class PdfWorkerTest : public WorkerTest {
 public:
  PdfWorkerTest() {
    // In content_shell (content_browsertests), ContentRendererClient does not
    // override IsDomStorageDisabled() for PDF processes (unlike Chrome). As a
    // result, calling `new SharedWorker(...)` in JS invokes
    // LocalDOMWindow::GetPublicURLManager(), which attempts to bind
    // BlobURLStore and triggers a renderer kill under
    // kEnforcePdfBlobRestrictions before SharedWorkerConnector::Connect reaches
    // the browser process. Disable kEnforcePdfBlobRestrictions so that the
    // SharedWorker connection request reaches SharedWorkerServiceImpl.
    feature_list_.InitAndDisableFeature(
        blink::features::kEnforcePdfBlobRestrictions);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that frames in a PDF process do not connect to or create shared
// workers, while an ordinary WebContents at the same URL connects normally.
IN_PROC_BROWSER_TEST_F(PdfWorkerTest, PdfCannotUseSharedWorker) {
  if (!SupportsSharedWorker()) {
    return;
  }

  const GURL page_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url =
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js");
  static constexpr char kConnectSharedWorker[] = R"(
    new Promise(resolve => {
      try {
        window.worker = new SharedWorker("/workers/messageport_worker.js");
      } catch (e) {
        resolve(e.name);
        return;
      }
      window.worker.onerror = (e) => resolve("Worker blocked.");
      window.worker.port.onmessage = (e) => resolve(e.data);
    })
  )";

  // With `features::kPdfOpaqueOrigin`, the PDF frame has an opaque origin, so
  // Blink throws a SecurityError before the renderer ever asks the browser for
  // a SharedWorkerConnector. Otherwise, the browser blocks the connection and
  // the worker's error event fires.
  const char* expected_pdf_result =
      base::FeatureList::IsEnabled(features::kPdfOpaqueOrigin)
          ? "SecurityError"
          : "Worker blocked.";

  // 1. A PDF frame attempting to create a shared worker is blocked.
  ASSERT_TRUE(NavigateToURLWithPdf(shell()->web_contents(), page_url));
  EXPECT_EQ(expected_pdf_result,
            EvalJs(shell()->web_contents(), kConnectSharedWorker));
  EXPECT_FALSE(GetSharedWorkerHost(worker_url));

  // 2. Control: an ordinary WebContents at the same URL connects and creates
  // the shared worker normally.
  Shell* ordinary_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(ordinary_shell, page_url));
  RenderFrameHostImpl* ordinary_rfh = static_cast<RenderFrameHostImpl*>(
      ordinary_shell->web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(ordinary_rfh->GetProcess()->IsPdf());
  EXPECT_EQ("Worker connected.", EvalJs(ordinary_shell, kConnectSharedWorker));
  SharedWorkerHost* host = GetSharedWorkerHost(worker_url);
  ASSERT_TRUE(host);
  EXPECT_EQ(std::vector<GlobalRenderFrameHostId>{ordinary_rfh->GetGlobalId()},
            host->GetRenderFrameIDsForWorker());

  // 3. A PDF frame is also blocked from connecting to the existing shared
  // worker, and the existing worker host remains unaffected.
  EXPECT_EQ(expected_pdf_result,
            EvalJs(shell()->web_contents(), kConnectSharedWorker));
  EXPECT_EQ(host, GetSharedWorkerHost(worker_url));
  EXPECT_EQ(std::vector<GlobalRenderFrameHostId>{ordinary_rfh->GetGlobalId()},
            host->GetRenderFrameIDsForWorker());
}

// Create a SharedWorker from a COEP:required-corp document.
IN_PROC_BROWSER_TEST_F(WorkerTest, SharedWorkerInCOEPRequireCorpDocument) {
  if (!SupportsSharedWorker())
    return;

  // Navigate to a page living in an isolated process.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/cross-origin-isolated.html")));
  RenderFrameHostImpl* page_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto page_lock =
      ProcessLock::FromSiteInfo(page_rfh->GetSiteInstance()->GetSiteInfo());
  EXPECT_TRUE(page_lock.GetWebExposedIsolationInfo().is_isolated());
  EXPECT_GT(page_rfh->GetWebExposedIsolationLevel(),
            WebExposedIsolationLevel::kNotIsolated);

  // Create a shared worker from the cross-origin-isolated page:

  // COEP:unsafe-none
  //
  // With CoepForSharedWorker: the worker's COEP policy is laxer than its
  // creator, it is blocked as a result. It can't communicate with the document,
  // outside of the worker.onerror message.
  // Without CoepForSharedWorker: the worker isn't blocked, but it should at
  // least not be loaded in the cross-origin isolated process.
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host = GetSharedWorkerHost(
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js"));
  EXPECT_TRUE(host);
  RenderProcessHost* worker_rph = host->GetProcessHost();
  EXPECT_NE(worker_rph, page_rfh->GetProcess());
  auto worker_lock =
      ProcessLock::FromSiteInfo(host->site_instance()->GetSiteInfo());
  EXPECT_FALSE(worker_lock.GetWebExposedIsolationInfo().is_isolated());

  // COEP:credentialless
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_credentialless.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_credentialless = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_credentialless.js"));
  EXPECT_TRUE(host_credentialless);
  RenderProcessHost* worker_rph_credentialless =
      host_credentialless->GetProcessHost();
  EXPECT_NE(worker_rph_credentialless, page_rfh->GetProcess());
  auto worker_lock_credentialless = ProcessLock::FromSiteInfo(
      host_credentialless->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:credentialless
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_credentialless.GetWebExposedIsolationInfo().is_isolated());

  // COEP:require-corp
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_require_corp.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_require_corp = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_require_corp.js"));
  RenderProcessHost* worker_rph_require_corp =
      host_require_corp->GetProcessHost();
  EXPECT_NE(worker_rph_require_corp, page_rfh->GetProcess());
  auto worker_lock_require_corp = ProcessLock::FromSiteInfo(
      host_require_corp->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:require-corp
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_require_corp.GetWebExposedIsolationInfo().is_isolated());
}

// Create a SharedWorker from a COEP:credentialless document.
IN_PROC_BROWSER_TEST_F(WorkerTest, SharedWorkerInCOEPCredentiallessDocument) {
  if (!SupportsSharedWorker())
    return;

  // Navigate to a page living in an isolated process.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL(
                   "a.test", "/cross-origin-isolated-credentialless.html")));
  RenderFrameHostImpl* page_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto page_lock =
      ProcessLock::FromSiteInfo(page_rfh->GetSiteInstance()->GetSiteInfo());
  EXPECT_TRUE(page_lock.GetWebExposedIsolationInfo().is_isolated());

  // Create a SharedWorker from the cross-origin-isolated page.

  // COEP:unsafe-none
  //
  // With CoepForSharedWorker: the worker's COEP policy is laxer than its
  // creator, it is blocked as a result. It can't communicate with the document,
  // outside of the worker.onerror message.
  // Without CoepForSharedWorker: the worker isn't blocked, but it should at
  // least not be loaded in the cross-origin isolated process.
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host = GetSharedWorkerHost(
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js"));
  EXPECT_TRUE(host);
  RenderProcessHost* worker_rph = host->GetProcessHost();
  EXPECT_NE(worker_rph, page_rfh->GetProcess());
  auto worker_lock =
      ProcessLock::FromSiteInfo(host->site_instance()->GetSiteInfo());
  EXPECT_FALSE(worker_lock.GetWebExposedIsolationInfo().is_isolated());

  // COEP:credentialless
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_credentialless.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_credentialless = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_credentialless.js"));
  EXPECT_TRUE(host_credentialless);
  RenderProcessHost* worker_rph_credentialless =
      host_credentialless->GetProcessHost();
  EXPECT_NE(worker_rph_credentialless, page_rfh->GetProcess());
  auto worker_lock_credentialless = ProcessLock::FromSiteInfo(
      host_credentialless->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:credentialless
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_credentialless.GetWebExposedIsolationInfo().is_isolated());

  // COEP:require-corp
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_require_corp.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_require_corp = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_require_corp.js"));
  RenderProcessHost* worker_rph_require_corp =
      host_require_corp->GetProcessHost();
  EXPECT_NE(worker_rph_require_corp, page_rfh->GetProcess());
  auto worker_lock_require_corp = ProcessLock::FromSiteInfo(
      host_require_corp->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:require-corp
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_require_corp.GetWebExposedIsolationInfo().is_isolated());
}

// http://crbug.com/96435
IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("multi_worker.html", "shared=true"));
}

// Incognito windows should not share workers with non-incognito windows
// http://crbug.com/30021
IN_PROC_BROWSER_TEST_F(WorkerTest, IncognitoSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  // Load a non-incognito tab and have it create a shared worker
  RunTest(ssl_server()->GetURL("a.test", "/workers/incognito_worker.html"));

  // Incognito worker should not share with non-incognito
  RunTest(CreateOffTheRecordBrowser(),
          ssl_server()->GetURL("a.test", "/workers/incognito_worker.html"));
}

// Make sure that auth dialog is displayed from worker context.
// http://crbug.com/33344
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerHttpAuth) {
  GURL url = ssl_server()->GetURL("a.test", "/workers/worker_auth.html");

  NavigateAndWaitForAuth(url);
}

// Tests that TLS client auth prompts for normal workers's importScripts.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerTlsClientAuthImportScripts) {
  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=import&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests that TLS client auth prompts for normal workers's fetch() call.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerTlsClientAuthFetch) {
  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=fetch&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests that TLS client auth does not prompt for a shared worker; shared
// workers are not associated with a WebContents.
IN_PROC_BROWSER_TEST_F(WorkerTest, SharedWorkerTlsClientAuthImportScripts) {
  if (!SupportsSharedWorker())
    return;

  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=import&shared=true&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(0, select_certificate_count());
}

IN_PROC_BROWSER_TEST_F(WorkerTest, WebSocketSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  // Launch WebSocket server.
  net::EmbeddedTestServer ws_server(net::EmbeddedTestServer::TYPE_HTTP);
  net::test_server::InstallDefaultWebSocketHandlers(&ws_server);
  // Needed to add the file handler.
  ws_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(ws_server.Start());

  // Run test.
  Shell* window = shell();
  const std::u16string expected_title = u"OK";
  TitleWatcher title_watcher(window->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(
      window, ws_server.GetURL("/workers/websocket_shared_worker.html")));
  std::u16string final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

IN_PROC_BROWSER_TEST_F(WorkerTest, PassMessagePortToSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("pass_messageport_to_sharedworker.html", ""));
}

IN_PROC_BROWSER_TEST_F(WorkerTest,
                       PassMessagePortToSharedWorkerDontWaitForConnect) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL(
      "pass_messageport_to_sharedworker_dont_wait_for_connect.html", ""));
}

// Tests the value of |request_initiator| for shared worker resources.
IN_PROC_BROWSER_TEST_F(WorkerTest,
                       VerifyInitiatorAndSameSiteCookiesSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  const GURL start_url(ssl_server()->GetURL("b.test", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // To make things tricky about |top_frame_origin|, this test navigates to
  // a page on |ssl_server()| which has a cross-origin iframe that registers the
  // worker.
  std::string cross_site_domain("a.test");
  const GURL test_url(ssl_server()->GetURL(
      cross_site_domain, "/workers/simple_shared_worker.html"));

  // There are three requests to test:
  // 1) The request for the worker itself ("worker.js")
  // 2) importScripts("empty.js") from the worker
  // 3) fetch("empty.html") from the worker
  const GURL worker_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/worker.js"));
  const GURL script_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/empty.js"));
  const GURL resource_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/empty.html"));

  // Set a cookie for verifying which requests send SameSite cookies.
  SetSameSiteLaxCookie(cross_site_domain);

  std::set<GURL> expected_request_urls = {worker_url, script_url, resource_url};
  const url::Origin expected_origin =
      url::Origin::Create(worker_url.DeprecatedGetOriginAsURL());

  base::RunLoop waiter;
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it != expected_request_urls.end()) {
          EXPECT_TRUE(params->url_request.request_initiator.has_value());
          EXPECT_EQ(expected_origin,
                    params->url_request.request_initiator.value());
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty())
          waiter.Quit();
        return false;
      }));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), test_url));
  waiter.Run();

  // Check cookies sent with each request to "a.test".
  // Neither the frame nor the SharedWorker should get SameSite cookies.
  EXPECT_EQ(kNoCookie, GetReceivedCookie(test_url.GetPath()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(worker_url.GetPath()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(script_url.GetPath()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(resource_url.GetPath()));
}

// Test that an "a.test" worker sends "a.test" SameSite cookies, both when
// requesting the worker script and when fetching other resources.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerSameSiteCookies1) {
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "a.test",
          "/workers/create_worker.html?worker_url=fetch_from_worker.js")));
  EXPECT_EQ(kSameSiteLaxCookie,
            EvalJs(shell()->web_contents(),
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kSameSiteLaxCookie,
            GetReceivedCookie(
                "/workers/create_worker.html?worker_url=fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie,
            GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that a "b.test" worker does not send "a.test" SameSite cookies when
// fetching resources.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerSameSiteCookies2) {
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "b.test",
          "/workers/create_worker.html?worker_url=fetch_from_worker.js")));
  EXPECT_EQ(kNoCookie,
            EvalJs(shell()->web_contents(),
                   JsReplace("worker.postMessage({url: $1}); waitForMessage();",
                             ssl_server()
                                 ->GetURL("a.test", "/echoheader?Cookie")
                                 .spec()
                                 .c_str())));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "a.test" nested worker sends "a.test" SameSite cookies, both
// when requesting the worker script and when fetching other resources.
IN_PROC_BROWSER_TEST_F(WorkerTest, NestedWorkerSameSiteCookies) {
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "a.test",
          "/workers/"
          "create_worker.html?worker_url=fetch_from_nested_worker.js")));
  EXPECT_EQ(kSameSiteLaxCookie,
            EvalJs(shell()->web_contents(),
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kSameSiteLaxCookie,
            GetReceivedCookie(
                "/workers/"
                "create_worker.html?worker_url=fetch_from_nested_worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie,
            GetReceivedCookie("/workers/fetch_from_nested_worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie,
            GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "a.test" iframe in a "b.test" frame does not send same-site
// cookies when requesting an "a.test" worker or when that worker requests
// "a.test" resources.
IN_PROC_BROWSER_TEST_F(WorkerTest,
                       CrossOriginIframeWorkerDoesNotSendSameSiteCookies1) {
  SetSameSiteLaxCookie("a.test");

  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));

  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);

  const char kSubframeName[] = "foo";
  EvalJsResult result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()
              ->GetURL(
                  "a.test",
                  "/workers/create_worker.html?worker_url=fetch_from_worker.js")
              .spec()
              .c_str(),
          kSubframeName));
  ASSERT_TRUE(result.is_ok());
  navigation_observer.Wait();

  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EXPECT_EQ(kNoCookie,
            EvalJs(subframe_rfh,
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kNoCookie,
            GetReceivedCookie(
                "/workers/create_worker.html?worker_url=fetch_from_worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "b.test" iframe in a "a.test" frame does not send same-site
// cookies when its "b.test" worker requests "a.test" resources.
IN_PROC_BROWSER_TEST_F(WorkerTest,
                       CrossOriginIframeWorkerDoesNotSendSameSiteCookies2) {
  SetSameSiteLaxCookie("a.test");

  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/frame_factory.html")));

  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);

  const char kSubframeName[] = "foo";
  EvalJsResult result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()
              ->GetURL(
                  "b.test",
                  "/workers/create_worker.html?worker_url=fetch_from_worker.js")
              .spec()
              .c_str(),
          kSubframeName));
  ASSERT_TRUE(result.is_ok());
  navigation_observer.Wait();

  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EXPECT_EQ(kNoCookie,
            EvalJs(subframe_rfh,
                   JsReplace("worker.postMessage({url: $1}); waitForMessage();",
                             ssl_server()
                                 ->GetURL("a.test", "/echoheader?Cookie")
                                 .spec()
                                 .c_str())));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

class WorkerFromCredentiallessIframeNikBrowserTest : public WorkerTest {
 public:
  WorkerFromCredentiallessIframeNikBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable parsing the iframe 'credentialless' attribute.
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
    WorkerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    connection_tracker_ = std::make_unique<net::test_server::ConnectionTracker>(
        embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    WorkerTest::SetUpOnMainThread();
  }

  void ResetNetworkState() {
    auto* network_context = shell()
                                ->web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetNetworkContext();
    base::RunLoop close_all_connections_loop;
    network_context->CloseAllConnections(
        close_all_connections_loop.QuitClosure());
    close_all_connections_loop.Run();

    connection_tracker_->ResetCounts();
  }

 protected:
  std::unique_ptr<net::test_server::ConnectionTracker> connection_tracker_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WorkerFromCredentiallessIframeNikBrowserTest,
                       SharedWorkerRequestIsDoneWithPartitionedNetworkState) {
  if (!SupportsSharedWorker())
    return;

  GURL main_url = embedded_test_server()->GetURL("/title1.html");

  for (bool credentialless : {false, true}) {
    SCOPED_TRACE(credentialless ? "credentialless iframe" : "normal iframe");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());

    // Create an iframe.
    EXPECT_TRUE(ExecJs(main_rfh,
                       JsReplace("let child = document.createElement('iframe');"
                                 "child.src = $1;"
                                 "child.credentialless = $2;"
                                 "document.body.appendChild(child);",
                                 main_url, credentialless)));
    WaitForLoadStop(shell()->web_contents());
    EXPECT_EQ(1U, main_rfh->child_count());
    RenderFrameHostImpl* iframe = main_rfh->child_at(0)->current_frame_host();
    EXPECT_EQ(credentialless, iframe->IsCredentialless());
    EXPECT_EQ(credentialless, EvalJs(iframe, "window.credentialless"));
    ResetNetworkState();

    GURL worker_url = embedded_test_server()->GetURL("/workers/worker.js");

    // Preconnect a socket with the NetworkAnonymizationKey of the main frame.
    // TODO(crbug.com/447954811): pass the `network_restrictions_id` from the
    // caller.
    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->PreconnectSockets(1, worker_url.DeprecatedGetOriginAsURL(),
                            network::mojom::CredentialsMode::kInclude,
                            main_rfh->GetIsolationInfoForSubresources()
                                .network_anonymization_key(),
                            network::GetTestNetworkRestrictionsId(),
                            net::MutableNetworkTrafficAnnotationTag(
                                TRAFFIC_ANNOTATION_FOR_TESTS),
                            std::nullopt, mojo::NullRemote());

    connection_tracker_->WaitForAcceptedConnections(1);
    EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    EXPECT_EQ(0u, connection_tracker_->GetReadSocketCount());

    std::string start_worker = JsReplace("new SharedWorker($1);", worker_url);

    ExecuteScriptAsync(iframe, start_worker);
    connection_tracker_->WaitUntilConnectionRead();

    // The normal iframe should reuse the preconnected socket, the
    // credentialless iframe should open a new one.
    if (credentialless) {
      EXPECT_EQ(2u, connection_tracker_->GetAcceptedSocketCount());
    } else {
      EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    }
    EXPECT_EQ(1u, connection_tracker_->GetReadSocketCount());
  }
}

// Test that an "a.test" frame starting a worker without any `sameSiteCookies`
// option sends SameSite cookies on the request.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerSameDefault) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result =
      EvalJs(shell(), "new SharedWorker('/workers/worker.js');");
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" frame starting a worker with `sameSiteCookies: 'none'`
// doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerSameNone) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result = EvalJs(
      shell(),
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'none'});");
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" frame starting a worker with `sameSiteCookies: 'none'`
// sends SameSite cookies on the request.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerSameAll) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result = EvalJs(
      shell(),
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'all'});");
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kSameSiteLaxCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame starting a worker without
// any `sameSiteCookies` option doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerCrossDefault) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult frame_result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(frame_result.is_ok());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result =
      EvalJs(subframe_rfh, "new SharedWorker('/workers/worker.js');");
  ASSERT_TRUE(worker_result.is_ok());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame starting a worker with
// `sameSiteCookies: 'none'` doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerCrossNone) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult frame_result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(frame_result.is_ok());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result = EvalJs(
      subframe_rfh,
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'none'});");
  ASSERT_TRUE(worker_result.is_ok());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame cannot set
// `sameSiteCookies: 'all'` option when starting a shared worker.
IN_PROC_BROWSER_TEST_F(WorkerTest, SameSiteCookiesSharedWorkerCrossAll) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult result_frame = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(result_frame.is_ok());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result = EvalJs(
      subframe_rfh,
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'all'});");
  ASSERT_FALSE(worker_result.is_ok());
}

// Test that an "a.test" iframe in a "b.test" frame starting a first-party
// shared worker via the Storage Access API (`handle.SharedWorker(...)`) sends
// "a.test"'s unpartitioned SameSite=None and cross-site-partition
// (AncestorChainBit::kCrossSite) SameSite=None; Partitioned cookies, rather
// than SameSite=Strict/Lax or first-party-partition
// (AncestorChainBit::kSameSite) cookies, on worker subresource requests.
IN_PROC_BROWSER_TEST_F(
    WorkerTest,
    StorageAccessSharedWorkerSubresourcesDoNotSendFirstPartyPartitionedCookies) {
  if (!SupportsSharedWorker()) {
    GTEST_SKIP() << "SharedWorker not supported";
  }
  BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  const GURL a_cookie_url = ssl_server()->GetURL("a.test", "/");
  ASSERT_TRUE(SetCookie(
      browser_context, a_cookie_url,
      base::StrCat({kSameSiteStrictCookie, "; SameSite=Strict; Secure"})));
  SetSameSiteLaxCookie("a.test");
  ASSERT_TRUE(SetCookie(
      browser_context, a_cookie_url,
      base::StrCat({kSameSiteNoneCookie, "; SameSite=None; Secure"})));
  SetPartitionedCookie("a.test", kFirstPartyPartitionedCookie,
                       net::CookiePartitionKey::AncestorChainBit::kSameSite);
  SetPartitionedCookie("a.test", kCrossSitePartitionedCookie,
                       net::CookiePartitionKey::AncestorChainBit::kCrossSite);

  const GURL top_url =
      ssl_server()->GetURL("b.test", "/workers/frame_factory.html");
  const GURL subframe_url =
      ssl_server()->GetURL("a.test", "/workers/simple.html");
  ASSERT_TRUE(NavigateToURL(shell(), top_url));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*expected_number_of_navigations=*/1);
  const char kSubframeName[] = "foo";
  EXPECT_TRUE(ExecJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace("createFrame($1, $2)", subframe_url.spec(), kSubframeName)));
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);

  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> future;
  static_cast<PermissionControllerImpl*>(
      subframe_rfh->GetBrowserContext()->GetPermissionController())
      ->SetPermissionOverride(
          /*requesting_origin=*/url::Origin::Create(subframe_url),
          /*embedding_origin=*/url::Origin::Create(top_url),
          blink::PermissionType::STORAGE_ACCESS_GRANT,
          blink::mojom::PermissionStatus::GRANTED, future.GetCallback());
  ASSERT_EQ(future.Get(),
            PermissionControllerImpl::OverrideStatus::kOverrideSet);

  EXPECT_TRUE(ExecJs(subframe_rfh, R"(
    (async () => {
      const handle = await document.requestStorageAccess({SharedWorker: true});
      handle.SharedWorker('/workers/worker.js');
    })()
  )"));
  const std::string expected_cookies = base::JoinString(
      {kSameSiteNoneCookie, kCrossSitePartitionedCookie}, "; ");
  EXPECT_EQ(expected_cookies, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(expected_cookies, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(expected_cookies, GetReceivedCookie("/workers/empty.html"));
}

// Test for the SharedWorker extendedLifetime option.
// See: https://github.com/whatwg/html/issues/10997
class SharedWorkerExtendedLifetimeBrowserTest : public ContentBrowserTest {
 public:
  SharedWorkerExtendedLifetimeBrowserTest()
      : features_({blink::features::kSharedWorkerExtendedLifetime}) {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  SharedWorkerHost* CreateSharedWorkerInMainURL() {
    GURL main_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js",
                          {extendedLifetime: true});
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
    auto* host = GetSharedWorkerHost(
        embedded_test_server()->GetURL("/workers/messageport_worker.js"));
    return host;
  }
  SharedWorkerHost* GetSharedWorkerHostFromToken(
      const blink::SharedWorkerToken& token) {
    return GetSharedWorkerService()->GetSharedWorkerHostFromToken(token);
  }

 private:
  SharedWorkerServiceImpl* GetSharedWorkerService() {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    DCHECK(partition);
    return static_cast<SharedWorkerServiceImpl*>(
        partition->GetSharedWorkerService());
  }
  SharedWorkerHost* GetSharedWorkerHost(const GURL& url) {
    auto* service = GetSharedWorkerService();
    return service->FindMatchingSharedWorkerHost(
        url, "", blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        blink::mojom::SharedWorkerSameSiteCookies::kAll);
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SharedWorkerExtendedLifetimeBrowserTest,
                       EnsureExtendLifetime) {
  if (!SupportsSharedWorker()) {
    return;
  }

  auto* host = CreateSharedWorkerInMainURL();
  EXPECT_TRUE(host);
  EXPECT_TRUE(host->instance().extended_lifetime());
  auto token = host->token();

  // Navigate to the other page to the other URL.
  GURL other_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), other_url));
  // Ensure the SharedWorker exist.
  EXPECT_TRUE(GetSharedWorkerHostFromToken(token));

  // Navigate back to the main URL.
  auto* host2 = CreateSharedWorkerInMainURL();
  EXPECT_TRUE(host2);
  EXPECT_TRUE(host2->instance().extended_lifetime());

  // Since the extended lifetime is enabled, SharedWorkerHost should be the
  // same.
  EXPECT_EQ(host, host2);
}

class SharedWorkerExtendedLifetimeBrowserOriginTrialTest
    : public SharedWorkerExtendedLifetimeBrowserTest {
 public:
  SharedWorkerExtendedLifetimeBrowserOriginTrialTest() {
    // Explicitly disable the feature.
    features_.InitWithFeatures(
        {}, {blink::features::kSharedWorkerExtendedLifetime});
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default privatey key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kOriginTrialTestPublicKey);
  }

  SharedWorkerHost* GetSharedWorkerHost(const GURL& url) {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    DCHECK(partition);
    auto* service = static_cast<SharedWorkerServiceImpl*>(
        partition->GetSharedWorkerService());
    return service->FindMatchingSharedWorkerHost(
        url, "", blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        blink::mojom::SharedWorkerSameSiteCookies::kAll);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SharedWorkerExtendedLifetimeBrowserOriginTrialTest,
                       Basic) {
  if (!SupportsSharedWorker()) {
    return;
  }

  embedded_test_server()->StartAcceptingConnections();

  // The URL that was used to register the Origin Trial token.
  static constexpr char kOriginUrl[] = "https://127.0.0.1:44444";
  // Generated by running (in tools/origin_trials):
  // $ tools/origin_trials/generate_token.py https://127.0.0.1:44444 \
  // SharedWorkerExtendedLifetime --expire-timestamp=2000000000
  static constexpr char kOriginTrialToken[] =
      "AzjgBY2MbbdL8LkRTaCTW9YVLuvei0SwokOz+"
      "XqA24rqL3DV5QunyXddT1SkivH7curgzFjvXj83ZefHUD5SgAYAAABmeyJvcmlnaW4iOiAia"
      "HR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZSI6ICJTaGFyZWRXb3JrZXJFeHRlb"
      "mRlZExpZmV0aW1lIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

  const GURL main_url(base::StrCat({kOriginUrl, "/title1.html"}));
  const GURL shared_worker_url(
      base::StrCat({kOriginUrl, "/workers/messageport_worker.js"}));

  std::map<GURL, int /* number_of_invocations */> expected_request_urls = {
      {main_url, 1},
      {shared_worker_url, 1},
  };

  base::RunLoop run_loop;

  // The origin trial token is associated with an origin. We can't guarantee the
  // EmbeddedTestServer to use a specific port. So the URLLoaderInterceptor is
  // used instead.
  URLLoaderInterceptor shared_worker_loader(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it == expected_request_urls.end()) {
          return false;
        }

        const std::string content_type =
            base::EndsWith(params->url_request.url.path(), ".js")
                ? "text/javascript"
                : "text/html";

        const std::string headers = base::ReplaceStringPlaceholders(
            "HTTP/1.1 200 OK\n"
            "Content-type: $1\n"
            "Origin-Trial: $2\n"
            "\n",
            {content_type, kOriginTrialToken}, {});

        URLLoaderInterceptor::WriteResponse(
            "content/test/data" + params->url_request.url.GetPath(),
            params->client.get(), &headers, std::optional<net::SSLInfo>(),
            params->url_request.url);

        if (--it->second == 0) {
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty()) {
          run_loop.Quit();
        }
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js",
                          {extendedLifetime: true});
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));

  auto* host = GetSharedWorkerHost(shared_worker_url);
  EXPECT_TRUE(host);
  EXPECT_TRUE(host->instance().extended_lifetime());

  run_loop.Run();
}

class CrossOriginWorkerScriptTest : public WorkerTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  CrossOriginWorkerScriptTest() {
    feature_list_.InitWithFeatureState(
        blink::features::kNoSynchronousThrowForCrossOriginBlockedWorker,
        FeatureEnabled());
  }

 protected:
  bool FeatureEnabled() { return GetParam(); }

  EvalJsResult CreateWorker(const GURL& worker_url) {
    return EvalJs(shell(), JsReplace(R"(
      new Promise(resolve => {
        let worker;
        try {
          worker = new Worker($1);
        } catch (e) {
          resolve('exception: ' + e.name);
          return;
        }
        worker.onmessage = () => resolve('message');
        worker.onerror = () => resolve('error');
      })
    )",
                                     worker_url));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, CrossOriginWorkerScriptTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(CrossOriginWorkerScriptTest,
                       CrossOriginScriptIsBlockedBeforeFetching) {
  const GURL page_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url =
      ssl_server()->GetURL("b.test", "/workers/post_ready.js");

  ASSERT_TRUE(NavigateToURL(shell(), page_url));

  URLLoaderMonitor monitor;
  EXPECT_EQ(FeatureEnabled() ? "error" : "exception: SecurityError",
            CreateWorker(worker_url));

  EXPECT_FALSE(monitor.GetRequestInfo(worker_url));
}

// TODO(crbug.com/581125012): Flaky on Android because
// URLLoaderMonitor::GetCompletionStatus() is queried without waiting for the
// aborted initial redirect request's OnComplete IPC on the IO thread.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CrossOriginRedirectFailsFetchWithSameOriginMode \
  DISABLED_CrossOriginRedirectFailsFetchWithSameOriginMode
#else
#define MAYBE_CrossOriginRedirectFailsFetchWithSameOriginMode \
  CrossOriginRedirectFailsFetchWithSameOriginMode
#endif
IN_PROC_BROWSER_TEST_P(CrossOriginWorkerScriptTest,
                       MAYBE_CrossOriginRedirectFailsFetchWithSameOriginMode) {
  const GURL page_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL redirect_target =
      ssl_server()->GetURL("b.test", "/workers/post_ready.js");
  const GURL worker_url = ssl_server()->GetURL(
      "a.test", "/server-redirect?" + redirect_target.spec());

  ASSERT_TRUE(NavigateToURL(shell(), page_url));

  URLLoaderMonitor monitor;
  EXPECT_EQ("error", CreateWorker(worker_url));

  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(worker_url);
  ASSERT_TRUE(request);
  EXPECT_EQ(network::mojom::RequestMode::kSameOrigin, request->mode);

  std::optional<network::URLLoaderCompletionStatus> status =
      monitor.GetCompletionStatus(worker_url);
  ASSERT_TRUE(status);
  EXPECT_EQ(net::ERR_ABORTED, status->error_code);
  EXPECT_FALSE(status->cors_error_status);
}

IN_PROC_BROWSER_TEST_P(CrossOriginWorkerScriptTest, SameOriginScriptStillRuns) {
  const GURL page_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL worker_url =
      ssl_server()->GetURL("a.test", "/workers/post_ready.js");

  ASSERT_TRUE(NavigateToURL(shell(), page_url));

  URLLoaderMonitor monitor;
  EXPECT_EQ("message", CreateWorker(worker_url));

  std::optional<network::URLLoaderCompletionStatus> status =
      monitor.GetCompletionStatus(worker_url);
  ASSERT_TRUE(status);
  EXPECT_EQ(net::OK, status->error_code);
  EXPECT_FALSE(status->cors_error_status);
}

class XMLHttpRequestLoadIgnoreLimitsTest
    : public WorkerTest,
      public testing::WithParamInterface<bool> {
 public:
  XMLHttpRequestLoadIgnoreLimitsTest() {
    feature_list_.InitWithFeatureState(
        blink::features::kRequiresLoadIgnoreLimitsForDomWindowsOnly,
        GetParam());
  }

  bool IsRequiresLoadIgnoreLimitsForDomWindowsOnlyEnabled() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         XMLHttpRequestLoadIgnoreLimitsTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(XMLHttpRequestLoadIgnoreLimitsTest, WorkerContext) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/workers/simple.html")));

  GURL worker_async_url = embedded_test_server()->GetURL("/echo?async");
  GURL worker_sync_url = embedded_test_server()->GetURL("/echo?sync");

  URLLoaderMonitor monitor({worker_async_url, worker_sync_url});

  // Async XHR in a Worker context should not set LOAD_IGNORE_LIMITS on the TCP
  // request and bypass socket pool size limitations.
  EXPECT_EQ("Echo", EvalJs(shell(), JsReplace(R"(
    new Promise((resolve, reject) => {
      const blob = new Blob([`
        const xhr = new XMLHttpRequest();
        xhr.open('GET', $1);
        xhr.onload = function () {
          postMessage(xhr.responseText);
        };
        xhr.send();
      `], {type: 'application/javascript'});
      const worker = new Worker(URL.createObjectURL(blob));
      worker.onmessage = (e) => resolve(e.data);
      worker.onerror = (e) => reject(e.message);
    })
  )",
                                              worker_async_url)));
  std::optional<network::ResourceRequest> worker_async_request =
      monitor.GetRequestInfo(worker_async_url);
  ASSERT_TRUE(worker_async_request.has_value());
  EXPECT_FALSE(
      (bool)(worker_async_request->load_flags & net::LOAD_IGNORE_LIMITS));

  // Sync XHR in a Worker context should set LOAD_IGNORE_LIMITS,
  // only if kRequiresLoadIgnoreLimitsForDomWindowsOnly is disabled.
  EXPECT_EQ("Echo", EvalJs(shell(), JsReplace(R"(
    new Promise((resolve, reject) => {
      const blob = new Blob([`
        const xhr = new XMLHttpRequest();
        xhr.open('GET', $1, false);
        xhr.onload = function () {
          postMessage(xhr.responseText);
        };
        xhr.send();
      `], {type: 'application/javascript'});
      const worker = new Worker(URL.createObjectURL(blob));
      worker.onmessage = (e) => resolve(e.data);
      worker.onerror = (e) => reject(e.message);
    })
  )",
                                              worker_sync_url)));
  std::optional<network::ResourceRequest> worker_sync_request =
      monitor.GetRequestInfo(worker_sync_url);
  ASSERT_TRUE(worker_sync_request.has_value());
  EXPECT_EQ(!IsRequiresLoadIgnoreLimitsForDomWindowsOnlyEnabled(),
            (bool)(worker_sync_request->load_flags & net::LOAD_IGNORE_LIMITS));
}

namespace {

class TestDedicatedWorkerHostFactoryClient
    : public blink::mojom::DedicatedWorkerHostFactoryClient {
 public:
  TestDedicatedWorkerHostFactoryClient() = default;
  ~TestDedicatedWorkerHostFactoryClient() override = default;

  mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitUntilFinished() { run_loop_.Run(); }

  bool script_load_started() const { return script_load_started_; }
  bool script_load_start_failed() const { return script_load_start_failed_; }
  const blink::mojom::ServiceWorkerContainerInfoForClientPtr&
  service_worker_container_info() const {
    return service_worker_container_info_;
  }
  const blink::mojom::ControllerServiceWorkerInfoPtr& controller_info() const {
    return controller_info_;
  }
  const blink::mojom::WorkerMainScriptLoadParamsPtr& main_script_load_params()
      const {
    return main_script_load_params_;
  }

  // blink::mojom::DedicatedWorkerHostFactoryClient:
  void OnWorkerHostCreated(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      mojo::PendingRemote<blink::mojom::DedicatedWorkerHost> host,
      const url::Origin& origin) override {
    browser_interface_broker_.Bind(std::move(browser_interface_broker));
    remote_host_.Bind(std::move(host));
  }

  void OnScriptLoadStarted(
      blink::mojom::ServiceWorkerContainerInfoForClientPtr
          service_worker_container_info,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          pending_subresource_loader_factory_bundle,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
          back_forward_cache_controller_host,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          coep_reporting_observer,
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          dip_reporting_observer) override {
    script_load_started_ = true;
    service_worker_container_info_ = std::move(service_worker_container_info);
    main_script_load_params_ = std::move(main_script_load_params);
    controller_info_ = std::move(controller_info);
    run_loop_.Quit();
  }

  void OnScriptLoadStartFailed() override {
    script_load_start_failed_ = true;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<blink::mojom::DedicatedWorkerHostFactoryClient> receiver_{
      this};
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> browser_interface_broker_;
  mojo::Remote<blink::mojom::DedicatedWorkerHost> remote_host_;
  bool script_load_started_ = false;
  bool script_load_start_failed_ = false;
  blink::mojom::ServiceWorkerContainerInfoForClientPtr
      service_worker_container_info_;
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params_;
  blink::mojom::ControllerServiceWorkerInfoPtr controller_info_;
};

void StartDedicatedWorkerScriptLoadForFrame(
    RenderFrameHostImpl* frame,
    const GURL& script_url,
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory>& factory_out,
    TestDedicatedWorkerHostFactoryClient& client_out) {
  frame->CreateDedicatedWorkerHostFactory(
      factory_out.BindNewPipeAndPassReceiver());
  auto fetch_settings = blink::mojom::FetchClientSettingsObject::New();
  fetch_settings->policy_container_policies =
      blink::mojom::PolicyContainerPolicies::New();
  fetch_settings->outgoing_referrer = frame->GetLastCommittedURL();
  factory_out->CreateWorkerHostAndStartScriptLoad(
      blink::DedicatedWorkerToken(), script_url,
      network::mojom::CredentialsMode::kSameOrigin, std::move(fetch_settings),
      mojo::NullRemote(), client_out.BindNewPipeAndPassRemote());
  client_out.WaitUntilFinished();
}

}  // namespace

class SandboxedDedicatedWorkerServiceWorkerBrowserTest : public WorkerTest {
 public:
  void SetUpOnMainThread() override {
    ssl_server()->RegisterRequestHandler(base::BindRepeating(
        &SandboxedDedicatedWorkerServiceWorkerBrowserTest::HandleRequest,
        base::Unretained(this)));
    WorkerTest::SetUpOnMainThread();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/sw_test_worker.js") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/javascript");
      response->set_content(R"(
        let handledScriptCount = 0;
        self.addEventListener('install', event => {
          event.waitUntil(
              caches.open('v1')
                  .then(c => c.put(
                      '/sw_cached_script.js',
                      new Response("postMessage('from-sw-cache');", {
                        headers: {'Content-Type': 'text/javascript'}
                      })))
                  .then(() => self.skipWaiting()));
        });
        self.addEventListener('activate', event => {
          event.waitUntil(self.clients.claim());
        });
        self.addEventListener('fetch', event => {
          if (event.request.url.endsWith('/sw_cached_script.js')) {
            handledScriptCount++;
          }
          const params = new URL(event.request.url).searchParams;
          if (params.has('redirect_to')) {
            event.respondWith(Response.redirect(params.get('redirect_to'), 302));
            return;
          }
          event.respondWith(
              caches.match(event.request).then(r => r || fetch(event.request)));
        });
        self.addEventListener('message', event => {
          event.source.postMessage(handledScriptCount);
        });
      )");
      return response;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(
    SandboxedDedicatedWorkerServiceWorkerBrowserTest,
    SandboxedFrameDedicatedWorkerDoesNotReceiveServiceWorkerCapabilities) {
  const GURL top_url = ssl_server()->GetURL("b.test", "/title1.html");
  const GURL frame_url = ssl_server()->GetURL("a.test", "/title1.html");
  const GURL cached_worker_url =
      ssl_server()->GetURL("a.test", "/sw_cached_script.js");
  const url::Origin target_origin = url::Origin::Create(frame_url);

  ASSERT_TRUE(NavigateToURL(shell(), top_url));
  RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  // 1. Positive control: `<iframe sandbox="allow-scripts allow-same-origin">`
  // under `b.test` retains `a.test` origin access (in the `{a.test, b.test,
  // kCrossSite}` partition), registers a cache-backed Service Worker, and
  // receives Service Worker container, controller, and CacheStorage side-data
  // writer capabilities for a dedicated worker load.
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    new Promise(resolve => {
      const f = document.createElement('iframe');
      f.id = 'same_origin_sandboxed';
      f.sandbox = 'allow-scripts allow-same-origin';
      f.src = $1;
      f.onload = resolve;
      document.body.appendChild(f);
    })
  )",
                                         frame_url)));
  RenderFrameHostImpl* same_origin_rfh =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_rfh, 0));
  ASSERT_TRUE(same_origin_rfh);
  ASSERT_FALSE(same_origin_rfh->GetStorageKey().origin().opaque());

  ASSERT_EQ("from-sw-cache", EvalJs(same_origin_rfh, R"(
    (async () => {
      await navigator.serviceWorker.register('/sw_test_worker.js', {scope: '/'});
      await navigator.serviceWorker.ready;
      return await new Promise((resolve, reject) => {
        const w = new Worker('/sw_cached_script.js');
        w.onmessage = e => resolve(e.data);
        w.onerror = () => reject(new Error('worker load failed'));
      });
    })()
  )"));

  {
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    TestDedicatedWorkerHostFactoryClient client;
    StartDedicatedWorkerScriptLoadForFrame(same_origin_rfh, cached_worker_url,
                                           factory, client);
    EXPECT_TRUE(client.script_load_started());
    EXPECT_FALSE(client.script_load_start_failed());
    EXPECT_TRUE(client.service_worker_container_info());
    EXPECT_TRUE(client.controller_info());
    ASSERT_TRUE(client.main_script_load_params());
    EXPECT_TRUE(client.main_script_load_params()
                    ->response_head->was_fetched_via_service_worker);
    EXPECT_TRUE(client.main_script_load_params()
                    ->response_head->cache_storage_side_data_writer.is_valid());
  }

  // 2. Boundary check: `<iframe sandbox="allow-scripts">` commits with an
  // opaque StorageKey whose `.WithOrigin(a.test)` matches `same_origin_rfh`'s
  // partitioned StorageKey (and in an isolated sandboxed process when
  // `AreIsolatedSandboxedIframesEnabled()` is true).
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    new Promise(resolve => {
      const f = document.createElement('iframe');
      f.id = 'sandboxed_frame';
      f.sandbox = 'allow-scripts';
      f.src = $1;
      f.onload = resolve;
      document.body.appendChild(f);
    })
  )",
                                         frame_url)));
  RenderFrameHostImpl* sandboxed_rfh =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_rfh, 1));
  ASSERT_TRUE(sandboxed_rfh);
  ASSERT_TRUE(sandboxed_rfh->GetStorageKey().origin().opaque());
  ASSERT_EQ(same_origin_rfh->GetStorageKey(),
            sandboxed_rfh->GetStorageKey().WithOrigin(target_origin));
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
    EXPECT_TRUE(sandboxed_rfh->GetProcess()->GetProcessLock().is_sandboxed());
    EXPECT_FALSE(
        ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
            sandboxed_rfh->GetProcess()->GetID().value(), target_origin));
  }

  // Requesting the cached HTTPS worker URL from `sandboxed_rfh`'s
  // DedicatedWorkerHostFactory must not dispatch a FetchEvent to `a.test`'s
  // Service Worker and must not deliver Service Worker capabilities.
  {
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory;
    TestDedicatedWorkerHostFactoryClient client;
    StartDedicatedWorkerScriptLoadForFrame(sandboxed_rfh, cached_worker_url,
                                           factory, client);
    EXPECT_TRUE(client.script_load_start_failed());
    EXPECT_FALSE(client.script_load_started());
    EXPECT_FALSE(client.service_worker_container_info());
    EXPECT_FALSE(client.controller_info());
    EXPECT_FALSE(client.main_script_load_params());
  }

  // 3. Cross-origin & redirect checks: a cross-origin request from `main_rfh`
  // (`b.test`) for `a.test`'s cached worker URL is rejected without invoking
  // `a.test`'s Service Worker, and a Service Worker cross-origin redirect
  // resets the unclaimed client cleanly.
  {
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> cross_origin_factory;
    TestDedicatedWorkerHostFactoryClient cross_origin_client;
    StartDedicatedWorkerScriptLoadForFrame(
        main_rfh, cached_worker_url, cross_origin_factory, cross_origin_client);
    EXPECT_TRUE(cross_origin_client.script_load_start_failed());
    EXPECT_FALSE(cross_origin_client.script_load_started());
  }

  // A same-origin worker script request from `main_rfh` (`b.test`) that
  // 302-redirects cross-origin to `a.test`'s cached worker URL must also clear
  // its ServiceWorkerClient and never invoke `a.test`'s Service Worker.
  {
    const GURL redirect_to_a_url = ssl_server()->GetURL(
        "b.test", "/server-redirect?" + cached_worker_url.spec());
    mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> redirect_factory;
    TestDedicatedWorkerHostFactoryClient redirect_client;
    StartDedicatedWorkerScriptLoadForFrame(main_rfh, redirect_to_a_url,
                                           redirect_factory, redirect_client);
    EXPECT_TRUE(redirect_client.script_load_start_failed());
    EXPECT_FALSE(redirect_client.script_load_started());
    EXPECT_FALSE(redirect_client.service_worker_container_info());
    EXPECT_FALSE(redirect_client.controller_info());
  }

  EXPECT_EQ("redirect-error", EvalJs(same_origin_rfh, JsReplace(R"(
    new Promise(resolve => {
      const w = new Worker('/redirect_worker.js?redirect_to=' +
                           encodeURIComponent($1));
      w.onmessage = () => resolve('unexpected-message');
      w.onerror = () => resolve('redirect-error');
    })
  )",
                                                                top_url)));

  // Confirm the active Service Worker only handled the two legitimate
  // `/sw_cached_script.js` loads from step 1, and never received a fetch event
  // from the sandboxed or cross-origin worker requests.
  EXPECT_EQ(2, EvalJs(same_origin_rfh, R"(
    (async () => {
      const reg = await navigator.serviceWorker.ready;
      return await new Promise(resolve => {
        navigator.serviceWorker.addEventListener(
            'message', e => resolve(e.data), {once: true});
        reg.active.postMessage('count');
      });
    })()
  )"));

  // 4. Non-regression check: local-scheme (`blob:` and `data:`) dedicated
  // workers inside `<iframe sandbox="allow-scripts">` continue to run normally.
  EXPECT_EQ("blob-ok", EvalJs(sandboxed_rfh, R"(
    new Promise((resolve, reject) => {
      const blob = new Blob(["postMessage('blob-ok');"],
                            {type: 'text/javascript'});
      const w = new Worker(URL.createObjectURL(blob));
      w.onmessage = e => resolve(e.data);
      w.onerror = () => reject(new Error('blob worker failed'));
    })
  )"));
  EXPECT_EQ("data-ok", EvalJs(sandboxed_rfh, R"(
    new Promise((resolve, reject) => {
      const w = new Worker("data:text/javascript,postMessage('data-ok');");
      w.onmessage = e => resolve(e.data);
      w.onerror = () => reject(new Error('data worker failed'));
    })
  )"));
}

}  // namespace content
