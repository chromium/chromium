// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "media/media_buildflags.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_handle.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-test-utils.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

// V8ScriptRunner::setCacheTimeStamp() stores 16 byte data (marker + tag +
// timestamp).
const int kV8CacheTimeStampDataSize =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(double);

void RunOnCoreThread(base::OnceClosure closure) {
  base::RunLoop run_loop;
  RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                        base::BindLambdaForTesting([&]() {
                          std::move(closure).Run();
                          run_loop.Quit();
                        }));
  run_loop.Run();
}

// Runs the given |callback|, which itself takes a callback, on the core thread.
// Returns when |callback| invokes its callback.
void RunOnCoreThread(
    base::OnceCallback<void(base::OnceClosure continuation)> callback) {
  base::RunLoop run_loop;
  base::OnceClosure continuation =
      base::BindOnce([](base::OnceClosure quit) { std::move(quit).Run(); },
                     run_loop.QuitClosure());
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(std::move(callback), std::move(continuation)));
  run_loop.Run();
}

void ExpectResultAndRun(bool expected,
                        base::RepeatingClosure continuation,
                        bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

class WorkerStateObserver
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<WorkerStateObserver> {
 public:
  WorkerStateObserver(ServiceWorkerContextWrapper* context,
                      ServiceWorkerVersion::Status target)
      : context_(context), target_(target) {}
  void Init() {
    RunOnCoreThread(
        base::BindOnce(&WorkerStateObserver::InitOnCoreThread, this));
  }
  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == target_) {
      context_->RemoveObserver(this);
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                            base::BindOnce(&WorkerStateObserver::Quit, this));
    }
  }
  void Wait() { run_loop_.Run(); }

  int64_t registration_id() { return registration_id_; }
  int64_t version_id() { return version_id_; }

 protected:
  friend class base::RefCountedThreadSafe<WorkerStateObserver>;
  ~WorkerStateObserver() override = default;

 private:
  void InitOnCoreThread() { context_->AddObserver(this); }
  void Quit() { run_loop_.Quit(); }

  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  base::RunLoop run_loop_;
  ServiceWorkerContextWrapper* context_;
  const ServiceWorkerVersion::Status target_;
  DISALLOW_COPY_AND_ASSIGN(WorkerStateObserver);
};

std::unique_ptr<net::test_server::HttpResponse> VerifySaveDataHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/service_worker/generated_sw.js")
    return nullptr;
  auto it = request.headers.find("Save-Data");
  EXPECT_NE(request.headers.end(), it);
  EXPECT_EQ("on", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse>
VerifySaveDataNotInAccessControlRequestHeader(
    const net::test_server::HttpRequest& request) {
  if (request.method == net::test_server::METHOD_OPTIONS) {
    // 'Save-Data' is not added to the CORS preflight request.
    auto it = request.headers.find("Save-Data");
    EXPECT_EQ(request.headers.end(), it);
  } else {
    // 'Save-Data' is added to the actual request, as expected.
    auto it = request.headers.find("Save-Data");
    EXPECT_NE(request.headers.end(), it);
    EXPECT_EQ("on", it->second);
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  if (request.method == net::test_server::METHOD_OPTIONS) {
    // Access-Control-Request-Headers should contain 'X-Custom-Header' and not
    // contain 'Save-Data'.
    auto acrh_iter = request.headers.find("Access-Control-Request-Headers");
    EXPECT_NE(request.headers.end(), acrh_iter);
    EXPECT_NE(std::string::npos, acrh_iter->second.find("x-custom-header"));
    EXPECT_EQ(std::string::npos, acrh_iter->second.find("save-data"));
    http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                   acrh_iter->second);
    http_response->AddCustomHeader("Access-Control-Allow-Methods", "GET");
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else {
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    http_response->set_content("PASS");
  }
  return std::move(http_response);
}

void CountScriptResources(
    ServiceWorkerContextWrapper* wrapper,
    const GURL& scope,
    int* num_resources) {
  *num_resources = -1;

  std::vector<ServiceWorkerRegistrationInfo> infos =
      wrapper->GetAllLiveRegistrationInfo();
  if (infos.empty())
    return;

  int version_id;
  size_t index = infos.size() - 1;
  if (infos[index].installing_version.version_id !=
      blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[index].installing_version.version_id;
  else if (infos[index].waiting_version.version_id !=
           blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[1].waiting_version.version_id;
  else if (infos[index].active_version.version_id !=
           blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[index].active_version.version_id;
  else
    return;

  ServiceWorkerVersion* version = wrapper->GetLiveVersion(version_id);
  *num_resources = static_cast<int>(version->script_cache_map()->size());
}

void StoreString(std::string* result,
                 base::OnceClosure callback,
                 base::Value value) {
  value.GetAsString(result);
  std::move(callback).Run();
}

int GetInt(const base::DictionaryValue& dict, base::StringPiece path) {
  int out = 0;
  EXPECT_TRUE(dict.GetInteger(path, &out));
  return out;
}

std::string GetString(const base::DictionaryValue& dict,
                      base::StringPiece path) {
  std::string out;
  EXPECT_TRUE(dict.GetString(path, &out));
  return out;
}

bool GetBoolean(const base::DictionaryValue& dict, base::StringPiece path) {
  bool out = false;
  EXPECT_TRUE(dict.GetBoolean(path, &out));
  return out;
}

bool CheckHeader(const base::DictionaryValue& dict,
                 base::StringPiece header_name,
                 base::StringPiece header_value) {
  const base::ListValue* headers = nullptr;
  EXPECT_TRUE(dict.GetList("headers", &headers));
  for (size_t i = 0; i < headers->GetSize(); ++i) {
    const base::ListValue* name_value_pair = nullptr;
    EXPECT_TRUE(headers->GetList(i, &name_value_pair));
    EXPECT_EQ(2u, name_value_pair->GetSize());
    std::string name;
    EXPECT_TRUE(name_value_pair->GetString(0, &name));
    std::string value;
    EXPECT_TRUE(name_value_pair->GetString(1, &value));
    if (name == header_name && value == header_value)
      return true;
  }
  return false;
}

bool HasHeader(const base::DictionaryValue& dict,
               base::StringPiece header_name) {
  const base::ListValue* headers = nullptr;
  EXPECT_TRUE(dict.GetList("headers", &headers));
  for (size_t i = 0; i < headers->GetSize(); ++i) {
    const base::ListValue* name_value_pair = nullptr;
    EXPECT_TRUE(headers->GetList(i, &name_value_pair));
    EXPECT_EQ(2u, name_value_pair->GetSize());
    std::string name;
    EXPECT_TRUE(name_value_pair->GetString(0, &name));
    if (name == header_name)
      return true;
  }
  return false;
}

const char kNavigationPreloadNetworkError[] =
    "NetworkError: The service worker navigation preload request failed with "
    "a network error.";

void CheckPageIsMarkedSecure(
    Shell* shell,
    scoped_refptr<net::X509Certificate> expected_certificate) {
  NavigationEntry* entry =
      shell->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_TRUE(expected_certificate->EqualsExcludingChain(
      entry->GetSSL().certificate.get()));
  EXPECT_FALSE(net::IsCertStatusError(entry->GetSSL().cert_status));
}

}  // namespace

class ServiceWorkerBrowserTest : public ContentBrowserTest {
 protected:
  using self = ServiceWorkerBrowserTest;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());

    RunOnCoreThread(
        base::BindOnce(&self::SetUpOnCoreThread, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    RunOnCoreThread(
        base::BindOnce(&self::TearDownOnCoreThread, base::Unretained(this)));
    wrapper_ = nullptr;
  }

  // Starts the test server and navigates the renderer to an empty page. Call
  // this after adding all request handlers to the test server. Adding handlers
  // after the test server has started is not allowed.
  void StartServerAndNavigateToSetup() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    embedded_test_server()->StartAcceptingConnections();

    // Navigate to the page to set up a renderer page (where we can embed
    // a worker).
    NavigateToURLBlockUntilNavigationsComplete(
        shell(), embedded_test_server()->GetURL("/service_worker/empty.html"),
        1);
  }

  virtual void SetUpOnCoreThread() {}
  virtual void TearDownOnCoreThread() {}

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Listens to console messages on ServiceWorkerContextWrapper.
class ConsoleMessageContextObserver
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<ConsoleMessageContextObserver> {
 public:
  explicit ConsoleMessageContextObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}
  void Init() { context_->AddObserver(this); }

  // ServiceWorkerContextCoreObserver overrides.
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const ConsoleMessage& console_message) override {
    messages_.push_back(console_message.message);
    if (messages_.size() == expected_message_count_) {
      run_loop_.Quit();
    }
  }

  void WaitForConsoleMessages(size_t expected_message_count) {
    if (messages_.size() >= expected_message_count) {
      context_->RemoveObserver(this);
      return;
    }

    expected_message_count_ = expected_message_count;
    run_loop_.Run();
    ASSERT_EQ(messages_.size(), expected_message_count);
    context_->RemoveObserver(this);
  }

  const std::vector<base::string16>& messages() const { return messages_; }

 private:
  friend class base::RefCountedThreadSafe<ConsoleMessageContextObserver>;
  ~ConsoleMessageContextObserver() override {}

  std::vector<base::string16> messages_;
  size_t expected_message_count_ = 0;
  base::RunLoop run_loop_;
  ServiceWorkerContextWrapper* context_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleMessageContextObserver);
};

class MockContentBrowserClient : public TestContentBrowserClient {
 public:
  MockContentBrowserClient()
      : TestContentBrowserClient(), data_saver_enabled_(false) {}

  ~MockContentBrowserClient() override {}

  void set_data_saver_enabled(bool enabled) { data_saver_enabled_ = enabled; }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override {
    return data_saver_enabled_;
  }

  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  bool data_saver_enabled_;
};

// An observer that waits for the service worker to be running.
class WorkerRunningStatusObserver : public ServiceWorkerContextObserver {
 public:
  explicit WorkerRunningStatusObserver(ServiceWorkerContext* context)
      : scoped_context_observer_(this) {
    scoped_context_observer_.Add(context);
  }

  ~WorkerRunningStatusObserver() override = default;

  int64_t version_id() { return version_id_; }

  void WaitUntilRunning() {
    if (version_id_ == blink::mojom::kInvalidServiceWorkerVersionId)
      run_loop_.Run();
  }

  void OnVersionStartedRunning(
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) override {
    version_id_ = version_id;

    if (run_loop_.running())
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  ScopedObserver<ServiceWorkerContext, ServiceWorkerContextObserver>
      scoped_context_observer_;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  DISALLOW_COPY_AND_ASSIGN(WorkerRunningStatusObserver);
};

// Tests the |top_frame_origin| and |request_initiator| on the main resource and
// subresource requests from service workers, in order to ensure proper handling
// by the SplitCache. See https://crbug.com/918868.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, RequestOrigin) {
  embedded_test_server()->StartAcceptingConnections();

  // To make things tricky about |top_frame_origin|, this test navigates to a
  // page on |embedded_test_server()| which has a cross-origin iframe that
  // registers the service worker.
  net::EmbeddedTestServer cross_origin_server;
  cross_origin_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server.Start());

  // There are three requests to test:
  // 1) The request for the worker itself ("request_origin_worker.js").
  // 2) importScripts("empty.js") from the service worker.
  // 3) fetch("empty.html") from the service worker.
  std::set<GURL> expected_request_urls = {
      cross_origin_server.GetURL("/service_worker/request_origin_worker.js"),
      cross_origin_server.GetURL("/service_worker/empty.js"),
      cross_origin_server.GetURL("/service_worker/empty.html")};

  base::RunLoop request_origin_expectation_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it != expected_request_urls.end()) {
          EXPECT_TRUE(params->url_request.originated_from_service_worker);
          EXPECT_FALSE(
              params->url_request.trusted_params.has_value() &&
              !params->url_request.trusted_params->isolation_info.IsEmpty());
          EXPECT_TRUE(params->url_request.request_initiator.has_value());
          EXPECT_EQ(params->url_request.request_initiator->GetURL(),
                    cross_origin_server.base_url());
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty())
          request_origin_expectation_waiter.Quit();
        return false;
      }));

  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL(
          "/service_worker/one_subframe.html?subframe_url=" +
          cross_origin_server
              .GetURL("/service_worker/create_service_worker.html")
              .spec()),
      1);
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents(),
      base::BindRepeating(&FrameMatchesName, "subframe_name"));
  DCHECK(subframe_rfh);

  EXPECT_EQ("DONE",
            EvalJs(subframe_rfh, "register('request_origin_worker.js');"));

  request_origin_expectation_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, FetchPageWithSaveData) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/add_save_data_to_title.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->OnWebPreferencesChanged();
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  const base::string16 title1 = base::ASCIIToUTF16("save-data=on");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests that when data saver is enabled and a cross-origin fetch by a webpage
// is intercepted by a serviceworker, and the serviceworker does a fetch, the
// preflight request does not have save-data in Access-Control-Request-Headers.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, CrossOriginFetchWithSaveData) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/fetch_cross_origin.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  net::EmbeddedTestServer cross_origin_server;
  cross_origin_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  cross_origin_server.RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataNotInAccessControlRequestHeader));
  ASSERT_TRUE(cross_origin_server.Start());

  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->OnWebPreferencesChanged();
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("PASS");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?%s", kPageUrl,
                   cross_origin_server.GetURL("/cross_origin_request.html")
                       .spec()
                       .c_str()))));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       FetchPageWithSaveDataPassThroughOnFetch) {
  const char kPageUrl[] = "/service_worker/pass_through_fetch.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->OnWebPreferencesChanged();
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();

  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();

  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(kPageUrl), 1);

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, Reload) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/reload.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_reload.js";
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  const base::string16 title1 = base::ASCIIToUTF16("reload=false");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  const base::string16 title2 = base::ASCIIToUTF16("reload=true");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

