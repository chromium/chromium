// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

// V8ScriptRunner::setCacheTimeStamp() stores 16 byte data (marker + tag +
// timestamp).
const int kV8CacheTimeStampDataSize =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(double);

struct FetchResult {
  blink::ServiceWorkerStatusCode status;
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
};

void RunAndQuit(base::OnceClosure closure,
                base::OnceClosure quit,
                base::SingleThreadTaskRunner* original_message_loop) {
  std::move(closure).Run();
  original_message_loop->PostTask(FROM_HERE, std::move(quit));
}

void RunOnIOThreadWithDelay(base::OnceClosure closure, base::TimeDelta delay) {
  base::RunLoop run_loop;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&RunAndQuit, std::move(closure), run_loop.QuitClosure(),
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get())),
      delay);
  run_loop.Run();
}

void RunOnIOThread(base::OnceClosure closure) {
  RunOnIOThreadWithDelay(std::move(closure), base::TimeDelta());
}

void RunOnIOThread(
    base::OnceCallback<void(base::OnceClosure continuation)> callback) {
  base::RunLoop run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          std::move(callback),
          base::BindOnce(
              base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
              base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
              run_loop.QuitClosure())));
  run_loop.Run();
}

void ReceivePrepareResult(bool* is_prepared) {
  *is_prepared = true;
}

base::OnceClosure CreatePrepareReceiver(bool* is_prepared) {
  return base::BindOnce(&ReceivePrepareResult, is_prepared);
}

void ReceiveFindRegistrationStatus(
    BrowserThread::ID run_quit_thread,
    base::OnceClosure quit,
    blink::ServiceWorkerStatusCode* out_status,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  *out_status = status;
  if (!quit.is_null())
    base::PostTaskWithTraits(FROM_HERE, {run_quit_thread}, std::move(quit));
}

ServiceWorkerStorage::FindRegistrationCallback CreateFindRegistrationReceiver(
    BrowserThread::ID run_quit_thread,
    base::OnceClosure quit,
    blink::ServiceWorkerStatusCode* status) {
  return base::BindOnce(&ReceiveFindRegistrationStatus, run_quit_thread,
                        std::move(quit), status);
}

void ReadResponseBody(std::string* body,
                      storage::BlobDataHandle* blob_data_handle) {
  ASSERT_TRUE(blob_data_handle);
  std::unique_ptr<storage::BlobDataSnapshot> data =
      blob_data_handle->CreateSnapshot();
  ASSERT_EQ(1U, data->items().size());
  *body =
      std::string(data->items()[0]->bytes().data(), data->items()[0]->length());
}

void ExpectResultAndRun(bool expected,
                        base::RepeatingClosure continuation,
                        bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

class WorkerActivatedObserver
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<WorkerActivatedObserver> {
 public:
  explicit WorkerActivatedObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}
  void Init() {
    RunOnIOThread(
        base::BindOnce(&WorkerActivatedObserver::InitOnIOThread, this));
  }
  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&WorkerActivatedObserver::Quit, this));
    }
  }
  void Wait() { run_loop_.Run(); }

  int64_t registration_id() { return registration_id_; }
  int64_t version_id() { return version_id_; }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override {}
  void InitOnIOThread() { context_->AddObserver(this); }
  void Quit() { run_loop_.Quit(); }

  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  base::RunLoop run_loop_;
  ServiceWorkerContextWrapper* context_;
  DISALLOW_COPY_AND_ASSIGN(WorkerActivatedObserver);
};

std::unique_ptr<net::test_server::HttpResponse>
VerifyServiceWorkerHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/service_worker/generated_sw.js")
    return nullptr;
  auto it = request.headers.find("Service-Worker");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_EQ("script", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return std::move(http_response);
}

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
VerifySaveDataHeaderNotInRequest(const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/service_worker/generated_sw.js")
    return nullptr;
  auto it = request.headers.find("Save-Data");
  EXPECT_EQ(request.headers.end(), it);
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

std::unique_ptr<net::test_server::HttpResponse>
VerifySaveDataNotInAccessControlRequestHeader(
    const net::test_server::HttpRequest& request) {
  // Save-Data should be present.
  auto it = request.headers.find("Save-Data");
  EXPECT_NE(request.headers.end(), it);
  EXPECT_EQ("on", it->second);

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
                 const base::Value* value) {
  value->GetAsString(result);
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

net::HttpResponseInfo CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, arraysize(data)));
  return info;
}

// Returns a unique script for each request, to test service worker update.
std::unique_ptr<net::test_server::HttpResponse> RequestHandlerForUpdateWorker(
    const net::test_server::HttpRequest& request) {
  static int counter = 0;

  if (request.relative_url != "/service_worker/update_worker.js")
    return std::unique_ptr<net::test_server::HttpResponse>();

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      base::StringPrintf("// empty script. counter = %d\n", counter++));
  http_response->set_content_type("text/javascript");
  // Use a large max-age to test the browser cache.
  http_response->AddCustomHeader("Cache-Control", "max-age=31536000");
  return http_response;
}

const char kNavigationPreloadAbortError[] =
    "NetworkError: The service worker navigation preload request was cancelled "
    "before 'preloadResponse' settled. If you intend to use 'preloadResponse', "
    "use waitUntil() or respondWith() to wait for the promise to settle.";
const char kNavigationPreloadNetworkError[] =
    "NetworkError: The service worker navigation preload request failed with "
    "a network error.";

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

    RunOnIOThread(
        base::BindOnce(&self::SetUpOnIOThread, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    RunOnIOThread(
        base::BindOnce(&self::TearDownOnIOThread, base::Unretained(this)));
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

  virtual void SetUpOnIOThread() {}
  virtual void TearDownOnIOThread() {}

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

class ConsoleListener : public EmbeddedWorkerInstance::Listener {
 public:
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ConsoleListener::OnReportConsoleMessageOnUI,
                       base::Unretained(this), message));
  }

  void WaitForConsoleMessages(size_t expected_message_count) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (messages_.size() >= expected_message_count)
      return;

    expected_message_count_ = expected_message_count;
    base::RunLoop console_run_loop;
    quit_ = console_run_loop.QuitClosure();
    console_run_loop.Run();

    ASSERT_EQ(messages_.size(), expected_message_count);
  }

  const std::vector<base::string16>& messages() const { return messages_; }

 private:
  void OnReportConsoleMessageOnUI(const base::string16& message) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    messages_.push_back(message);
    if (messages_.size() == expected_message_count_) {
      DCHECK(quit_);
      std::move(quit_).Run();
    }
  }

  // These parameters must be accessed on the UI thread.
  std::vector<base::string16> messages_;
  size_t expected_message_count_ = 0;
  base::OnceClosure quit_;
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

class ServiceWorkerVersionBrowserTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerVersionBrowserTest;

  ~ServiceWorkerVersionBrowserTest() override {}

  void TearDownOnIOThread() override {
    // Ensure that these resources are released while the IO thread still
    // exists.
    registration_ = nullptr;
    version_ = nullptr;
    blob_context_ = nullptr;
    remote_endpoints_.clear();
  }

  void InstallTestHelper(const std::string& worker_url,
                         blink::ServiceWorkerStatusCode expected_status,
                         blink::mojom::ScriptType script_type =
                             blink::mojom::ScriptType::kClassic) {
    RunOnIOThread(
        base::BindOnce(&self::SetUpRegistrationWithScriptTypeOnIOThread,
                       base::Unretained(this), worker_url, script_type));

    // Dispatch install on a worker.
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop install_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::InstallOnIOThread, base::Unretained(this),
                       install_run_loop.QuitClosure(), &status));
    install_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());

    // Stop the worker.
    base::RunLoop stop_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::StopOnIOThread, base::Unretained(this),
                       stop_run_loop.QuitClosure()));
    stop_run_loop.Run();
  }

  void ActivateTestHelper(blink::ServiceWorkerStatusCode expected_status) {
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::ActivateOnIOThread, base::Unretained(this),
                       run_loop.QuitClosure(), &status));
    run_loop.Run();
    ASSERT_EQ(expected_status, status.value());
  }

  void FetchOnRegisteredWorker(
      ServiceWorkerFetchDispatcher::FetchEventResult* result,
      blink::mojom::FetchAPIResponsePtr* response,
      std::unique_ptr<storage::BlobDataHandle>* blob_data_handle) {
    blob_context_ = ChromeBlobStorageContext::GetFor(
        shell()->web_contents()->GetBrowserContext());
    bool prepare_result = false;
    FetchResult fetch_result;
    fetch_result.status = blink::ServiceWorkerStatusCode::kErrorFailed;
    base::RunLoop fetch_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::FetchOnIOThread, base::Unretained(this),
                       fetch_run_loop.QuitClosure(), &prepare_result,
                       &fetch_result));
    fetch_run_loop.Run();
    ASSERT_TRUE(prepare_result);
    *result = fetch_result.result;
    *response = std::move(fetch_result.response);
    *blob_data_handle = std::move(fetch_result.blob_data_handle);
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, fetch_result.status);
  }

  void SetUpRegistrationOnIOThread(const std::string& worker_url) {
    SetUpRegistrationWithScriptTypeOnIOThread(
        worker_url, blink::mojom::ScriptType::kClassic);
  }

  void SetUpRegistrationWithScriptTypeOnIOThread(
      const std::string& worker_url,
      blink::mojom::ScriptType script_type) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const GURL scope = embedded_test_server()->GetURL("/service_worker/");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    options.type = script_type;
    registration_ = new ServiceWorkerRegistration(
        options, wrapper()->context()->storage()->NewRegistrationId(),
        wrapper()->context()->AsWeakPtr());
    // Set the update check time to avoid triggering updates in the middle of
    // tests.
    registration_->set_last_update_check(base::Time::Now());

    version_ = new ServiceWorkerVersion(
        registration_.get(), embedded_test_server()->GetURL(worker_url),
        script_type, wrapper()->context()->storage()->NewVersionId(),
        wrapper()->context()->AsWeakPtr());
    // Make the registration findable via storage functions.
    wrapper()->context()->storage()->NotifyInstallingRegistration(
        registration_.get());
  }

  void TimeoutWorkerOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_TRUE(version_->timeout_timer_.IsRunning());
    version_->SimulatePingTimeoutForTesting();
  }

  void AddControlleeOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(
            33 /* dummy render process id */, 1 /* dummy provider_id */,
            true /* is_parent_frame_secure */,
            wrapper()->context()->AsWeakPtr(), &remote_endpoints_.back());
    host->SetDocumentUrl(
        embedded_test_server()->GetURL("/service_worker/host"));
    host->SetControllerRegistration(registration_,
                                    false /* notify_controllerchange */);
    wrapper()->context()->AddProviderHost(std::move(host));
  }

  void AddWaitingWorkerOnIOThread(const std::string& worker_url) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    scoped_refptr<ServiceWorkerVersion> waiting_version(
        new ServiceWorkerVersion(
            registration_.get(), embedded_test_server()->GetURL(worker_url),
            blink::mojom::ScriptType::kClassic,
            wrapper()->context()->storage()->NewVersionId(),
            wrapper()->context()->AsWeakPtr()));
    waiting_version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    waiting_version->SetStatus(ServiceWorkerVersion::INSTALLED);
    registration_->SetWaitingVersion(waiting_version.get());
    registration_->ActivateWaitingVersionWhenReady();
  }

  void StartWorker(blink::ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop start_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::StartOnIOThread, base::Unretained(this),
                       start_run_loop.QuitClosure(), &status));
    start_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());
  }

  void StopWorker() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::RunLoop stop_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::StopOnIOThread, base::Unretained(this),
                       stop_run_loop.QuitClosure()));
    stop_run_loop.Run();
  }

  void StoreRegistration(int64_t version_id,
                         blink::ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop store_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::StoreOnIOThread, base::Unretained(this),
                       store_run_loop.QuitClosure(), &status, version_id));
    store_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());

    RunOnIOThread(
        base::BindOnce(&self::NotifyDoneInstallingRegistrationOnIOThread,
                       base::Unretained(this), status.value()));
  }

  void UpdateRegistration(int64_t registration_id,
                          blink::ServiceWorkerStatusCode* out_status,
                          bool* out_update_found) {
    base::RunLoop update_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &self::UpdateOnIOThread, base::Unretained(this), registration_id,
            base::BindOnce(
                &self::ReceiveUpdateResultOnIOThread, base::Unretained(this),
                update_run_loop.QuitClosure(), out_status, out_update_found)));
    update_run_loop.Run();
  }

  void FindRegistrationForId(int64_t id,
                             const GURL& origin,
                             blink::ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    base::RunLoop run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::FindRegistrationForIdOnIOThread,
                       base::Unretained(this), run_loop.QuitClosure(), &status,
                       id, origin));
    run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void FindRegistrationForIdOnIOThread(base::OnceClosure done,
                                       blink::ServiceWorkerStatusCode* result,
                                       int64_t id,
                                       const GURL& origin) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->storage()->FindRegistrationForId(
        id, origin,
        CreateFindRegistrationReceiver(BrowserThread::UI, std::move(done),
                                       result));
  }

  void NotifyDoneInstallingRegistrationOnIOThread(
      blink::ServiceWorkerStatusCode status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->storage()->NotifyDoneInstallingRegistration(
        registration_.get(), version_.get(), status);
  }

  void RemoveLiveRegistrationOnIOThread(int64_t id) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->RemoveLiveRegistration(id);
  }

  void StartOnIOThread(base::OnceClosure done,
                       base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
    version_->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
  }

  void InstallOnIOThread(
      const base::RepeatingClosure& done,
      base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::INSTALL,
        base::BindOnce(&self::DispatchInstallEventOnIOThread,
                       base::Unretained(this), done, result));
  }

  void DispatchInstallEventOnIOThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result,
      blink::ServiceWorkerStatusCode start_worker_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, start_worker_status);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    auto repeating_done = base::AdaptCallbackForRepeating(std::move(done));
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::INSTALL,
        CreateReceiver(BrowserThread::UI, repeating_done, result));
    version_->endpoint()->DispatchInstallEvent(base::BindOnce(
        &self::ReceiveInstallEventOnIOThread, base::Unretained(this),
        repeating_done, result, request_id));
  }

  void ReceiveInstallEventOnIOThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* out_result,
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status,
      bool has_fetch_handler,
      base::TimeTicks dispatch_event_time) {
    version_->FinishRequest(
        request_id, status == blink::mojom::ServiceWorkerEventStatus::COMPLETED,
        dispatch_event_time);
    version_->set_fetch_handler_existence(
        has_fetch_handler
            ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
            : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);

    *out_result = mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status);
    if (!done.is_null())
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(done));
  }

  void StoreOnIOThread(base::OnceClosure done,
                       base::Optional<blink::ServiceWorkerStatusCode>* result,
                       int64_t version_id) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerVersion* version =
        wrapper()->context()->GetLiveVersion(version_id);
    wrapper()->context()->storage()->StoreRegistration(
        registration_.get(), version,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
  }

  void ActivateOnIOThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::ACTIVATE,
        base::BindOnce(&self::DispatchActivateEventOnIOThread,
                       base::Unretained(this), std::move(done), result));
  }

  void DispatchActivateEventOnIOThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result,
      blink::ServiceWorkerStatusCode start_worker_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, start_worker_status);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    registration_->SetActiveVersion(version_.get());
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::INSTALL,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
    version_->endpoint()->DispatchActivateEvent(
        version_->CreateSimpleEventCallback(request_id));
  }

  void UpdateOnIOThread(int registration_id,
                        ServiceWorkerContextCore::UpdateCallback callback) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    wrapper()->context()->UpdateServiceWorker(
        registration, false /* force_bypass_cache */,
        false /* skip_script_comparison */, std::move(callback));
  }

  void ReceiveUpdateResultOnIOThread(const base::RepeatingClosure& done_on_ui,
                                     blink::ServiceWorkerStatusCode* out_status,
                                     bool* out_update_found,
                                     blink::ServiceWorkerStatusCode status,
                                     const std::string& status_message,
                                     int64_t registration_id) {
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    DCHECK(registration);

    *out_status = status;
    *out_update_found = !!registration->installing_version();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, done_on_ui);
  }

  void FetchOnIOThread(base::OnceClosure done,
                       bool* prepare_result,
                       FetchResult* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    GURL url = embedded_test_server()->GetURL("/service_worker/empty.html");
    ResourceType resource_type = RESOURCE_TYPE_MAIN_FRAME;
    base::OnceClosure prepare_callback = CreatePrepareReceiver(prepare_result);
    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback =
        CreateResponseReceiver(std::move(done), blob_context_.get(), result);
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->method = "GET";
    request->resource_type = resource_type;
    fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
        std::move(request), std::string() /* request_body_blob_uuid */,
        0 /* request_body_blob_size */, nullptr /* request_body_blob */,
        std::string() /* client_id */, version_, net::NetLogWithSource(),
        std::move(prepare_callback), std::move(fetch_callback));
    fetch_dispatcher_->Run();
  }

  base::Time GetLastUpdateCheck(int64_t registration_id) {
    base::Time last_update_time;
    base::RunLoop time_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::GetLastUpdateCheckOnIOThread,
                       base::Unretained(this), registration_id,
                       &last_update_time, time_run_loop.QuitClosure()));
    time_run_loop.Run();
    return last_update_time;
  }

  void GetLastUpdateCheckOnIOThread(int64_t registration_id,
                                    base::Time* out_time,
                                    const base::RepeatingClosure& done_on_ui) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    *out_time = registration->last_update_check();
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, done_on_ui);
  }

  void SetLastUpdateCheckOnIOThread(int64_t registration_id,
                                    base::Time last_update_time,
                                    const base::RepeatingClosure& done_on_ui) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    registration->set_last_update_check(last_update_time);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, done_on_ui);
  }

  // Contrary to the style guide, the output parameter of this function comes
  // before input parameters so Bind can be used on it to create a FetchCallback
  // to pass to DispatchFetchEvent.
  void ReceiveFetchResultOnIOThread(
      base::OnceClosure quit,
      ChromeBlobStorageContext* blob_context,
      FetchResult* out_result,
      blink::ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchDispatcher::FetchEventResult actual_result,
      blink::mojom::FetchAPIResponsePtr actual_response,
      blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
      blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
      scoped_refptr<ServiceWorkerVersion> worker) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(fetch_dispatcher_);
    fetch_dispatcher_.reset();
    out_result->status = actual_status;
    out_result->result = actual_result;
    out_result->response = std::move(actual_response);
    if (out_result->response->blob) {
      DCHECK(!out_result->response->blob->uuid.empty());
      out_result->blob_data_handle =
          blob_context->context()->GetBlobDataFromUUID(
              out_result->response->blob->uuid);
    }
    if (!quit.is_null())
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(quit));
  }

  ServiceWorkerFetchDispatcher::FetchCallback CreateResponseReceiver(
      base::OnceClosure quit,
      ChromeBlobStorageContext* blob_context,
      FetchResult* result) {
    return base::BindOnce(&self::ReceiveFetchResultOnIOThread,
                          base::Unretained(this), std::move(quit),
                          base::RetainedRef(blob_context), result);
  }

  void StopOnIOThread(base::OnceClosure done) {
    ASSERT_TRUE(version_.get());
    version_->StopWorker(std::move(done));
  }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<ChromeBlobStorageContext> blob_context_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartAndStop) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));

  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StartOnIOThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Stop the worker.
  base::RunLoop stop_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StopOnIOThread, base::Unretained(this),
                     stop_run_loop.QuitClosure()));
  stop_run_loop.Run();
}

// TODO(lunalu): remove this test when blink side use counter is removed
// (crbug.com/811948).
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       DropCountsOnBlinkUseCounter) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));
  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StartOnIOThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));

  // Stop the worker.
  base::RunLoop stop_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StopOnIOThread, base::Unretained(this),
                     stop_run_loop.QuitClosure()));
  stop_run_loop.Run();
  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartNotFound) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/nonexistent.js"));

  // Start a worker for nonexistent URL.
  StartWorker(blink::ServiceWorkerStatusCode::kErrorNetwork);
}

#if defined(ANDROID)
// Flaky failures on Android; see https://crbug.com/720275.
#define MAYBE_ReadResourceFailure DISABLED_ReadResourceFailure
#else
#define MAYBE_ReadResourceFailure ReadResourceFailure
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       MAYBE_ReadResourceFailure) {
  StartServerAndNavigateToSetup();
  // Create a registration.
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Add a non-existent resource to the version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100));
  version_->script_cache_map()->SetResources(records);

  // Store the registration.
  StoreRegistration(version_->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the worker. We'll fail to read the resource.
  StartWorker(blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should be deleted from storage since the broken worker was
  // the stored one.
  RunOnIOThread(base::BindOnce(&self::RemoveLiveRegistrationOnIOThread,
                               base::Unretained(this), registration_->id()));
  FindRegistrationForId(registration_->id(), registration_->scope().GetOrigin(),
                        blink::ServiceWorkerStatusCode::kErrorNotFound);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ReadResourceFailure_WaitingWorker) {
  StartServerAndNavigateToSetup();
  // Create a registration and active version.
  InstallTestHelper("/service_worker/worker.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);
  ASSERT_TRUE(registration_->active_version());

  // Give the version a controllee.
  RunOnIOThread(
      base::BindOnce(&self::AddControlleeOnIOThread, base::Unretained(this)));

  // Add a non-existent resource to the version.
  version_->script_cache_map()->resource_map_[version_->script_url()] =
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100);

  // Make a waiting version and store it.
  RunOnIOThread(base::BindOnce(&self::AddWaitingWorkerOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));
  std::vector<ServiceWorkerDatabase::ResourceRecord> records = {
      ServiceWorkerDatabase::ResourceRecord(31, version_->script_url(), 100)};
  registration_->waiting_version()->script_cache_map()->SetResources(records);
  StoreRegistration(registration_->waiting_version()->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the broken worker. We'll fail to read from disk and the worker should
  // be doomed.
  StopWorker();  // in case it's already running
  StartWorker(blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should still be in storage since the waiting worker was
  // the stored one.
  RunOnIOThread(base::BindOnce(&self::RemoveLiveRegistrationOnIOThread,
                               base::Unretained(this), registration_->id()));
  FindRegistrationForId(registration_->id(), registration_->scope().GetOrigin(),
                        blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Install) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/worker.js",
                    blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Fulfilled) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/worker_install_fulfilled.js",
                    blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithFetchHandler) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/fetch_event.js",
                    blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithoutFetchHandler) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/worker.js",
                    blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST,
            version_->fetch_handler_existence());
}