// Test when the renderer requests termination because the service worker is
// idle, and the browser ignores the request because DevTools is attached. The
// renderer should continue processing events on the service worker instead of
// waiting for termination or an event from the browser. Regression test for
// https://crbug.com/878667.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, IdleTimerWithDevTools) {
  StartServerAndNavigateToSetup();

  // Register a service worker.
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  const GURL scope =
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html");
  const GURL worker_url = embedded_test_server()->GetURL(
      "/service_worker/fetch_event_respond_with_fetch.js");

  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone);
  public_context()->RegisterServiceWorker(
      worker_url, options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  // Navigate to a new page and request a sub resource. This should succeed
  // normally.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html")));
  EXPECT_EQ("Echo", EvalJs(shell(), "fetch_from_page('/echo');"));

  // Simulate to attach DevTools.
  base::RunLoop loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          [](base::OnceClosure done, ServiceWorkerContextWrapper* wrapper,
             int64_t version_id) {
            ASSERT_TRUE(BrowserThread::CurrentlyOn(
                ServiceWorkerContext::GetCoreThreadId()));
            scoped_refptr<ServiceWorkerVersion> version =
                wrapper->GetLiveVersion(version_id);
            version->SetDevToolsAttached(true);

            // Set the idle timer delay to zero for making the service worker
            // idle immediately. This may cause infinite loop of IPCs when no
            // event was queued in the renderer because a callback of
            // RequestTermination() is called and it triggers another
            // RequestTermination() immediately. However, this is unusual
            // situation happening only in testing so it's acceptable.
            // In production code, WakeUp() as the result of
            // RequestTermination() doesn't happen when the idle timer delay is
            // set to zero. Instead, activating a new worker will be triggered.
            version->endpoint()->SetIdleDelay(base::TimeDelta::FromSeconds(0));
            std::move(done).Run();
          },
          loop.QuitClosure(), base::Unretained(wrapper()),
          observer->version_id()));
  loop.Run();

  // Trigger another sub resource request. The sub resource request will
  // directly go to the worker thread and be queued because the worker is
  // idle. However, the browser process notifies the renderer to let it continue
  // to work because DevTools is attached, and it'll result in running all
  // queued events.
  EXPECT_EQ("Echo", EvalJs(shell(), "fetch_from_page('/echo');"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPSServiceWorkerIsMarkedAsSecure) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/in-scope";
  const char kWorkerUrl[] = "/service_worker/worker_script";
  const char kWorkerScript[] = R"(
      self.addEventListener('fetch', e => {
        e.respondWith(new Response('<title>Title</title>', {
          headers: {'Content-Type': 'text/html'}
        }));
      });
      // Version: %d)";

  // Register a handler which serves different script on each request. The
  // service worker returns a page titled by "Title" via Blob.
  int service_worker_served_count = 0;
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kWorkerUrl)
          return nullptr;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/javascript");
        response->set_content(
            base::StringPrintf(kWorkerScript, ++service_worker_served_count));
        return response;
      }));
  ASSERT_TRUE(https_server.Start());

  // 1st attempt: install a service worker and open the controlled page.
  {
    // Register a service worker which controls |kPageUrl|.
    auto observer = base::MakeRefCounted<WorkerStateObserver>(
        wrapper(), ServiceWorkerVersion::ACTIVATED);
    observer->Init();
    blink::mojom::ServiceWorkerRegistrationOptions options(
        https_server.GetURL(kPageUrl), blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        https_server.GetURL(kWorkerUrl), options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
    EXPECT_EQ(1, service_worker_served_count);

    // Wait until the page is appropriately served by the service worker.
    const base::string16 title = base::ASCIIToUTF16("Title");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), https_server.GetURL(kPageUrl)));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

    // The page should be marked as secure.
    CheckPageIsMarkedSecure(shell(), https_server.GetCertificate());
  }

  // Navigate away from the page so that the worker has no controllee.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2nd attempt: update the service worker and open the controlled page again.
  {
    // Update the service worker.
    auto observer = base::MakeRefCounted<WorkerStateObserver>(
        wrapper(), ServiceWorkerVersion::ACTIVATED);
    observer->Init();
    wrapper()->UpdateRegistration(https_server.GetURL(kPageUrl));
    observer->Wait();

    // Wait until the page is appropriately served by the service worker.
    const base::string16 title = base::ASCIIToUTF16("Title");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), https_server.GetURL(kPageUrl)));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

    // The page should be marked as secure.
    CheckPageIsMarkedSecure(shell(), https_server.GetCertificate());
  }

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      https_server.GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPServiceWorkerIsNotMarkedAsSecure) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/fetch_event_blob.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("Title");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_FALSE(entry->GetSSL().certificate);

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, ImportsBustMemcache) {
  StartServerAndNavigateToSetup();
  const char kScopeUrl[] = "/service_worker/imports_bust_memcache_scope/";
  const char kPageUrl[] = "/service_worker/imports_bust_memcache.html";
  const base::string16 kOKTitle(base::ASCIIToUTF16("OK"));
  const base::string16 kFailTitle(base::ASCIIToUTF16("FAIL"));

  TitleWatcher title_watcher(shell()->web_contents(), kOKTitle);
  title_watcher.AlsoWaitForTitle(kFailTitle);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  base::string16 title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(kOKTitle, title);

  // Verify the number of resources in the implicit script cache is correct.
  const int kExpectedNumResources = 2;
  int num_resources = 0;
  RunOnCoreThread(base::BindOnce(
      &CountScriptResources, base::Unretained(wrapper()),
      embedded_test_server()->GetURL(kScopeUrl), &num_resources));
  EXPECT_EQ(kExpectedNumResources, num_resources);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, GetRunningServiceWorkerInfos) {
  StartServerAndNavigateToSetup();
  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/fetch_event.js"),
            running_info.script_url);
  EXPECT_EQ(shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
            running_info.render_process_id);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, StartWorkerWhileInstalling) {
  StartServerAndNavigateToSetup();
  const char kWorkerUrl[] = "/service_worker/while_true_in_install_worker.js";
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::INSTALLING);
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kWorkerUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  base::RunLoop run_loop;
  wrapper()->StartActiveServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl),
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorNotFound);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Make sure that a fetch event is dispatched to a stopped worker in the task
// which calls ServiceWorkerFetchDispatcher::Run().
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       DispatchFetchEventToStoppedWorkerSynchronously) {
  // Setup the server so that the test doesn't crash when tearing down.
  StartServerAndNavigateToSetup();
  // This test is meaningful only when ServiceWorkerOnUI is enabled.
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    return;

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  ASSERT_TRUE(
      BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version->running_status());

  {
    base::RunLoop loop;
    version->StopWorker(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version->running_status());
  }

  bool is_prepare_callback_called = false;
  base::RunLoop fetch_loop;
  blink::ServiceWorkerStatusCode fetch_status;
  ServiceWorkerFetchDispatcher::FetchEventResult fetch_result;
  blink::mojom::FetchAPIResponsePtr fetch_response;

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = embedded_test_server()->GetURL("/service_worker/in-scope");
  request->method = "GET";
  request->is_main_resource_load = true;
  auto dispatcher = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(request), blink::mojom::ResourceType::kMainFrame,
      /*client_id=*/base::GenerateGUID(), version,
      base::BindLambdaForTesting([&]() { is_prepare_callback_called = true; }),
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              ServiceWorkerFetchDispatcher::FetchEventResult result,
              blink::mojom::FetchAPIResponsePtr response,
              blink::mojom::ServiceWorkerStreamHandlePtr,
              blink::mojom::ServiceWorkerFetchEventTimingPtr,
              scoped_refptr<ServiceWorkerVersion>) {
            fetch_status = status;
            fetch_result = result;
            fetch_response = std::move(response);
            fetch_loop.Quit();
          }),
      /*is_offline_capability_check=*/false);

  // DispatchFetchEvent is called synchronously with dispatcher->Run() even if
  // the worker is stopped.
  dispatcher->Run();
  EXPECT_TRUE(is_prepare_callback_called);
  EXPECT_FALSE(fetch_response);

  // Check if the fetch event is handled by fetch_event.js correctly.
  fetch_loop.Run();
  ASSERT_TRUE(fetch_response);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, fetch_status);
  EXPECT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            fetch_result);
  EXPECT_EQ(301, fetch_response->status_code);
}