// Check that ServiceWorker script requests set a "Service-Worker: script"
// header.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ServiceWorkerScriptHeader) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifyServiceWorkerHeaderInRequest));
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/generated_sw.js",
                    blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       Activate_NoEventListener) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/worker.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Activate_Rejected) {
  StartServerAndNavigateToSetup();
  InstallTestHelper("/service_worker/worker_activate_rejected.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Rejected) {
  StartServerAndNavigateToSetup();
  InstallTestHelper(
      "/service_worker/worker_install_rejected.js",
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_RejectConsoleMessage) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker_install_rejected.js"));

  ConsoleListener console_listener;
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                               base::Unretained(version_->embedded_worker()),
                               &console_listener));

  // Dispatch install on a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop install_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::InstallOnIOThread, base::Unretained(this),
                     install_run_loop.QuitClosure(), &status));
  install_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected,
            status.value());

  const base::string16 expected =
      base::ASCIIToUTF16("Rejecting oninstall event");
  console_listener.WaitForConsoleMessages(1);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected));
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
                               base::Unretained(version_->embedded_worker()),
                               &console_listener));
}

// Tests starting an installed classic service worker while offline.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       StartInstalledClassicScriptWhileOffline) {
  StartServerAndNavigateToSetup();

  // Install a service worker.
  InstallTestHelper("/service_worker/worker_with_one_import.js",
                    blink::ServiceWorkerStatusCode::kOk,
                    blink::mojom::ScriptType::kClassic);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Emulate offline by stopping the test server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(embedded_test_server()->Started());

  // Restart the worker while offline.
  StartWorker(blink::ServiceWorkerStatusCode::kOk);
}

// Tests starting an installed module service worker while offline.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       StartInstalledModuleScriptWhileOffline) {
  StartServerAndNavigateToSetup();

  // Install a service worker.
  InstallTestHelper("/service_worker/static_import_worker.js",
                    blink::ServiceWorkerStatusCode::kOk,
                    blink::mojom::ScriptType::kModule);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Emulate offline by stopping the test server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(embedded_test_server()->Started());

  // Restart the worker while offline.
  StartWorker(blink::ServiceWorkerStatusCode::kOk);
}

class WaitForLoaded : public EmbeddedWorkerInstance::Listener {
 public:
  explicit WaitForLoaded(base::OnceClosure quit) : quit_(std::move(quit)) {}

  void OnScriptEvaluationStart() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(quit_);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(quit_));
  }

 private:
  base::OnceClosure quit_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutStartingWorker) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/while_true_worker.js"));

  // Start a worker, waiting until the script is loaded.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::RunLoop load_run_loop;
  WaitForLoaded wait_for_load(load_run_loop.QuitClosure());
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                               base::Unretained(version_->embedded_worker()),
                               &wait_for_load));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StartOnIOThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  load_run_loop.Run();
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
                               base::Unretained(version_->embedded_worker()),
                               &wait_for_load));

  // The script has loaded but start has not completed yet.
  ASSERT_FALSE(status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunOnIOThreadWithDelay(
      base::BindOnce(&self::TimeoutWorkerOnIOThread, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(100));
  start_run_loop.Run();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutWorkerInEvent) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(
      base::BindOnce(&self::SetUpRegistrationOnIOThread, base::Unretained(this),
                     "/service_worker/while_true_in_install_worker.js"));

  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::StartOnIOThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Dispatch an event.
  base::RunLoop install_run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&self::InstallOnIOThread, base::Unretained(this),
                     install_run_loop.QuitClosure(), &status));

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunOnIOThreadWithDelay(
      base::BindOnce(&self::TimeoutWorkerOnIOThread, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(100));
  install_run_loop.Run();

  // Terminating a worker, even one in an infinite loop, is treated as if
  // waitUntil was rejected in the renderer code.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected,
            status.value());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchEvent_Response) {
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  InstallTestHelper("/service_worker/fetch_event.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(301, response->status_code);
  EXPECT_EQ("Moved Permanently", response->status_text);
  base::flat_map<std::string, std::string> expected_headers;
  expected_headers["content-language"] = "fi";
  expected_headers["content-type"] = "text/html; charset=UTF-8";
  EXPECT_EQ(expected_headers, response->headers);

  std::string body;
  RunOnIOThread(base::BindOnce(&ReadResponseBody, &body,
                               base::Owned(blob_data_handle.release())));
  EXPECT_EQ("This resource is gone. Gone, gone, gone.", body);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_ResponseViaCache) {
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response1;
  blink::mojom::FetchAPIResponsePtr response2;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  const base::Time start_time(base::Time::Now());
  InstallTestHelper("/service_worker/fetch_event_response_via_cache.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  FetchOnRegisteredWorker(&result, &response1, &blob_data_handle);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(200, response1->status_code);
  EXPECT_EQ("OK", response1->status_text);
  EXPECT_TRUE(response1->response_time >= start_time);
  EXPECT_FALSE(response1->is_in_cache_storage);
  ASSERT_TRUE(response1->cache_storage_cache_name);
  EXPECT_EQ(std::string(), *response1->cache_storage_cache_name);

  FetchOnRegisteredWorker(&result, &response2, &blob_data_handle);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(200, response2->status_code);
  EXPECT_EQ("OK", response2->status_text);
  EXPECT_EQ(response1->response_time, response2->response_time);
  EXPECT_TRUE(response2->is_in_cache_storage);
  ASSERT_TRUE(response2->cache_storage_cache_name);
  EXPECT_EQ("cache_name", *response2->cache_storage_cache_name);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_respondWithRejection) {
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  InstallTestHelper("/service_worker/fetch_event_rejected.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  ConsoleListener console_listener;
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                               base::Unretained(version_->embedded_worker()),
                               &console_listener));

  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  const base::string16 expected1 = base::ASCIIToUTF16(
      "resulted in a network error response: the promise was rejected.");
  const base::string16 expected2 =
      base::ASCIIToUTF16("Uncaught (in promise) Rejecting respondWith promise");
  console_listener.WaitForConsoleMessages(2);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected1));
  ASSERT_EQ(0u, console_listener.messages()[1].find(expected2));
  RunOnIOThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
                               base::Unretained(version_->embedded_worker()),
                               &console_listener));

  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(0, response->status_code);

  ASSERT_FALSE(blob_data_handle);
}