// Check if a fetch event can be failed without crashing if starting a service
// worker fails. This is a regression test for https://crbug.com/1106977.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       DispatchFetchEventToBrokenWorker) {
  // Setup the server so that the test doesn't crash when tearing down.
  StartServerAndNavigateToSetup();
  // This test is meaningful only when ServiceWorkerOnUI is enabled.
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    return;

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  ASSERT_TRUE(
      BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version->running_status());

  {
    base::RunLoop loop;
    version->StopWorker(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version->running_status());
  }

  // Set a non-existent resource to the version.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  resources.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      123456789, version->script_url(), 100));
  version->script_cache_map()->resource_map_.clear();
  version->script_cache_map()->SetResources(resources);

  bool is_prepare_callback_called = false;
  base::RunLoop fetch_loop;
  blink::ServiceWorkerStatusCode fetch_status;
  ServiceWorkerFetchDispatcher::FetchEventResult fetch_result;

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = embedded_test_server()->GetURL("/service_worker/in-scope");
  request->method = "GET";
  request->is_main_resource_load = true;
  auto dispatcher = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(request), blink::mojom::ResourceType::kMainFrame,
      /*client_id=*/base::GenerateGUID(), version,
      base::BindLambdaForTesting([&]() { is_prepare_callback_called = true; }),
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              ServiceWorkerFetchDispatcher::FetchEventResult result,
              blink::mojom::FetchAPIResponsePtr response,
              blink::mojom::ServiceWorkerStreamHandlePtr,
              blink::mojom::ServiceWorkerFetchEventTimingPtr,
              scoped_refptr<ServiceWorkerVersion>) {
            fetch_status = status;
            fetch_result = result;
            fetch_loop.Quit();
          }),
      /*is_offline_capability_check=*/false);

  // DispatchFetchEvent is called synchronously with dispatcher->Run() even if
  // the worker is stopped.
  dispatcher->Run();
  EXPECT_TRUE(is_prepare_callback_called);

  // Check if the fetch event fails due to error of reading the resource.
  fetch_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorDiskCache, fetch_status);
  EXPECT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
            fetch_result);

  // Make sure that no crash happens in the remaining tasks.
  base::RunLoop().RunUntilIdle();
}

class ServiceWorkerEagerCacheStorageSetupTest
    : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerEagerCacheStorageSetupTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kEagerCacheStorageSetupForServiceWorkers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/1077916.
// Update the service worker by registering a worker with different script url.
// This test makes sure the worker can handle the fetch event using CacheStorage
// API.
// TODO(crbug.com/1087869): flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ServiceWorkerEagerCacheStorageSetupTest,
                       DISABLED_UpdateOnScriptUrlChange) {
  StartServerAndNavigateToSetup();
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));

  // Register a service worker.
  EXPECT_EQ(
      "DONE",
      EvalJs(
          shell(),
          "registerWithoutAwaitingReady('fetch_event.js', './empty.html');"));
  {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_FALSE(infos.empty());
    const ServiceWorkerRunningInfo& running_info = infos.rbegin()->second;
    EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/fetch_event.js"),
              running_info.script_url);
  }

  // Update the service worker by changing the script url.
  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();
  EXPECT_EQ("DONE", EvalJs(shell(),
                           "registerWithoutAwaitingReady('fetch_event_response_"
                           "via_cache.js', './empty.html');"));

  {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_FALSE(infos.empty());
    const ServiceWorkerRunningInfo& running_info = infos.rbegin()->second;
    EXPECT_EQ(embedded_test_server()->GetURL(
                  "/service_worker/fetch_event_response_via_cache.js"),
              running_info.script_url);
  }
  observer->Wait();

  // Navigation should succeed.
  const base::string16 title =
      base::ASCIIToUTF16("ServiceWorker test - empty page");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// TODO(crbug.com/709385): ServiceWorkerNavigationPreloadTest should be
// converted to WPT.
class ServiceWorkerNavigationPreloadTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerNavigationPreloadTest;

  ~ServiceWorkerNavigationPreloadTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ServiceWorkerBrowserTest::SetUpOnMainThread();
  }

 protected:
  static const std::string kNavigationPreloadHeaderName;
  static const std::string kEnableNavigationPreloadScript;
  static const std::string kPreloadResponseTestScript;

  static bool HasNavigationPreloadHeader(
      const net::test_server::HttpRequest& request) {
    return request.headers.find(kNavigationPreloadHeaderName) !=
           request.headers.end();
  }

  static std::string GetNavigationPreloadHeader(
      const net::test_server::HttpRequest& request) {
    DCHECK(HasNavigationPreloadHeader(request));
    return request.headers.find(kNavigationPreloadHeaderName)->second;
  }

  void SetupForNavigationPreloadTest(const GURL& scope,
                                     const GURL& worker_url) {
    auto observer = base::MakeRefCounted<WorkerStateObserver>(
        wrapper(), ServiceWorkerVersion::ACTIVATED);
    observer->Init();

    blink::mojom::ServiceWorkerRegistrationOptions options(
        scope, blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        worker_url, options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
  }

  std::string LoadNavigationPreloadTestPage(const GURL& page_url,
                                            const GURL& worker_url,
                                            const char* expected_result) {
    RegisterMonitorRequestHandler();
    StartServerAndNavigateToSetup();
    SetupForNavigationPreloadTest(page_url, worker_url);

    const base::string16 title = base::ASCIIToUTF16("PASS");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("ERROR"));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("REJECTED"));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("RESOLVED"));
    EXPECT_TRUE(NavigateToURL(shell(), page_url));
    EXPECT_EQ(base::ASCIIToUTF16(expected_result),
              title_watcher.WaitAndGetTitle());
    return GetTextContent();
  }

  void RegisterMonitorRequestHandler() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &self::MonitorRequestHandler, base::Unretained(this)));
  }

  void RegisterStaticFile(const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&self::StaticRequestHandler, base::Unretained(this),
                            relative_url, content, content_type));
  }

  void RegisterCustomResponse(const std::string& relative_url,
                              const std::string& response) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&self::CustomRequestHandler, base::Unretained(this),
                            relative_url, response));
  }

  void RegisterKeepSearchRedirect(const std::string& relative_url,
                                  const std::string& redirect_location) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &self::KeepSearchRedirectHandler, base::Unretained(this), relative_url,
        redirect_location));
  }

  int GetRequestCount(const std::string& relative_url) const {
    const auto& it = request_log_.find(relative_url);
    if (it == request_log_.end())
      return 0;
    return it->second.size();
  }

  std::string GetTextContent() {
    base::RunLoop run_loop;
    std::string text_content;
    shell()->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("document.body.textContent;"),
        base::BindOnce(&StoreString, &text_content, run_loop.QuitClosure()));
    run_loop.Run();
    return text_content;
  }

  std::map<std::string, std::vector<net::test_server::HttpRequest>>
      request_log_;

 private:
  class CustomResponse : public net::test_server::HttpResponse {
   public:
    explicit CustomResponse(const std::string& response)
        : response_(response) {}
    ~CustomResponse() override {}

    void SendResponse(const net::test_server::SendBytesCallback& send,
                      net::test_server::SendCompleteCallback done) override {
      send.Run(response_, std::move(done));
    }

   private:
    const std::string response_;

    DISALLOW_COPY_AND_ASSIGN(CustomResponse);
  };

  std::unique_ptr<net::test_server::HttpResponse> StaticRequestHandler(
      const std::string& relative_url,
      const std::string& content,
      const std::string& content_type,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return std::unique_ptr<net::test_server::HttpResponse>();
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        std::make_unique<net::test_server::BasicHttpResponse>());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(content);
    http_response->set_content_type(content_type);
    return std::move(http_response);
  }

  std::unique_ptr<net::test_server::HttpResponse> CustomRequestHandler(
      const std::string& relative_url,
      const std::string& response,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return std::unique_ptr<net::test_server::HttpResponse>();
    return std::make_unique<CustomResponse>(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> KeepSearchRedirectHandler(
      const std::string& relative_url,
      const std::string& redirect_location,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return std::unique_ptr<net::test_server::HttpResponse>();
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());
    response->set_code(net::HTTP_PERMANENT_REDIRECT);
    response->AddCustomHeader(
        "Location",
        query_position == std::string::npos
            ? redirect_location
            : redirect_location + request.relative_url.substr(query_position));
    return std::move(response);
  }

  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    request_log_[request.relative_url].push_back(request);
  }
};