// Tests that the browser cache is bypassed on update checks after 24 hours
// elapsed since the last update check that accessed network.
//
// Due to the nature of what this is testing, this test depends on the system
// clock being reasonable during the test. So it might break on daylight savings
// leap or something:
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/C3EvKPrb0XM/4Jv02SpNYncJ
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       UpdateBypassesCacheAfter24Hours) {
  const char kScope[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/update_worker.js";

  // Tell the server to return a new script for each `update_worker.js` request.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForUpdateWorker));
  StartServerAndNavigateToSetup();

  // Register a service worker.

  // Make options. Set to kAll so updating exercises the browser cache.
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kScope),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll);

  // Register and wait for activation.
  auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();
  int64_t registration_id = observer->registration_id();

  // The registration's last update time should be non-null.
  base::Time last_update_time = GetLastUpdateCheck(registration_id);
  EXPECT_NE(base::Time(), last_update_time);

  // Try to update. The request should hit the browser cache so no update should
  // be found.
  {
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    bool update_found = true;
    UpdateRegistration(registration_id, &status, &update_found);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_FALSE(update_found);
  }
  // The last update time should be unchanged.
  EXPECT_EQ(last_update_time, GetLastUpdateCheck(registration_id));

  // Set the last update time far in the past.
  {
    last_update_time = base::Time::Now() - base::TimeDelta::FromHours(24);
    base::RunLoop time_run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&self::SetLastUpdateCheckOnIOThread,
                       base::Unretained(this), registration_id,
                       last_update_time, time_run_loop.QuitClosure()));
    time_run_loop.Run();
  }

  // Try to update again. The browser cache should be bypassed so the update
  // should be found.
  {
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    bool update_found = false;
    UpdateRegistration(registration_id, &status, &update_found);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_TRUE(update_found);
  }
  // The last update time should be bumped.
  EXPECT_LT(last_update_time, GetLastUpdateCheck(registration_id));
}

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
                           WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  bool data_saver_enabled_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  InstallTestHelper("/service_worker/fetch_in_install.js",
                    blink::ServiceWorkerStatusCode::kOk);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       RequestWorkerScriptWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  InstallTestHelper("/service_worker/generated_sw.js",
                    blink::ServiceWorkerStatusCode::kOk);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithoutSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderNotInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  InstallTestHelper("/service_worker/fetch_in_install.js",
                    blink::ServiceWorkerStatusCode::kOk);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, FetchPageWithSaveData) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/add_save_data_to_title.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
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
  cross_origin_server.ServeFilesFromSourceDirectory("content/test/data");
  cross_origin_server.RegisterRequestHandler(
      base::Bind(&VerifySaveDataNotInAccessControlRequestHeader));
  ASSERT_TRUE(cross_origin_server.Start());

  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL(base::StringPrintf(
                    "%s?%s", kPageUrl,
                    cross_origin_server.GetURL("/cross_origin_request.html")
                        .spec()
                        .c_str())));
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
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));
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
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
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

  // This test is based on a new idle timer mechanism which is available only
  // when S13nServiceWorker or NetworkService is enabled.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    LOG(WARNING)
        << "This test requires NetworkService or ServiceWorkerServicification.";
    return;
  }

  // Register a service worker.
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](base::OnceClosure done, ServiceWorkerContextWrapper* wrapper,
             int64_t version_id) {
            ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
            version->endpoint()->SetIdleTimerDelayToZero();
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

  static void CancellingInterceptorCallback(
      const std::string& header,
      const std::string& value,
      int child_process_id,
      content::ResourceContext* resource_context,
      OnHeaderProcessedCallback callback) {
    DCHECK_EQ(kNavigationPreloadHeaderName, header);
    std::move(callback).Run(HeaderInterceptorResult::KILL);
  }

  void SetupForNavigationPreloadTest(const GURL& scope,
                                     const GURL& worker_url) {
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
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
    NavigateToURL(shell(), page_url);
    EXPECT_EQ(base::ASCIIToUTF16(expected_result),
              title_watcher.WaitAndGetTitle());
    return GetTextContent();
  }

  void RegisterMonitorRequestHandler() {
    embedded_test_server()->RegisterRequestMonitor(
        base::Bind(&self::MonitorRequestHandler, base::Unretained(this)));
  }

  void RegisterStaticFile(const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type) {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&self::StaticRequestHandler, base::Unretained(this),
                   relative_url, content, content_type));
  }

  void RegisterCustomResponse(const std::string& relative_url,
                              const std::string& response) {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&self::CustomRequestHandler, base::Unretained(this),
                   relative_url, response));
  }

  void RegisterKeepSearchRedirect(const std::string& relative_url,
                                  const std::string& redirect_location) {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&self::KeepSearchRedirectHandler, base::Unretained(this),
                   relative_url, redirect_location));
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
        base::Bind(&StoreString, &text_content, run_loop.QuitClosure()));
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

    void SendResponse(
        const net::test_server::SendBytesCallback& send,
        const net::test_server::SendCompleteCallback& done) override {
      send.Run(response_, done);
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl1));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  // When the navigation started, the navigation preload was not enabled yet.
  EXPECT_EQ("undefined", GetTextContent());
  ASSERT_EQ(0, GetRequestCount(kPageUrl1));

  const std::string kPageUrl2 = kPageUrl + "?change";
  const base::string16 title2 = base::ASCIIToUTF16("CHANGED");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  title_watcher2.AlsoWaitForTitle(base::ASCIIToUTF16("FROM_SERVER"));
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl2));
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl3));
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl4));
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
  NavigateToURL(shell(), page_url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(kNavigationPreloadNetworkError, GetTextContent());

  console_observer->WaitForConsoleMessages(1);
  const base::string16 expected =
      base::ASCIIToUTF16("net::ERR_CONNECTION_REFUSED");
  std::vector<base::string16> messages = console_observer->messages();
  EXPECT_NE(base::string16::npos, messages[0].find(expected));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       CanceledByInterceptor) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    // This is a test for a ResourceDispatcherHost interceptor cancelling the
    // Navigation Preload request. The analogue for the
    // NetworkService/ServiceWorker*Loader code path would be throttles, but
    // these don't see Navigation Preload (crbug.com/825717) and there is no
    // plan to allow them to, so just skip this test.

    // This has to be called so the EmbeddedTestServer IO Thread is created,
    // otherwise we crash on destruction.
    embedded_test_server()->StartAcceptingConnections();
    return;
  }

  auto console_observer =
      base::MakeRefCounted<ConsoleMessageContextObserver>(wrapper());
  console_observer->Init();

  content::ResourceDispatcherHost::Get()->RegisterInterceptor(
      kNavigationPreloadHeaderName, "",
      base::Bind(&CancellingInterceptorCallback));

  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  EXPECT_EQ(kNavigationPreloadAbortError,
            LoadNavigationPreloadTestPage(page_url, worker_url, "REJECTED"));

  console_observer->WaitForConsoleMessages(1);
  const base::string16 expected = base::ASCIIToUTF16("request was cancelled");
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

  std::unique_ptr<base::Value> result = base::JSONReader::Read(
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
                          base::IntToString(sizeof(kPage) - 1)));
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

  std::unique_ptr<base::Value> result = base::JSONReader::Read(
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
  NavigateToURL(shell(), redirect_page_url);
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?1"));

  // Navigate to a same-origin, in-scope URL that redirects to the target URL.
  // The navigation preload request should be the single request to the target
  // URL.
  const base::string16 title2 = base::ASCIIToUTF16("?2");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  NavigateToURL(shell(), in_scope_redirect_page_url);
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?2"));

  // Navigate to a cross-origin URL that redirects to the target URL. The
  // navigation preload request should be the single request to the target URL.
  const base::string16 title3 = base::ASCIIToUTF16("?3");
  TitleWatcher title_watcher3(shell()->web_contents(), title3);
  NavigateToURL(shell(), cross_origin_redirect_page_url);
  EXPECT_EQ(title3, title_watcher3.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?3"));
}

// When the content type of the page is not correctly set,
// OnStartLoadingResponseBody() of network::mojom::URLLoaderClient is called
// before OnReceiveResponse(). This behavior is caused by
// MimeSniffingResourceHandler. This test checks that even if the
// MimeSniffingResourceHandler is triggered navigation preload must be handled
// correctly.
IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       RespondWithNavigationPreloadWithMimeSniffing) {
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    // When S13nSW/NetworkService is enabled, we don't do MIME sniffing
    // (https://crbug.com/771118), so just skip this test. Also, this test was
    // meant to test an internal quirk of MimeSniffingResourceHandler, which
    // might not make sense in the NetworkService implementation anyway. If we
    // want a behavior test for MIME sniffing for navigation preload, it can be
    // an end-to-end layout test instead.

    // This has to be called so the EmbeddedTestServer IO Thread is created,
    // otherwise we crash on destruction.
    embedded_test_server()->StartAcceptingConnections();
    return;
  }

  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript = kEnableNavigationPreloadScript +
                              "self.addEventListener('fetch', event => {\n"
                              "    event.respondWith(event.preloadResponse);\n"
                              "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);

  // Setting an empty content type to trigger MimeSniffingResourceHandler.
  RegisterStaticFile(kPageUrl, kPage, "");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(page_url, worker_url);

  const base::string16 title = base::ASCIIToUTF16("PASS");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), page_url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ("Hello world.", GetTextContent());

  // The page request must be sent only once, since the worker responded with
  // the navigation preload response
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPSServiceWorkerIsMarkedAsSecure) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/fetch_event_blob.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  blink::mojom::ServiceWorkerRegistrationOptions options(
      https_server.GetURL(kPageUrl), blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  public_context()->RegisterServiceWorker(
      https_server.GetURL(kWorkerUrl), options,
      base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("Title");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), https_server.GetURL(kPageUrl));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_TRUE(https_server.GetCertificate()->EqualsExcludingChain(
      entry->GetSSL().certificate.get()));
  EXPECT_FALSE(net::IsCertStatusError(entry->GetSSL().cert_status));

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
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  base::string16 title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(kOKTitle, title);

  // Verify the number of resources in the implicit script cache is correct.
  const int kExpectedNumResources = 2;
  int num_resources = 0;
  RunOnIOThread(base::BindOnce(
      &CountScriptResources, base::Unretained(wrapper()),
      embedded_test_server()->GetURL(kScopeUrl), &num_resources));
  EXPECT_EQ(kExpectedNumResources, num_resources);
}

// An observer that waits for the version to stop.
class StopObserver : public ServiceWorkerVersion::Observer {
 public:
  explicit StopObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    if (version->running_status() == EmbeddedWorkerStatus::STOPPED) {
      DCHECK(quit_closure_);
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                               std::move(quit_closure_));
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, RendererCrash) {
  // Start a worker.
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));
  StartWorker(blink::ServiceWorkerStatusCode::kOk);

  // Crash the renderer process. The version should stop.
  base::RunLoop run_loop;
  StopObserver observer(run_loop.QuitClosure());
  RunOnIOThread(base::BindOnce(&ServiceWorkerVersion::AddObserver,
                               base::Unretained(version_.get()), &observer));
  shell()->web_contents()->GetMainFrame()->GetProcess()->Shutdown(
      content::RESULT_CODE_KILLED);
  run_loop.Run();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
  RunOnIOThread(base::BindOnce(&ServiceWorkerVersion::RemoveObserver,
                               base::Unretained(version_.get()), &observer));
}

class ServiceWorkerBlackBoxBrowserTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerBlackBoxBrowserTest;

  void FindRegistrationOnIO(const GURL& document_url,
                            blink::ServiceWorkerStatusCode* status,
                            base::OnceClosure continuation) {
    wrapper()->FindReadyRegistrationForDocument(
        document_url,
        base::BindOnce(
            &ServiceWorkerBlackBoxBrowserTest::DidFindRegistrationOnIO,
            base::Unretained(this), status, std::move(continuation)));
  }

  void DidFindRegistrationOnIO(
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
    RunOnIOThread(base::BindOnce(
        &ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO,
        base::Unretained(this),
        embedded_test_server()->GetURL("/service_worker/empty.html"), &status));
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound, status);
  }
}

class ServiceWorkerSitePerProcessTest : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerSitePerProcessTest() {}
  ~ServiceWorkerSitePerProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSitePerProcessTest);
};

// Times out on CrOS and Linux. https://crbug.com/702256
#if defined(ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_CrossSiteNavigation DISABLED_CrossSiteNavigation
#else
#define MAYBE_CrossSiteNavigation CrossSiteNavigation
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerSitePerProcessTest,
                       MAYBE_CrossSiteNavigation) {
  StartServerAndNavigateToSetup();
  // The first page registers a service worker.
  const char kRegisterPageUrl[] = "/service_worker/cross_site_xfer.html";
  const base::string16 kOKTitle1(base::ASCIIToUTF16("OK_1"));
  const base::string16 kFailTitle1(base::ASCIIToUTF16("FAIL_1"));
  content::TitleWatcher title_watcher1(shell()->web_contents(), kOKTitle1);
  title_watcher1.AlsoWaitForTitle(kFailTitle1);

  NavigateToURL(shell(), embedded_test_server()->GetURL(kRegisterPageUrl));
  ASSERT_EQ(kOKTitle1, title_watcher1.WaitAndGetTitle());

  // The second pages loads via the serviceworker including a subresource.
  const char kConfirmPageUrl[] =
      "/service_worker/cross_site_xfer_scope/"
      "cross_site_xfer_confirm_via_serviceworker.html";
  const base::string16 kOKTitle2(base::ASCIIToUTF16("OK_2"));
  const base::string16 kFailTitle2(base::ASCIIToUTF16("FAIL_2"));
  content::TitleWatcher title_watcher2(shell()->web_contents(), kOKTitle2);
  title_watcher2.AlsoWaitForTitle(kFailTitle2);

  NavigateToURL(shell(), embedded_test_server()->GetURL(kConfirmPageUrl));
  EXPECT_EQ(kOKTitle2, title_watcher2.WaitAndGetTitle());
}

class ServiceWorkerVersionBrowserV8FullCodeCacheTest
    : public ServiceWorkerVersionBrowserTest,
      public ServiceWorkerVersion::Observer {
 public:
  using self = ServiceWorkerVersionBrowserV8FullCodeCacheTest;
  ServiceWorkerVersionBrowserV8FullCodeCacheTest() = default;
  ~ServiceWorkerVersionBrowserV8FullCodeCacheTest() override {
    if (version_)
      version_->RemoveObserver(this);
  }
  void SetUpRegistrationAndListenerOnIOThread(const std::string& worker_url) {
    SetUpRegistrationOnIOThread(worker_url);
    version_->AddObserver(this);
  }
  void StartWorkerAndWaitUntilCachedMetadataUpdated(
      blink::ServiceWorkerStatusCode status) {
    DCHECK(!cache_updated_closure_);

    base::RunLoop run_loop;
    cache_updated_closure_ = run_loop.QuitClosure();

    // Start a worker.
    StartWorker(status);

    // Wait for the metadata to be stored. This run loop should finish when
    // OnCachedMetadataUpdated() is called.
    run_loop.Run();
  }
  size_t metadata_size() { return metadata_size_; };

 protected:
  // ServiceWorkerVersion::Observer overrides
  void OnCachedMetadataUpdated(ServiceWorkerVersion* version,
                               size_t size) override {
    DCHECK(cache_updated_closure_);
    metadata_size_ = size;
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             std::move(cache_updated_closure_));
  }

 private:
  base::OnceClosure cache_updated_closure_;
  size_t metadata_size_ = 0;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserV8FullCodeCacheTest,
                       FullCode) {
  StartServerAndNavigateToSetup();
  RunOnIOThread(base::BindOnce(&self::SetUpRegistrationAndListenerOnIOThread,
                               base::Unretained(this),
                               "/service_worker/worker.js"));

  StartWorkerAndWaitUntilCachedMetadataUpdated(
      blink::ServiceWorkerStatusCode::kOk);

  // The V8 code cache should be stored to the storage. It must have size
  // greater than 16 bytes.
  EXPECT_GT(static_cast<int>(metadata_size()), kV8CacheTimeStampDataSize);

  // Stop the worker.
  StopWorker();
}