const std::string
    ServiceWorkerNavigationPreloadTest::kNavigationPreloadHeaderName(
        "Service-Worker-Navigation-Preload");

const std::string
    ServiceWorkerNavigationPreloadTest::kEnableNavigationPreloadScript(
        "self.addEventListener('activate', event => {\n"
        "    event.waitUntil(self.registration.navigationPreload.enable());\n"
        "  });\n");

const std::string
    ServiceWorkerNavigationPreloadTest::kPreloadResponseTestScript =
        "var preload_resolve;\n"
        "var preload_promise = new Promise(r => { preload_resolve = r; });\n"
        "self.addEventListener('fetch', event => {\n"
        "    event.waitUntil(event.preloadResponse.then(\n"
        "        r => {\n"
        "          if (!r) {\n"
        "            preload_resolve(\n"
        "                {result: 'RESOLVED', \n"
        "                 info: 'Resolved with ' + r + '.'});\n"
        "            return;\n"
        "          }\n"
        "          var info = {};\n"
        "          info.type = r.type;\n"
        "          info.url = r.url;\n"
        "          info.status = r.status;\n"
        "          info.ok = r.ok;\n"
        "          info.statusText = r.statusText;\n"
        "          info.headers = [];\n"
        "          r.headers.forEach(\n"
        "              (v, n) => { info.headers.push([n,v]); });\n"
        "          preload_resolve({result: 'RESOLVED',\n"
        "                           info: JSON.stringify(info)}); },\n"
        "        e => { preload_resolve({result: 'REJECTED',\n"
        "                                info: e.toString()}); }));\n"
        "    event.respondWith(\n"
        "        new Response(\n"
        "            '<title>WAITING</title><script>\\n' +\n"
        "            'navigator.serviceWorker.onmessage = e => {\\n' +\n"
        "            '    var div = document.createElement(\\'div\\');\\n' +\n"
        "            '    div.appendChild(' +\n"
        "            '        document.createTextNode(e.data.info));\\n' +\n"
        "            '    document.body.appendChild(div);\\n' +\n"
        "            '    document.title = e.data.result;\\n' +\n"
        "            '  };\\n' +\n"
        "            'navigator.serviceWorker.controller.postMessage(\\n' +\n"
        "            '    null);\\n' +\n"
        "            '</script>',"
        "            {headers: [['content-type', 'text/html']]}));\n"
        "  });\n"
        "self.addEventListener('message', event => {\n"
        "    event.waitUntil(\n"
        "        preload_promise.then(\n"
        "            result => event.source.postMessage(result)));\n"
        "  });";

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NetworkFallback) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript = kEnableNavigationPreloadScript +
                              "self.addEventListener('fetch', event => {\n"
                              "    // Do nothing.\n"
                              "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request can be sent one, two, or three times.
  // - A navigation preload request may be sent. But it is possible that the
  //   navigation preload request is canceled before reaching the server.
  // - A fallback request must be sent since respondWith wasn't used.
  // - A second fallback request can be sent because the HttpCache may get
  //   confused when there are two concurrent requests (navigation preload and
  //   fallback) and one of them is cancelled (navigation preload). It restarts
  //   the ongoing request, possibly triggering another network request (see
  //   https://crbug.com/876911).
  const int request_count = GetRequestCount(kPageUrl);
  EXPECT_TRUE(request_count == 1 || request_count == 2 || request_count == 3)
      << request_count;

  // There should be at least one fallback request.
  int fallback_count = 0;
  const auto& requests = request_log_[kPageUrl];
  for (int i = 0; i < request_count; i++) {
    if (!HasNavigationPreloadHeader(requests[i]))
      fallback_count++;
  }
  EXPECT_GT(fallback_count, 0);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, SetHeaderValue) {
  const std::string kPageUrl = "/service_worker/navigation_preload.html";
  const std::string kWorkerUrl = "/service_worker/navigation_preload.js";
  const std::string kPage = "<title>FROM_SERVER</title>";
  const std::string kScript =
      "function createResponse(title, body) {\n"
      "  return new Response('<title>' + title + '</title>' + body,\n"
      "                      {headers: [['content-type', 'text/html']]})\n"
      "}\n"
      "self.addEventListener('fetch', event => {\n"
      "    if (event.request.url.indexOf('?enable') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.enable()\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('ENABLED', res)));\n"
      "    } else if (event.request.url.indexOf('?change') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.setHeaderValue('Hello')\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('CHANGED', res)));\n"
      "    } else if (event.request.url.indexOf('?disable') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.disable()\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('DISABLED', res)));\n"
      "    } else if (event.request.url.indexOf('?test') != -1) {\n"
      "      event.respondWith(event.preloadResponse.then(res =>\n"
      "          createResponse('TEST', res)));\n"
      "    }\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(page_url, worker_url);

  const std::string kPageUrl1 = kPageUrl + "?enable";
  const base::string16 title1 = base::ASCIIToUTF16("ENABLED");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  title_watcher1.AlsoWaitForTitle(base::ASCIIToUTF16("FROM_SERVER"));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl1)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  // When the navigation started, the navigation preload was not enabled yet.
  EXPECT_EQ("undefined", GetTextContent());
  ASSERT_EQ(0, GetRequestCount(kPageUrl1));

  const std::string kPageUrl2 = kPageUrl + "?change";
  const base::string16 title2 = base::ASCIIToUTF16("CHANGED");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  title_watcher2.AlsoWaitForTitle(base::ASCIIToUTF16("FROM_SERVER"));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl2)));
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());
  // When the navigation started, the navigation preload was enabled, but the
  // header was not changed yet.
  EXPECT_EQ("[object Response]", GetTextContent());
  ASSERT_EQ(1, GetRequestCount(kPageUrl2));
  ASSERT_TRUE(HasNavigationPreloadHeader(request_log_[kPageUrl2][0]));
  EXPECT_EQ("true", GetNavigationPreloadHeader(request_log_[kPageUrl2][0]));

  const std::string kPageUrl3 = kPageUrl + "?disable";
  const base::string16 title3 = base::ASCIIToUTF16("DISABLED");
  TitleWatcher title_watcher3(shell()->web_contents(), title3);
  title_watcher3.AlsoWaitForTitle(base::ASCIIToUTF16("FROM_SERVER"));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl3)));
  EXPECT_EQ(title3, title_watcher3.WaitAndGetTitle());
  // When the navigation started, the navigation preload was not disabled yet.
  EXPECT_EQ("[object Response]", GetTextContent());
  ASSERT_EQ(1, GetRequestCount(kPageUrl3));
  ASSERT_TRUE(HasNavigationPreloadHeader(request_log_[kPageUrl3][0]));
  EXPECT_EQ("Hello", GetNavigationPreloadHeader(request_log_[kPageUrl3][0]));

  const std::string kPageUrl4 = kPageUrl + "?test";
  const base::string16 title4 = base::ASCIIToUTF16("TEST");
  TitleWatcher title_watcher4(shell()->web_contents(), title4);
  title_watcher4.AlsoWaitForTitle(base::ASCIIToUTF16("FROM_SERVER"));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl4)));
  EXPECT_EQ(title4, title_watcher4.WaitAndGetTitle());
  // When the navigation started, the navigation preload must be disabled.
  EXPECT_EQ("undefined", GetTextContent());
  ASSERT_EQ(0, GetRequestCount(kPageUrl4));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       RespondWithNavigationPreload) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    if (!event.preloadResponse) {\n"
      "      event.respondWith(\n"
      "          new Response('<title>ERROR</title>',"
      "                       {headers: [['content-type', 'text/html']]}));\n"
      "      return;\n"
      "    }\n"
      "    event.respondWith(event.preloadResponse);\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // the navigation preload response
  ASSERT_EQ(1, GetRequestCount(kPageUrl));
  EXPECT_EQ("true",
            request_log_[kPageUrl][0].headers[kNavigationPreloadHeaderName]);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, GetResponseText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // "Hello world".
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseCloneText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.clone())\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseReadableStream) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "function drain(reader) {\n"
      "  var data = [];\n"
      "  var decoder = new TextDecoder();\n"
      "  function nextChunk(chunk) {\n"
      "    if (chunk.done)\n"
      "      return data.join('');\n"
      "    data.push(decoder.decode(chunk.value));\n"
      "    return reader.read().then(nextChunk);\n"
      "  }\n"
      "  return reader.read().then(nextChunk);\n"
      "}\n"
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.body.getReader())\n"
      "          .then(reader => drain(reader))\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NetworkError) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(page_url, worker_url);

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  scoped_refptr<ConsoleMessageContextObserver> console_observer =
      new ConsoleMessageContextObserver(wrapper());
  console_observer->Init();

  const base::string16 title = base::ASCIIToUTF16("REJECTED");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("RESOLVED"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(kNavigationPreloadNetworkError, GetTextContent());

  console_observer->WaitForConsoleMessages(1);
  const base::string16 expected =
      base::ASCIIToUTF16("net::ERR_CONNECTION_REFUSED");
  std::vector<base::string16> messages = console_observer->messages();
  EXPECT_NE(base::string16::npos, messages[0].find(expected));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       PreloadHeadersSimple) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  std::unique_ptr<base::Value> result = base::JSONReader::ReadDeprecated(
      LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  base::DictionaryValue* dict = nullptr;
  ASSERT_TRUE(result->GetAsDictionary(&dict));
  EXPECT_EQ("basic", GetString(*dict, "type"));
  EXPECT_EQ(page_url, GURL(GetString(*dict, "url")));
  EXPECT_EQ(200, GetInt(*dict, "status"));
  EXPECT_TRUE(GetBoolean(*dict, "ok"));
  EXPECT_EQ("OK", GetString(*dict, "statusText"));
  EXPECT_TRUE(CheckHeader(*dict, "content-type", "text/html"));
  EXPECT_TRUE(CheckHeader(*dict, "content-length",
                          base::NumberToString(sizeof(kPage) - 1)));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NotEnabled) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kPreloadResponseTestScript, "text/javascript");

  EXPECT_EQ("Resolved with undefined.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must not be sent, since the worker responded with a
  // generated Response and the navigation preload isn't enabled.
  EXPECT_EQ(0, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       PreloadHeadersCustom) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPageResponse[] =
      "HTTP/1.1 201 HELLOWORLD\r\n"
      "Connection: close\r\n"
      "Content-Length: 32\r\n"
      "Content-Type: text/html\r\n"
      "Custom-Header: pen pineapple\r\n"
      "Custom-Header: apple pen\r\n"
      "Set-Cookie: COOKIE1\r\n"
      "Set-Cookie2: COOKIE2\r\n"
      "\r\n"
      "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, kPageResponse);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  std::unique_ptr<base::Value> result = base::JSONReader::ReadDeprecated(
      LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  base::DictionaryValue* dict = nullptr;
  ASSERT_TRUE(result->GetAsDictionary(&dict));
  EXPECT_EQ("basic", GetString(*dict, "type"));
  EXPECT_EQ(page_url, GURL(GetString(*dict, "url")));
  EXPECT_EQ(201, GetInt(*dict, "status"));
  EXPECT_TRUE(GetBoolean(*dict, "ok"));
  EXPECT_EQ("HELLOWORLD", GetString(*dict, "statusText"));
  EXPECT_TRUE(CheckHeader(*dict, "content-type", "text/html"));
  EXPECT_TRUE(CheckHeader(*dict, "content-length", "32"));
  EXPECT_TRUE(CheckHeader(*dict, "custom-header", "pen pineapple, apple pen"));
  // The forbidden response headers (Set-Cookie, Set-Cookie2) must be removed.
  EXPECT_FALSE(HasHeader(*dict, "set-cookie"));
  EXPECT_FALSE(HasHeader(*dict, "set-cookie2"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       InvalidRedirect_MultiLocation) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kRedirectedPageUrl1[] =
      "/service_worker/navigation_preload_redirected1.html";
  const char kRedirectedPageUrl2[] =
      "/service_worker/navigation_preload_redirected2.html";
  const char kPageResponse[] =
      "HTTP/1.1 302 Found\r\n"
      "Connection: close\r\n"
      "Location: /service_worker/navigation_preload_redirected1.html\r\n"
      "Location: /service_worker/navigation_preload_redirected2.html\r\n"
      "\r\n";
  const char kRedirectedPage[] = "<title>ERROR</title>Redirected page.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, kPageResponse);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");
  RegisterStaticFile(kRedirectedPageUrl1, kRedirectedPage, "text/html");

  scoped_refptr<ConsoleMessageContextObserver> console_observer =
      new ConsoleMessageContextObserver(wrapper());
  console_observer->Init();

  // According to the spec, multiple Location headers is not an error. So the
  // preloadResponse must be resolved with an opaque redirect response.
  // But Chrome treats multiple Location headers as an error (crbug.com/98895).
  EXPECT_EQ(kNavigationPreloadNetworkError,
            LoadNavigationPreloadTestPage(page_url, worker_url, "REJECTED"));

  console_observer->WaitForConsoleMessages(1);
  const base::string16 expected =
      base::ASCIIToUTF16("ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION");
  std::vector<base::string16> messages = console_observer->messages();
  EXPECT_NE(base::string16::npos, messages[0].find(expected));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  // The redirected request must not be sent.
  EXPECT_EQ(0, GetRequestCount(kRedirectedPageUrl1));
  EXPECT_EQ(0, GetRequestCount(kRedirectedPageUrl2));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       InvalidRedirect_InvalidLocation) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPageResponse[] =
      "HTTP/1.1 302 Found\r\n"
      "Connection: close\r\n"
      "Location: http://\r\n"
      "\r\n";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, kPageResponse);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  // TODO(horo): According to the spec, even if the location URL is invalid, the
  // preloadResponse must be resolve with an opaque redirect response. But
  // currently Chrome handles the invalid location URL in the browser process as
  // an error. crbug.com/707185
  EXPECT_EQ(kNavigationPreloadNetworkError,
            LoadNavigationPreloadTestPage(page_url, worker_url, "REJECTED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

// Tests responding with the navigation preload response when the navigation
// occurred after a redirect.
IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       RedirectAndRespondWithNavigationPreload) {
  const std::string kPageUrl = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] =
      "<title></title>\n"
      "<script>document.title = document.location.search;</script>";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    if (event.request.url.indexOf('navigation_preload.html') == -1)\n"
      "      return; // For in scope redirection.\n"
      "    event.respondWith(event.preloadResponse);\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  // Register redirects to the target URL. The service worker responds to the
  // target URL with the navigation preload response.
  const char kRedirectPageUrl[] = "/redirect";
  const char kInScopeRedirectPageUrl[] = "/service_worker/redirect";
  RegisterKeepSearchRedirect(kRedirectPageUrl, page_url.spec());
  RegisterKeepSearchRedirect(kInScopeRedirectPageUrl, page_url.spec());

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(
      embedded_test_server()->GetURL("/service_worker/"), worker_url);

  const GURL redirect_page_url =
      embedded_test_server()->GetURL(kRedirectPageUrl).Resolve("?1");
  const GURL in_scope_redirect_page_url =
      embedded_test_server()->GetURL(kInScopeRedirectPageUrl).Resolve("?2");
  const GURL cross_origin_redirect_page_url =
      embedded_test_server()->GetURL("a.com", kRedirectPageUrl).Resolve("?3");

  // Navigate to a same-origin, out of scope URL that redirects to the target
  // URL. The navigation preload request should be the single request to the
  // target URL.
  const base::string16 title1 = base::ASCIIToUTF16("?1");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  GURL expected_commit_url1(embedded_test_server()->GetURL(kPageUrl + "?1"));
  EXPECT_TRUE(NavigateToURL(shell(), redirect_page_url, expected_commit_url1));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?1"));

  // Navigate to a same-origin, in-scope URL that redirects to the target URL.
  // The navigation preload request should be the single request to the target
  // URL.
  const base::string16 title2 = base::ASCIIToUTF16("?2");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  GURL expected_commit_url2(embedded_test_server()->GetURL(kPageUrl + "?2"));
  EXPECT_TRUE(
      NavigateToURL(shell(), in_scope_redirect_page_url, expected_commit_url2));
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?2"));

  // Navigate to a cross-origin URL that redirects to the target URL. The
  // navigation preload request should be the single request to the target URL.
  const base::string16 title3 = base::ASCIIToUTF16("?3");
  TitleWatcher title_watcher3(shell()->web_contents(), title3);
  GURL expected_commit_url3(embedded_test_server()->GetURL(kPageUrl + "?3"));
  EXPECT_TRUE(NavigateToURL(shell(), cross_origin_redirect_page_url,
                            expected_commit_url3));
  EXPECT_EQ(title3, title_watcher3.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?3"));
}

class ServiceWorkerBlackBoxBrowserTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerBlackBoxBrowserTest;

  void FindRegistrationOnCoreThread(const GURL& document_url,
                                    blink::ServiceWorkerStatusCode* status,
                                    base::OnceClosure continuation) {
    wrapper()->FindReadyRegistrationForClientUrl(
        document_url,
        base::BindOnce(
            &ServiceWorkerBlackBoxBrowserTest::DidFindRegistrationOnCoreThread,
            base::Unretained(this), status, std::move(continuation)));
  }

  void DidFindRegistrationOnCoreThread(
      blink::ServiceWorkerStatusCode* out_status,
      base::OnceClosure continuation,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration) {
    *out_status = status;
    if (!registration.get())
      EXPECT_NE(blink::ServiceWorkerStatusCode::kOk, status);
    std::move(continuation).Run();
  }
};

static int CountRenderProcessHosts() {
  return RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBlackBoxBrowserTest, Registration) {
  StartServerAndNavigateToSetup();
  // Close the only window to be sure we're not re-using its RenderProcessHost.
  shell()->Close();
  EXPECT_EQ(0, CountRenderProcessHosts());

  const char kWorkerUrl[] = "/service_worker/fetch_event.js";
  const char kScope[] = "/service_worker/";

  // Unregistering nothing should return false.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        base::BindOnce(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // If we use a worker URL that doesn't exist, registration fails.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/does/not/exist"), options,
        base::BindOnce(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(0, CountRenderProcessHosts());

  // Register returns when the promise would be resolved.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kWorkerUrl), options,
        base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, CountRenderProcessHosts());

  // Registering again should succeed, although the algo still
  // might not be complete.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kWorkerUrl), options,
        base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The registration algo might not be far enough along to have
  // stored the registration data, so it may not be findable
  // at this point.

  // Unregistering something should return true.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL(kScope),
        base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_GE(1, CountRenderProcessHosts()) << "Unregistering doesn't stop the "
                                             "workers eagerly, so their RPHs "
                                             "can still be running.";

  // Should not be able to find it.
  {
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    RunOnCoreThread(base::BindOnce(
        &ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnCoreThread,
        base::Unretained(this),
        embedded_test_server()->GetURL("/service_worker/empty.html"), &status));
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound, status);
  }
}