class CacheStorageSideDataSizeChecker
    : public base::RefCountedThreadSafe<CacheStorageSideDataSizeChecker> {
 public:
  static int GetSize(CacheStorageContextImpl* cache_storage_context,
                     storage::FileSystemContext* file_system_context,
                     const GURL& origin,
                     const std::string& cache_name,
                     const GURL& url) {
    scoped_refptr<CacheStorageSideDataSizeChecker> checker(
        new CacheStorageSideDataSizeChecker(cache_storage_context,
                                            file_system_context, origin,
                                            cache_name, url));
    return checker->GetSizeImpl();
  }

 private:
  using self = CacheStorageSideDataSizeChecker;
  friend class base::RefCountedThreadSafe<self>;

  CacheStorageSideDataSizeChecker(
      CacheStorageContextImpl* cache_storage_context,
      storage::FileSystemContext* file_system_context,
      const GURL& origin,
      const std::string& cache_name,
      const GURL& url)
      : cache_storage_context_(cache_storage_context),
        file_system_context_(file_system_context),
        origin_(origin),
        cache_name_(cache_name),
        url_(url) {}

  ~CacheStorageSideDataSizeChecker() {}

  int GetSizeImpl() {
    int result = 0;
    RunOnIOThread(base::BindOnce(&self::OpenCacheOnIOThread, this, &result));
    return result;
  }

  void OpenCacheOnIOThread(int* result, base::OnceClosure continuation) {
    cache_storage_context_->cache_manager()->OpenCache(
        url::Origin::Create(origin_), CacheStorageOwner::kCacheAPI, cache_name_,
        base::BindOnce(&self::OnCacheStorageOpenCallback, this, result,
                       std::move(continuation)));
  }

  void OnCacheStorageOpenCallback(int* result,
                                  base::OnceClosure continuation,
                                  CacheStorageCacheHandle cache_handle,
                                  CacheStorageError error) {
    ASSERT_EQ(CacheStorageError::kSuccess, error);
    std::unique_ptr<ServiceWorkerFetchRequest> scoped_request(
        new ServiceWorkerFetchRequest());
    scoped_request->url = url_;
    CacheStorageCache* cache = cache_handle.value();
    cache->Match(
        std::move(scoped_request), nullptr,
        base::BindOnce(&self::OnCacheStorageCacheMatchCallback, this, result,
                       std::move(continuation), std::move(cache_handle)));
  }

  void OnCacheStorageCacheMatchCallback(
      int* result,
      base::OnceClosure continuation,
      CacheStorageCacheHandle cache_handle,
      CacheStorageError error,
      blink::mojom::FetchAPIResponsePtr response) {
    ASSERT_EQ(CacheStorageError::kSuccess, error);
    ASSERT_TRUE(response->blob);
    blink::mojom::BlobPtr blob_ptr(std::move(response->blob->blob));
    auto blob_handle =
        base::MakeRefCounted<storage::BlobHandle>(std::move(blob_ptr));
    blob_handle->get()->ReadSideData(base::BindOnce(
        [](scoped_refptr<storage::BlobHandle> blob_handle, int* result,
           base::OnceClosure continuation,
           const base::Optional<std::vector<uint8_t>>& data) {
          if (data)
            *result = data->size();
          std::move(continuation).Run();
        },
        blob_handle, result, std::move(continuation)));
  }

  CacheStorageContextImpl* cache_storage_context_;
  storage::FileSystemContext* file_system_context_;
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
  void RegisterAndActivateServiceWorker() {
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
    observer->Init();
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kPageUrl),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kWorkerUrl), options,
        base::BindOnce(&ExpectResultAndRun, true, base::DoNothing()));
    observer->Wait();
  }

  void NavigateToTestPage() {
    const base::string16 title =
        base::ASCIIToUTF16("Title was changed by the script.");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
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
        partition->GetFileSystemContext(), embedded_test_server()->base_url(),
        std::string("cache_name"), embedded_test_server()->GetURL(kScriptUrl));
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
    cross_origin_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(cross_origin_server_.Start());
    ServiceWorkerBrowserTest::SetUpOnMainThread();
  }

  void RegisterServiceWorkerOnCrossOriginServer(const std::string& scope,
                                                const std::string& script) {
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
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
    NavigateToURL(shell(),
                  embedded_test_server()->GetURL(
                      test_page + "?" +
                      cross_origin_server_.GetURL(cross_origin_url).spec()));
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

class HeaderInjectingThrottle : public URLLoaderThrottle {
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
  std::vector<std::unique_ptr<URLLoaderThrottle>> CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      ResourceContext* resource_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles;
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
  // This tests throttling behavior which only has an effect on service worker
  // interception when servicification is on.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    LOG(WARNING)
        << "This test requires NetworkService or ServiceWorkerServicification.";
    return;
  }

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
      base::JSONReader::Read(result.ExtractString()));
  ASSERT_TRUE(dict);

  // Default headers are present.
  EXPECT_TRUE(CheckHeader(*dict, "accept", network::kFrameAcceptHeader));
  // Injected headers are present.
  EXPECT_TRUE(CheckHeader(*dict, "x-injected", "injected value"));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that redirects by throttles occur before service worker interception.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       RedirectOccursBeforeFetchEvent) {
  // This tests throttling behavior which only has an effect on service worker
  // interception when servicification is on.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    LOG(WARNING)
        << "This test requires NetworkService or ServiceWorkerServicification.";
    return;
  }

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
  list.GetList().emplace_back(redirect_url.spec());
  EXPECT_EQ(list, EvalJs(shell()->web_contents()->GetMainFrame(), script));

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that the headers injected by throttles during navigation are
// present in the network request in the case of network fallback.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerURLLoaderThrottleTest,
    NavigationHasThrottledRequestHeadersAfterNetworkFallback) {
  // This tests throttling behavior which only has an effect on service worker
  // interception when servicification is on.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    LOG(WARNING)
        << "This test requires NetworkService or ServiceWorkerServicification.";
    return;
  }

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

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // The injected header should be present.
    EXPECT_EQ("injected value", EvalJs(shell()->web_contents()->GetMainFrame(),
                                       "document.body.textContent"));
  } else {
    // S13nServiceWorker: the injected header is not present. Throttle-modified
    // headers are not propagated to network requests through
    // ResourceDispatcherHost, because the legacy network code has its own code
    // path that applies the same throttling independent from the navigation's
    // URLLoaderThrottles.
    DCHECK(base::FeatureList::IsEnabled(
        blink::features::kServiceWorkerServicification));
    EXPECT_EQ("None", EvalJs(shell()->web_contents()->GetMainFrame(),
                             "document.body.textContent"));
  }

  SetBrowserClientForTesting(old_content_browser_client);
}

// Test that the headers injected by throttles during navigation are
// present in the navigation preload request.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       NavigationPreloadHasThrottledRequestHeaders) {
  // This tests throttling behavior which only has an effect on service worker
  // interception when servicification is on.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    LOG(WARNING)
        << "This test requires NetworkService or ServiceWorkerServicification.";
    return;
  }

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

}  // namespace content