class CacheStorageSideDataSizeChecker
    : public base::RefCountedThreadSafe<CacheStorageSideDataSizeChecker> {
 public:
  static int GetSize(CacheStorageContextImpl* cache_storage_context,
                     const GURL& origin,
                     const std::string& cache_name,
                     const GURL& url) {
    scoped_refptr<CacheStorageSideDataSizeChecker> checker(
        new CacheStorageSideDataSizeChecker(cache_storage_context, origin,
                                            cache_name, url));
    return checker->GetSizeImpl();
  }

 private:
  using self = CacheStorageSideDataSizeChecker;
  friend class base::RefCountedThreadSafe<self>;

  CacheStorageSideDataSizeChecker(
      CacheStorageContextImpl* cache_storage_context,
      const GURL& origin,
      const std::string& cache_name,
      const GURL& url)
      : cache_storage_context_(cache_storage_context),
        origin_(origin),
        cache_name_(cache_name),
        url_(url) {}

  ~CacheStorageSideDataSizeChecker() {}

  int GetSizeImpl() {
    int result = 0;
    RunOnCoreThread(
        base::BindOnce(&self::OpenCacheOnCoreThread, this, &result));
    return result;
  }

  void OpenCacheOnCoreThread(int* result, base::OnceClosure continuation) {
    CacheStorageHandle cache_storage =
        cache_storage_context_->CacheManager()->OpenCacheStorage(
            url::Origin::Create(origin_), CacheStorageOwner::kCacheAPI);
    cache_storage.value()->OpenCache(
        cache_name_, /* trace_id = */ 0,
        base::BindOnce(&self::OnCacheStorageOpenCallback, this, result,
                       std::move(continuation)));
  }

  void OnCacheStorageOpenCallback(int* result,
                                  base::OnceClosure continuation,
                                  CacheStorageCacheHandle cache_handle,
                                  CacheStorageError error) {
    ASSERT_EQ(CacheStorageError::kSuccess, error);
    auto scoped_request = blink::mojom::FetchAPIRequest::New();
    scoped_request->url = url_;
    CacheStorageCache* cache = cache_handle.value();
    cache->Match(
        std::move(scoped_request), nullptr,
        CacheStorageSchedulerPriority::kNormal, /* trace_id = */ 0,
        base::BindOnce(&self::OnCacheStorageCacheMatchCallback, this, result,
                       std::move(continuation), std::move(cache_handle)));
  }

  void OnCacheStorageCacheMatchCallback(
      int* result,
      base::OnceClosure continuation,
      CacheStorageCacheHandle cache_handle,
      CacheStorageError error,
      blink::mojom::FetchAPIResponsePtr response) {
    if (error == CacheStorageError::kErrorNotFound) {
      *result = 0;
      std::move(continuation).Run();
      return;
    }

    ASSERT_EQ(CacheStorageError::kSuccess, error);
    ASSERT_TRUE(response->side_data_blob);
    auto blob_handle = base::MakeRefCounted<storage::BlobHandle>(
        std::move(response->side_data_blob->blob));
    blob_handle->get()->ReadSideData(base::BindOnce(
        [](scoped_refptr<storage::BlobHandle> blob_handle, int* result,
           base::OnceClosure continuation,
           const base::Optional<mojo_base::BigBuffer> data) {
          *result = data ? data->size() : 0;
          std::move(continuation).Run();
        },
        blob_handle, result, std::move(continuation)));
  }

  CacheStorageContextImpl* cache_storage_context_;
  const GURL origin_;
  const std::string cache_name_;
  const GURL url_;
  DISALLOW_COPY_AND_ASSIGN(CacheStorageSideDataSizeChecker);
};

class ServiceWorkerV8CodeCacheForCacheStorageTest
    : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerV8CodeCacheForCacheStorageTest() = default;
  ~ServiceWorkerV8CodeCacheForCacheStorageTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

 protected:
  virtual std::string GetWorkerURL() { return kWorkerUrl; }

  void RegisterAndActivateServiceWorker() {
    auto observer = base::MakeRefCounted<WorkerStateObserver>(
        wrapper(), ServiceWorkerVersion::ACTIVATED);
    observer->Init();
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kPageUrl),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(GetWorkerURL()), options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
  }

  void NavigateToTestPage() {
    const base::string16 title =
        base::ASCIIToUTF16("Title was changed by the script.");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  void WaitUntilSideDataSizeIs(int expected_size) {
    while (true) {
      if (GetSideDataSize() == expected_size)
        return;
    }
  }

  void WaitUntilSideDataSizeIsBiggerThan(int minimum_size) {
    while (true) {
      if (GetSideDataSize() > minimum_size)
        return;
    }
  }

 private:
  static const char kPageUrl[];
  static const char kWorkerUrl[];
  static const char kScriptUrl[];

  int GetSideDataSize() {
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    return CacheStorageSideDataSizeChecker::GetSize(
        static_cast<CacheStorageContextImpl*>(
            partition->GetCacheStorageContext()),
        embedded_test_server()->base_url(), std::string("cache_name"),
        embedded_test_server()->GetURL(kScriptUrl));
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CodeCacheForCacheStorageTest);
};

const char ServiceWorkerV8CodeCacheForCacheStorageTest::kPageUrl[] =
    "/service_worker/v8_cache_test.html";
const char ServiceWorkerV8CodeCacheForCacheStorageTest::kWorkerUrl[] =
    "/service_worker/fetch_event_response_via_cache.js";
const char ServiceWorkerV8CodeCacheForCacheStorageTest::kScriptUrl[] =
    "/service_worker/v8_cache_test.js";

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CodeCacheForCacheStorageTest,
                       V8CacheOnCacheStorage) {
  RegisterAndActivateServiceWorker();

  // First load: fetch_event_response_via_cache.js returns |cloned_response|.
  // The V8 code cache should not be stored in CacheStorage.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);

  // Second load: The V8 code cache should be stored in CacheStorage. It must
  // have size greater than 16 bytes.
  NavigateToTestPage();
  WaitUntilSideDataSizeIsBiggerThan(kV8CacheTimeStampDataSize);
}

class ServiceWorkerV8CodeCacheForCacheStorageNoneTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerV8CodeCacheForCacheStorageNoneTest() {}
  ~ServiceWorkerV8CodeCacheForCacheStorageNoneTest() override {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheOptions, "none");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CodeCacheForCacheStorageNoneTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CodeCacheForCacheStorageNoneTest,
                       V8CacheOnCacheStorage) {
  RegisterAndActivateServiceWorker();

  // First load.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);

  // Second load: The V8 code cache must not be stored even after the second
  // load when --v8-cache-options=none is set.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);
}

namespace {

class CodeCacheHostInterceptor
    : public blink::mojom::CodeCacheHostInterceptorForTesting,
      public RenderProcessHostObserver {
 public:
  CodeCacheHostInterceptor(RenderProcessHost* rph,
                           CodeCacheHostImpl* code_cache_host_impl)
      : render_process_host_(rph), code_cache_host_impl_(code_cache_host_impl) {
    // Intercept messages for the CodeCacheHost.
    code_cache_host_impl_->receiver().SwapImplForTesting(this);

    // Register with the RenderProcessHost so we can cleanup properly.
    render_process_host_->AddObserver(this);
  }

  ~CodeCacheHostInterceptor() override {
    if (render_process_host_)
      render_process_host_->RemoveObserver(this);
  }

  CodeCacheHost* GetForwardingInterface() override {
    return code_cache_host_impl_;
  }

  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    DCHECK(host == render_process_host_);

    // The CodeCacheHostImpl will be destroyed when the renderer exits.
    // Drop our reference to avoid holding a stale pointer.
    code_cache_host_impl_ = nullptr;

    render_process_host_->RemoveObserver(this);
    render_process_host_ = nullptr;
  }

  void DidGenerateCacheableMetadataInCacheStorage(
      const GURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const url::Origin& cache_storage_origin,
      const std::string& cache_storage_cache_name) override {
    // Send the message with an overriden, bad origin.
    GetForwardingInterface()->DidGenerateCacheableMetadataInCacheStorage(
        url, expected_response_time, std::move(data),
        url::Origin::Create(GURL("https://bad.com")), cache_storage_cache_name);
  }

 private:
  // These can be held as raw pointers since we use the
  // RenderProcessHostObserver interface to clear them before they are
  // destroyed.
  RenderProcessHost* render_process_host_;
  CodeCacheHostImpl* code_cache_host_impl_;
};

class CacheStorageContextForBadOrigin : public CacheStorageContextImpl {
 public:
  scoped_refptr<CacheStorageManager> CacheManager() override {
    // The CodeCacheHostImpl should not try to access the CacheManager()
    // if the origin is bad.
    NOTREACHED();
    return nullptr;
  }

 private:
  ~CacheStorageContextForBadOrigin() override = default;
};

}  // namespace

// Test that forces a bad origin to be sent to CodeCacheHost's
// DidGenerateCacheableMetadataInCacheStorage method.
class ServiceWorkerV8CodeCacheForCacheStorageBadOriginTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerV8CodeCacheForCacheStorageBadOriginTest() {
    // Register a callback to be notified of new CodeCacheHostImpl objects.
    RenderProcessHostImpl::SetCodeCacheHostReceiverHandlerForTesting(
        base::BindRepeating(
            &ServiceWorkerV8CodeCacheForCacheStorageBadOriginTest::
                CreateTestCodeCacheHost,
            base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ServiceWorkerV8CodeCacheForCacheStorageTest::SetUpCommandLine(command_line);
    // The purpose of this test is to verify how CodeCacheHostImpl behaves
    // when it receives an origin that is different from the site locked to the
    // process.  In order for this to work properly on platforms like android
    // we must explicitly enable site isolation.
    IsolateAllSitesForTesting(command_line);
  }

  ~ServiceWorkerV8CodeCacheForCacheStorageBadOriginTest() override {
    // Disable the callback now that this object is being destroyed.
    RenderProcessHostImpl::SetCodeCacheHostReceiverHandlerForTesting(
        RenderProcessHostImpl::CodeCacheHostReceiverHandler());
  }

  void CreateTestCodeCacheHost(RenderProcessHost* rph,
                               CodeCacheHostImpl* code_cache_host_impl) {
    // Override the cache_storage context to assert that CodeCacheHostImpl
    // does not try to access it when given a bad origin.
    code_cache_host_impl->SetCacheStorageContextForTesting(
        base::MakeRefCounted<CacheStorageContextForBadOrigin>());

    // Create an interceptor that passes a bad origin to CodeCacheHostImpl.
    interceptors_.push_back(
        std::make_unique<CodeCacheHostInterceptor>(rph, code_cache_host_impl));
  }

 private:
  // Track the CodeCacheHostIntercptor objects so we can delete them in
  // the test destructor.
  std::vector<std::unique_ptr<CodeCacheHostInterceptor>> interceptors_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CodeCacheForCacheStorageBadOriginTest,
                       V8CacheOnCacheStorage) {
  RegisterAndActivateServiceWorker();

  // First load: fetch_event_response_via_cache.js returns |cloned_response|.
  // The V8 code cache should not be stored in CacheStorage.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);

  // Second load: The V8 code cache should still be zero.  When a bad origin
  // is received by CodeCacheHost it should ignore the provided metadata.
  // TODO(crbug/925035): In the future this should instead kill the renderer.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);
}

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  std::string GetWorkerURL() override {
    return "/service_worker/install_event_caches_script.js";
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventTest,
    FullCodeCacheGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should have been generated when the script was
  // stored in the install event.
  WaitUntilSideDataSizeIsBiggerThan(kV8CacheTimeStampDataSize);
}

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "CacheStorageCodeCacheHint");
  }

  std::string GetWorkerURL() override {
    return "/service_worker/install_event_caches_script_with_hint.js";
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest,
    FullCodeCacheNotGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should not be generated when the script was
  // stored in the install event and the header hint disables code cache.
  WaitUntilSideDataSizeIs(0);
}

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ServiceWorkerV8CodeCacheForCacheStorageTest::SetUpOnMainThread();
  }

  std::string GetWorkerURL() override {
    GURL cross_origin_script = embedded_test_server()->GetURL(
        "bar.com", "/service_worker/v8_cache_test.js");
    return "/service_worker/"
           "install_event_caches_no_cors_script.js?script_url=" +
           cross_origin_script.spec();
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest,
    FullCodeCacheGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should not be generated when the script is an opaque
  // response.
  WaitUntilSideDataSizeIs(0);
}

// ServiceWorkerDisableWebSecurityTests check the behavior when the web security
// is disabled. If '--disable-web-security' flag is set, we don't check the
// origin equality in Blink. So the Service Worker related APIs should succeed
// even if it is thouching other origin Service Workers.
class ServiceWorkerDisableWebSecurityTest : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerDisableWebSecurityTest() {}
  ~ServiceWorkerDisableWebSecurityTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void SetUpOnMainThread() override {
    cross_origin_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(cross_origin_server_.Start());
    ServiceWorkerBrowserTest::SetUpOnMainThread();
  }

  void RegisterServiceWorkerOnCrossOriginServer(const std::string& scope,
                                                const std::string& script) {
    auto observer = base::MakeRefCounted<WorkerStateObserver>(
        wrapper(), ServiceWorkerVersion::ACTIVATED);
    observer->Init();
    blink::mojom::ServiceWorkerRegistrationOptions options(
        cross_origin_server_.GetURL(scope), blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        cross_origin_server_.GetURL(script), options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
  }

  void RunTestWithCrossOriginURL(const std::string& test_page,
                                 const std::string& cross_origin_url) {
    const base::string16 title = base::ASCIIToUTF16("PASS");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     test_page + "?" +
                     cross_origin_server_.GetURL(cross_origin_url).spec())));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

 private:
  net::EmbeddedTestServer cross_origin_server_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDisableWebSecurityTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest,
                       GetRegistrationNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] =
      "/service_worker/disable_web_security_get_registration.html";
  const char kScopeUrl[] = "/service_worker/";
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest, RegisterNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/disable_web_security_register.html";
  const char kScopeUrl[] = "/service_worker/";
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest, UnregisterNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] =
      "/service_worker/disable_web_security_unregister.html";
  const char kScopeUrl[] = "/service_worker/scope/";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  RegisterServiceWorkerOnCrossOriginServer(kScopeUrl, kWorkerUrl);
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest, UpdateNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/disable_web_security_update.html";
  const char kScopeUrl[] = "/service_worker/scope/";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  RegisterServiceWorkerOnCrossOriginServer(kScopeUrl, kWorkerUrl);
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

class HeaderInjectingThrottle : public blink::URLLoaderThrottle {
 public:
  HeaderInjectingThrottle() = default;
  ~HeaderInjectingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    GURL url = request->url;
    if (url.query().find("PlzRedirect") != std::string::npos) {
      GURL::Replacements replacements;
      replacements.SetQueryStr("DidRedirect");
      request->url = url.ReplaceComponents(replacements);
      return;
    }

    request->headers.SetHeader("x-injected", "injected value");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HeaderInjectingThrottle);
};

class ThrottlingContentBrowserClient : public TestContentBrowserClient {
 public:
  ThrottlingContentBrowserClient() : TestContentBrowserClient() {}
  ~ThrottlingContentBrowserClient() override {}

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    auto throttle = std::make_unique<HeaderInjectingThrottle>();
    throttles.push_back(std::move(throttle));
    return throttles;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThrottlingContentBrowserClient);
};

class ServiceWorkerURLLoaderThrottleTest : public ServiceWorkerBrowserTest {
 public:
  ~ServiceWorkerURLLoaderThrottleTest() override {}

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ServiceWorkerBrowserTest::TearDownOnMainThread();
  }

  void RegisterServiceWorker(const std::string& worker_url) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_url + "');"));
  }

  void RegisterServiceWorkerWithScope(const std::string& worker_url,
                                      const std::string& scope) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_url + "', '" +
                                          scope + "');"));
  }
};

// Test that the throttles can inject headers during navigation that are
// observable inside the service worker's fetch event.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       FetchEventForNavigationHasThrottledRequest) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;
  auto* old_content_browser_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Register the service worker.
  RegisterServiceWorker("/service_worker/echo_request_headers.js");

  // Perform a navigation. Add "?dump_headers" to tell the service worker to
  // respond with the request headers.
  GURL url =
      embedded_test_server()->GetURL("/service_worker/empty.html?dump_headers");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Extract the headers.
  EvalJsResult result = EvalJs(shell()->web_contents()->GetMainFrame(),
                               "document.body.textContent");
  ASSERT_TRUE(result.error.empty());
  std::unique_ptr<base::DictionaryValue> dict = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(result.ExtractString()));
  ASSERT_TRUE(dict);

  // Default headers are present.
  const char* frame_accept_c_str = network::kFrameAcceptHeaderValue;
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kAVIF)) {
    frame_accept_c_str =
        "text/html,application/xhtml+xml,application/xml;q=0.9,"
        "image/avif,image/webp,image/apng,*/*;q=0.8";
  }
#endif
  EXPECT_TRUE(CheckHeader(*dict, "accept",
                          std::string(frame_accept_c_str) +
                              std::string(kAcceptHeaderSignedExchangeSuffix)));

  // Injected headers are present.
  EXPECT_TRUE(CheckHeader(*dict, "x-injected", "injected value"));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that redirects by throttles occur before service worker interception.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       RedirectOccursBeforeFetchEvent) {
  // Add a throttle which performs a redirect.
  ThrottlingContentBrowserClient content_browser_client;
  auto* old_content_browser_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Register the service worker.
  RegisterServiceWorker("/service_worker/fetch_event_pass_through.js");

  // Perform a navigation. Add "?PlzRedirect" to tell the throttle to
  // redirect to another URL.
  GURL url =
      embedded_test_server()->GetURL("/service_worker/empty.html?PlzRedirect");
  GURL redirect_url =
      embedded_test_server()->GetURL("/service_worker/empty.html?DidRedirect");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  EXPECT_EQ(redirect_url, shell()->web_contents()->GetLastCommittedURL());

  // This script asks the service worker what fetch events it saw.
  const std::string script = R"(
      (async () => {
        const saw_message = new Promise(resolve => {
          navigator.serviceWorker.onmessage = event => {
            resolve(event.data);
          };
        });
        const registration = await navigator.serviceWorker.ready;
        registration.active.postMessage('');
        return await saw_message;
      })();
  )";

  // Ensure the service worker did not see a fetch event for the PlzRedirect
  // URL, since throttles should have redirected before interception.
  base::Value list(base::Value::Type::LIST);
  list.Append(redirect_url.spec());
  EXPECT_EQ(list, EvalJs(shell()->web_contents()->GetMainFrame(), script));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that the headers injected by throttles during navigation are
// present in the network request in the case of network fallback.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerURLLoaderThrottleTest,
    NavigationHasThrottledRequestHeadersAfterNetworkFallback) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;
  auto* old_content_browser_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Register the service worker. Use "/" scope so the "/echoheader" default
  // handler of EmbeddedTestServer is in-scope.
  RegisterServiceWorkerWithScope("/service_worker/fetch_event_pass_through.js",
                                 "/");

  // Perform a navigation. Use "/echoheader" which echoes the given header.
  GURL url = embedded_test_server()->GetURL("/echoheader?x-injected");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that there is a controller to check that the test is really testing
  // service worker network fallback.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetMainFrame(),
                         "!!navigator.serviceWorker.controller"));

  // The injected header should be present.
  EXPECT_EQ("injected value", EvalJs(shell()->web_contents()->GetMainFrame(),
                                     "document.body.textContent"));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that the headers injected by throttles during navigation are
// present in the navigation preload request.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       NavigationPreloadHasThrottledRequestHeaders) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;
  auto* old_content_browser_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Register the service worker. Use "/" scope so the "/echoheader" default
  // handler of EmbeddedTestServer is in-scope.
  RegisterServiceWorkerWithScope("/service_worker/navigation_preload_worker.js",
                                 "/");

  // Perform a navigation. Use "/echoheader" which echoes the given header. The
  // server responds to the navigation preload request with this echoed
  // response, and the service worker responds with the navigation preload
  // response.
  //
  // Also test that "Service-Worker-Navigation-Preload" is present to verify
  // we are testing the navigation preload request.
  GURL url = embedded_test_server()->GetURL(
      "/echoheader?Service-Worker-Navigation-Preload&x-injected");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ("true\ninjected value",
            EvalJs(shell()->web_contents()->GetMainFrame(),
                   "document.body.textContent"));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test fixture to support validating throttling from within an installing
// service worker.
class ServiceWorkerThrottlingTest : public ServiceWorkerBrowserTest {
 protected:
  ServiceWorkerThrottlingTest() {
    // Configure the field trial param to trigger throttling after
    // there are only 2 outstanding requests from an installiner
    // service worker.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kThrottleInstallingServiceWorker, {{"limit", "2"}});
  }

  void RegisterServiceWorkerAndWaitForState(
      const std::string& script_url,
      const std::string& scope,
      ServiceWorkerVersion::Status state) {
    auto observer = base::MakeRefCounted<WorkerStateObserver>(wrapper(), state);
    observer->Init();
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(scope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(script_url), options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
  }

  int GetBlockingResponseCount() { return blocking_response_list_.size(); }

  void StopBlocking() {
    std::vector<scoped_refptr<BlockingResponse>> list;
    {
      base::AutoLock auto_lock(lock_);
      should_block_ = false;
      list = std::move(blocking_response_list_);
    }
    for (const auto& response : list) {
      response->StopBlocking();
    }
  }

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    // Configure the EmbeddedTestServer to use our custom request handler
    // to return blocking responses where appropriate.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ServiceWorkerThrottlingTest::HandleRequest, base::Unretained(this)));
    StartServerAndNavigateToSetup();
  }

 private:
  // An object representing an http response that blocks returning its status
  // code until the test tells it to proceed.
  class BlockingResponse : public base::RefCountedThreadSafe<BlockingResponse> {
   public:
    // We must return ownership of a net::test_server::HttpResponse from
    // HandleRequest(), but we also want to track the Response in our test
    // so that we can unblock the response.  In addition, the EmbeddedTestServer
    // deletes its HttpResponse after calling SendResponse().  Therefore, we
    // use an inner class to return to EmbeddedTestServer and hold the
    // outer BlockingResponse alive in the test itself.  The inner class simply
    // forwards the SendResponse() method to the outer class.
    class Inner : public net::test_server::HttpResponse {
     public:
      explicit Inner(base::WeakPtr<BlockingResponse> owner)
          : owner_(std::move(owner)) {}

      ~Inner() override = default;
      void SendResponse(const net::test_server::SendBytesCallback& send,
                        base::OnceClosure done) override {
        if (owner_)
          owner_->SendResponse(std::move(send), std::move(done));
      }

     private:
      base::WeakPtr<BlockingResponse> owner_;
    };

    BlockingResponse() : task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

    // Mint an HttpResponse suitable for returning to the EmbeddedTestServer
    // that will forward to this BlockingResponse.
    std::unique_ptr<net::test_server::HttpResponse> GetResponse() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      return std::make_unique<Inner>(weak_factory_.GetWeakPtr());
    }

    // Called by the EmbeddedTestServer via our inner class.  The callbacks
    // are stored and invoked later when we've been told to unblock.
    void SendResponse(const net::test_server::SendBytesCallback& send,
                      base::OnceClosure done) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      send_ = send;
      done_ = std::move(done);
      if (should_block_) {
        blocking_ = true;
        return;
      }
      CompleteResponseOnTaskRunner();
    }

    // Called by the test when we want to unblock this response.
    void StopBlocking() {
      // Called on the main thread by the test.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&BlockingResponse::StopBlockingOnTaskRunner, this));
    }

   private:
    friend class base::RefCountedThreadSafe<BlockingResponse>;
    ~BlockingResponse() = default;

    void StopBlockingOnTaskRunner() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      should_block_ = false;
      if (!blocking_)
        return;
      blocking_ = false;
      CompleteResponseOnTaskRunner();
    }

    void CompleteResponseOnTaskRunner() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      const char kPageResponse[] =
          "HTTP/1.1 200 HELLOWORLD\r\n"
          "Connection: close\r\n"
          "Content-Length: 32\r\n"
          "Content-Type: text/html\r\n"
          "Cache-Control: no-store\r\n"
          "\r\n"
          "<title>ERROR</title>Hello world.";
      std::move(send_).Run(kPageResponse, std::move(done_));
    }

    // Accessed on any thread.
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    // All other members only accessed on |task_runner_| sequence.
    net::test_server::SendBytesCallback send_;
    base::OnceClosure done_;
    bool should_block_ = true;
    bool blocking_ = false;
    base::WeakPtrFactory<BlockingResponse> weak_factory_{this};
  };

  // Return a blocking response to the EmbeddedTestServer for any
  // request where there is a search param named "block".
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    if (!should_block_ ||
        request.GetURL().query().find("block") == std::string::npos) {
      return nullptr;
    }
    auto response = base::MakeRefCounted<BlockingResponse>();
    blocking_response_list_.push_back(std::move(response));
    return blocking_response_list_.back()->GetResponse();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Lock lock_;
  // Accessed from multiple threads, but protected by |lock_|.
  std::vector<scoped_refptr<BlockingResponse>> blocking_response_list_;
  // Accessed from multiple threads, but protected by |lock_|.
  bool should_block_ = true;
  base::WeakPtrFactory<ServiceWorkerThrottlingTest> weak_factory_{this};
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerThrottlingTest, ThrottleInstalling) {
  // Register a service worker that loads 3 resources in its install
  // handler.  The test server will cause these loads to block which
  // should trigger throttling on the third request.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_blocking_sw.js",
      "/service_worker/throttling_blocking", ServiceWorkerVersion::INSTALLING);

  // Register a second service worker that also loads 3 resources in
  // its install handler.  The test server will not block these loads
  // and the worker should progress to the activated state.
  //
  // This second service worker is used to wait for the first worker
  // to potentially request its resources.  By the time the second worker
  // activates the first worker should have requested its resources and
  // triggered throttling.  This avoids the need for an arbitrary timeout.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_non_blocking_sw.js",
      "/service_worker/throttling_non_blocking",
      ServiceWorkerVersion::ACTIVATED);

  // If throttling worked correctly then there should only be 2 outstanding
  // requests blocked by the test server.
  EXPECT_EQ(2, GetBlockingResponseCount());

  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();

  // Stop blocking the resources loaded by the first service worker.
  StopBlocking();

  // Verify that throttling correctly notes when resources can load and
  // the first service worker fully activates.
  observer->Wait();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerThrottlingTest,
                       ThrottleInstallingWithCacheAddAll) {
  // Register a service worker that loads 3 resources in its install
  // handler via cache.addAll().  The test server will cause these loads
  // to block which should trigger throttling on the third request.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_blocking_cache_addall_sw.js",
      "/service_worker/throttling_blocking_cache_addall",
      ServiceWorkerVersion::INSTALLING);

  // Register a second service worker that also loads 3 resources in
  // its install handler using cache.addAll().  The test server will not
  // block these loads and the worker should progress to the activated state.
  //
  // This second service worker is used to wait for the first worker
  // to potentially request its resources.  By the time the second worker
  // activates the first worker should have requested its resources and
  // triggered throttling.  This avoids the need for an arbitrary timeout.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_non_blocking_cache_addall_sw.js",
      "/service_worker/throttling_non_blocking_cache_addall",
      ServiceWorkerVersion::ACTIVATED);

  // If throttling worked correctly then there should only be 2 outstanding
  // requests blocked by the test server.
  EXPECT_EQ(2, GetBlockingResponseCount());

  auto observer = base::MakeRefCounted<WorkerStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  observer->Init();

  // Stop blocking the resources loaded by the first service worker.
  StopBlocking();

  // Verify that throttling correctly notes when resources can load and
  // the first service worker fully activates.
  observer->Wait();
}

}  // namespace content
