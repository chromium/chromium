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
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
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
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
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
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_info.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/blob_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-test-utils.h"
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

size_t BlobSideDataLength(blink::mojom::Blob* actual_blob) {
  size_t result = 0;
  base::RunLoop run_loop;
  actual_blob->ReadSideData(base::BindOnce(
      [](size_t* result, base::OnceClosure continuation,
         const base::Optional<mojo_base::BigBuffer> data) {
        *result = data ? data->size() : 0;
        std::move(continuation).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

struct FetchResult {
  blink::ServiceWorkerStatusCode status;
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
};

void RunOnCoreThreadWithDelay(base::OnceClosure closure,
                              base::TimeDelta delay) {
  base::RunLoop run_loop;
  base::PostDelayedTask(FROM_HERE, {ServiceWorkerContext::GetCoreThreadId()},
                        base::BindLambdaForTesting([&]() {
                          std::move(closure).Run();
                          run_loop.Quit();
                        }),
                        delay);
  run_loop.Run();
}

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
    RunOrPostTaskOnThread(FROM_HERE, run_quit_thread, std::move(quit));
}

ServiceWorkerStorage::FindRegistrationCallback CreateFindRegistrationReceiver(
    BrowserThread::ID run_quit_thread,
    base::OnceClosure quit,
    blink::ServiceWorkerStatusCode* status) {
  return base::BindOnce(&ReceiveFindRegistrationStatus, run_quit_thread,
                        std::move(quit), status);
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
    RunOnCoreThread(
        base::BindOnce(&WorkerActivatedObserver::InitOnCoreThread, this));
  }
  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      RunOrPostTaskOnThread(
          FROM_HERE, BrowserThread::UI,
          base::BindOnce(&WorkerActivatedObserver::Quit, this));
    }
  }
  void Wait() { run_loop_.Run(); }

  int64_t registration_id() { return registration_id_; }
  int64_t version_id() { return version_id_; }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override {}
  void InitOnCoreThread() { context_->AddObserver(this); }
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
  if (request.method == net::test_server::METHOD_OPTIONS &&
      network::features::ShouldEnableOutOfBlinkCorsForTesting()) {
    // In OOR-CORS mode, 'Save-Data' is not added to the CORS preflight request.
    // This is the desired behavior.
    auto it = request.headers.find("Save-Data");
    EXPECT_EQ(request.headers.end(), it);
  } else {
    // In the legacy code path, 'Save-Data' is (undesirably) added to the
    // preflight request. And in both code paths, 'Save-Data' is added to the
    // actual request, as expected.
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

net::HttpResponseInfo CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, base::size(data)));
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

class ConsoleListener : public EmbeddedWorkerInstance::Listener {
 public:
  void OnReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    RunOrPostTaskOnThread(
        FROM_HERE, BrowserThread::UI,
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

  void TearDownOnCoreThread() override {
    // Ensure that these resources are released while the core thread still
    // exists.
    registration_ = nullptr;
    version_ = nullptr;
    remote_endpoints_.clear();
  }

  void InstallTestHelper(const std::string& worker_url,
                         blink::ServiceWorkerStatusCode expected_status,
                         blink::mojom::ScriptType script_type =
                             blink::mojom::ScriptType::kClassic) {
    RunOnCoreThread(
        base::BindOnce(&self::SetUpRegistrationWithScriptTypeOnCoreThread,
                       base::Unretained(this), worker_url, script_type));

    // Dispatch install on a worker.
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop install_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::InstallOnCoreThread, base::Unretained(this),
                       install_run_loop.QuitClosure(), &status));
    install_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());

    // Stop the worker.
    base::RunLoop stop_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::StopOnCoreThread, base::Unretained(this),
                       stop_run_loop.QuitClosure()));
    stop_run_loop.Run();
  }

  void ActivateTestHelper(blink::ServiceWorkerStatusCode expected_status) {
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::ActivateOnCoreThread, base::Unretained(this),
                       run_loop.QuitClosure(), &status));
    run_loop.Run();
    ASSERT_EQ(expected_status, status.value());
  }

  void FetchOnRegisteredWorker(
      const std::string& path,
      ServiceWorkerFetchDispatcher::FetchEventResult* result,
      blink::mojom::FetchAPIResponsePtr* response) {
    bool prepare_result = false;
    FetchResult fetch_result;
    fetch_result.status = blink::ServiceWorkerStatusCode::kErrorFailed;
    base::RunLoop fetch_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::FetchOnCoreThread, base::Unretained(this),
                       fetch_run_loop.QuitClosure(), path, &prepare_result,
                       &fetch_result));
    fetch_run_loop.Run();
    ASSERT_TRUE(prepare_result);
    *result = fetch_result.result;
    *response = std::move(fetch_result.response);
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, fetch_result.status);
  }

  void SetUpRegistrationOnCoreThread(const std::string& worker_url) {
    SetUpRegistrationWithScriptTypeOnCoreThread(
        worker_url, blink::mojom::ScriptType::kClassic);
  }

  void SetUpRegistrationWithScriptTypeOnCoreThread(
      const std::string& worker_url,
      blink::mojom::ScriptType script_type) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
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

  void TimeoutWorkerOnCoreThread() {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    EXPECT_TRUE(version_->timeout_timer_.IsRunning());
    version_->SimulatePingTimeoutForTesting();
  }

  void AddControlleeOnCoreThread() {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    remote_endpoints_.emplace_back();
    base::WeakPtr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
        33 /* dummy render process id */, true /* is_parent_frame_secure */,
        wrapper()->context()->AsWeakPtr(), &remote_endpoints_.back());
    const GURL url = embedded_test_server()->GetURL("/service_worker/host");
    host->UpdateUrls(url, url, url::Origin::Create(url));
    host->SetControllerRegistration(registration_,
                                    false /* notify_controllerchange */);
  }

  void AddWaitingWorkerOnCoreThread(const std::string& worker_url) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
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
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::StartOnCoreThread, base::Unretained(this),
                       start_run_loop.QuitClosure(), &status));
    start_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());
  }

  void StopWorker() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::RunLoop stop_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::StopOnCoreThread, base::Unretained(this),
                       stop_run_loop.QuitClosure()));
    stop_run_loop.Run();
  }

  void StoreRegistration(int64_t version_id,
                         blink::ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop store_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::StoreOnCoreThread, base::Unretained(this),
                       store_run_loop.QuitClosure(), &status, version_id));
    store_run_loop.Run();
    ASSERT_EQ(expected_status, status.value());

    RunOnCoreThread(
        base::BindOnce(&self::NotifyDoneInstallingRegistrationOnCoreThread,
                       base::Unretained(this), status.value()));
  }

  void UpdateRegistration(int64_t registration_id,
                          blink::ServiceWorkerStatusCode* out_status,
                          bool* out_update_found) {
    base::RunLoop update_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(
            &self::UpdateOnCoreThread, base::Unretained(this), registration_id,
            base::BindOnce(
                &self::ReceiveUpdateResultOnCoreThread, base::Unretained(this),
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
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::FindRegistrationForIdOnCoreThread,
                       base::Unretained(this), run_loop.QuitClosure(), &status,
                       id, origin));
    run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void FindRegistrationForIdOnCoreThread(base::OnceClosure done,
                                         blink::ServiceWorkerStatusCode* result,
                                         int64_t id,
                                         const GURL& origin) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    wrapper()->context()->storage()->FindRegistrationForId(
        id, origin,
        CreateFindRegistrationReceiver(BrowserThread::UI, std::move(done),
                                       result));
  }

  void NotifyDoneInstallingRegistrationOnCoreThread(
      blink::ServiceWorkerStatusCode status) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    wrapper()->context()->storage()->NotifyDoneInstallingRegistration(
        registration_.get(), version_.get(), status);
  }

  void StartOnCoreThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    version_->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
    version_->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
  }

  void InstallOnCoreThread(
      const base::RepeatingClosure& done,
      base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::INSTALL,
        base::BindOnce(&self::DispatchInstallEventOnCoreThread,
                       base::Unretained(this), done, result));
  }

  void DispatchInstallEventOnCoreThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result,
      blink::ServiceWorkerStatusCode start_worker_status) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, start_worker_status);
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    auto repeating_done = base::AdaptCallbackForRepeating(std::move(done));
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::INSTALL,
        CreateReceiver(BrowserThread::UI, repeating_done, result));
    version_->endpoint()->DispatchInstallEvent(base::BindOnce(
        &self::ReceiveInstallEventOnCoreThread, base::Unretained(this),
        repeating_done, result, request_id));
  }

  void ReceiveInstallEventOnCoreThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* out_result,
      int request_id,
      blink::mojom::ServiceWorkerEventStatus status,
      bool has_fetch_handler) {
    version_->FinishRequest(
        request_id,
        status == blink::mojom::ServiceWorkerEventStatus::COMPLETED);
    version_->set_fetch_handler_existence(
        has_fetch_handler
            ? ServiceWorkerVersion::FetchHandlerExistence::EXISTS
            : ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);

    *out_result = mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status);
    if (!done.is_null())
      RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, std::move(done));
  }

  void StoreOnCoreThread(base::OnceClosure done,
                         base::Optional<blink::ServiceWorkerStatusCode>* result,
                         int64_t version_id) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ServiceWorkerVersion* version =
        wrapper()->context()->GetLiveVersion(version_id);
    wrapper()->context()->storage()->StoreRegistration(
        registration_.get(), version,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
  }

  void ActivateOnCoreThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::ACTIVATE,
        base::BindOnce(&self::DispatchActivateEventOnCoreThread,
                       base::Unretained(this), std::move(done), result));
  }

  void DispatchActivateEventOnCoreThread(
      base::OnceClosure done,
      base::Optional<blink::ServiceWorkerStatusCode>* result,
      blink::ServiceWorkerStatusCode start_worker_status) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, start_worker_status);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    registration_->SetActiveVersion(version_.get());
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::INSTALL,
        CreateReceiver(BrowserThread::UI, std::move(done), result));
    version_->endpoint()->DispatchActivateEvent(
        version_->CreateSimpleEventCallback(request_id));
  }

  void UpdateOnCoreThread(int registration_id,
                          ServiceWorkerContextCore::UpdateCallback callback) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    wrapper()->context()->UpdateServiceWorker(
        registration, false /* force_bypass_cache */,
        false /* skip_script_comparison */,
        blink::mojom::FetchClientSettingsObject::New(), std::move(callback));
  }

  void ReceiveUpdateResultOnCoreThread(
      const base::RepeatingClosure& done_on_ui,
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
    RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, done_on_ui);
  }

  void FetchOnCoreThread(base::OnceClosure done,
                         const std::string& path,
                         bool* prepare_result,
                         FetchResult* result) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    GURL url = embedded_test_server()->GetURL(path);
    ResourceType resource_type = ResourceType::kMainFrame;
    base::OnceClosure prepare_callback = CreatePrepareReceiver(prepare_result);
    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback =
        CreateResponseReceiver(std::move(done), result);
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    request->method = "GET";
    fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
        std::move(request), resource_type, std::string() /* client_id */,
        version_, std::move(prepare_callback), std::move(fetch_callback));
    fetch_dispatcher_->Run();
  }

  base::Time GetLastUpdateCheck(int64_t registration_id) {
    base::Time last_update_time;
    base::RunLoop time_run_loop;
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::GetLastUpdateCheckOnCoreThread,
                       base::Unretained(this), registration_id,
                       &last_update_time, time_run_loop.QuitClosure()));
    time_run_loop.Run();
    return last_update_time;
  }

  void GetLastUpdateCheckOnCoreThread(
      int64_t registration_id,
      base::Time* out_time,
      const base::RepeatingClosure& done_on_ui) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    *out_time = registration->last_update_check();
    RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, done_on_ui);
  }

  void SetLastUpdateCheckOnCoreThread(
      int64_t registration_id,
      base::Time last_update_time,
      const base::RepeatingClosure& done_on_ui) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ServiceWorkerRegistration* registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    registration->set_last_update_check(last_update_time);
    RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, done_on_ui);
  }

  // Contrary to the style guide, the output parameter of this function comes
  // before input parameters so Bind can be used on it to create a FetchCallback
  // to pass to DispatchFetchEvent.
  void ReceiveFetchResultOnCoreThread(
      base::OnceClosure quit,
      FetchResult* out_result,
      blink::ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchDispatcher::FetchEventResult actual_result,
      blink::mojom::FetchAPIResponsePtr actual_response,
      blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
      blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
      scoped_refptr<ServiceWorkerVersion> worker) {
    ASSERT_TRUE(
        BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId()));
    ASSERT_TRUE(fetch_dispatcher_);
    fetch_dispatcher_.reset();
    out_result->status = actual_status;
    out_result->result = actual_result;
    out_result->response = std::move(actual_response);
    if (!quit.is_null())
      RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, std::move(quit));
  }

  ServiceWorkerFetchDispatcher::FetchCallback CreateResponseReceiver(
      base::OnceClosure quit,
      FetchResult* result) {
    return base::BindOnce(&self::ReceiveFetchResultOnCoreThread,
                          base::Unretained(this), std::move(quit), result);
  }

  void StopOnCoreThread(base::OnceClosure done) {
    ASSERT_TRUE(version_.get());
    version_->StopWorker(std::move(done));
  }

  void SetActiveVersionOnCoreThread(ServiceWorkerRegistration* registration,
                                    ServiceWorkerVersion* version) {
    version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration->SetActiveVersion(version);
  }

  void SetResourcesOnCoreThread(
      ServiceWorkerVersion* version,
      const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources) {
    version->script_cache_map()->resource_map_.clear();
    version->script_cache_map()->SetResources(resources);
  }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartAndStop) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/worker.js"));

  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StartOnCoreThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Stop the worker.
  base::RunLoop stop_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StopOnCoreThread, base::Unretained(this),
                     stop_run_loop.QuitClosure()));
  stop_run_loop.Run();
}

// TODO(lunalu): remove this test when blink side use counter is removed
// (crbug.com/811948).
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       DropCountsOnBlinkUseCounter) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/worker.js"));
  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StartOnCoreThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));

  // Stop the worker.
  base::RunLoop stop_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StopOnCoreThread, base::Unretained(this),
                     stop_run_loop.QuitClosure()));
  stop_run_loop.Run();
  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartNotFound) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/nonexistent.js"));

  // Start a worker for nonexistent URL.
  StartWorker(blink::ServiceWorkerStatusCode::kErrorNetwork);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, ReadResourceFailure) {
  StartServerAndNavigateToSetup();

  // Create a registration with an active version.
  RunOnCoreThread(base::BindOnce(
      &ServiceWorkerVersionBrowserTest::SetUpRegistrationOnCoreThread,
      base::Unretained(this), "/service_worker/worker.js"));
  RunOnCoreThread(base::BindOnce(
      &ServiceWorkerVersionBrowserTest::SetActiveVersionOnCoreThread,
      base::Unretained(this), base::Unretained(registration_.get()),
      base::Unretained(version_.get())));

  // Add a non-existent resource to the version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> records = {
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100)};
  RunOnCoreThread(base::BindOnce(
      &ServiceWorkerVersionBrowserTest::SetResourcesOnCoreThread,
      base::Unretained(this), base::Unretained(version_.get()), records));

  // Store the registration.
  StoreRegistration(version_->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the worker. We'll fail to read the resource.
  StartWorker(blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should be deleted from storage.
  FindRegistrationForId(registration_->id(), registration_->scope().GetOrigin(),
                        blink::ServiceWorkerStatusCode::kErrorNotFound);
  EXPECT_TRUE(registration_->is_uninstalled());
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
  RunOnCoreThread(
      base::BindOnce(&self::AddControlleeOnCoreThread, base::Unretained(this)));

  // Set a non-existent resource to the version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> records = {
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100)};
  RunOnCoreThread(base::BindOnce(
      &ServiceWorkerVersionBrowserTest::SetResourcesOnCoreThread,
      base::Unretained(this), base::Unretained(version_.get()), records));

  // Make a waiting version and store it.
  RunOnCoreThread(base::BindOnce(&self::AddWaitingWorkerOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/worker.js"));
  records = {
      ServiceWorkerDatabase::ResourceRecord(31, version_->script_url(), 100)};
  RunOnCoreThread(base::BindOnce(
      &ServiceWorkerVersionBrowserTest::SetResourcesOnCoreThread,
      base::Unretained(this),
      base::Unretained(registration_->waiting_version()), records));
  StoreRegistration(registration_->waiting_version()->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the broken worker. We'll fail to read from disk and the worker should
  // be doomed.
  StopWorker();  // in case it's already running
  StartWorker(blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The whole registration should be deleted from storage even though the
  // waiting version was not the broken one.
  FindRegistrationForId(registration_->id(), registration_->scope().GetOrigin(),
                        blink::ServiceWorkerStatusCode::kErrorNotFound);
  EXPECT_TRUE(registration_->is_uninstalled());
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
      base::BindRepeating(&VerifyServiceWorkerHeaderInRequest));
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
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/worker_install_rejected.js"));

  ConsoleListener console_listener;
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                                 base::Unretained(version_->embedded_worker()),
                                 &console_listener));

  // Dispatch install on a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop install_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::InstallOnCoreThread, base::Unretained(this),
                     install_run_loop.QuitClosure(), &status));
  install_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected,
            status.value());

  const base::string16 expected =
      base::ASCIIToUTF16("Rejecting oninstall event");
  console_listener.WaitForConsoleMessages(1);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected));
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
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
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    DCHECK(quit_);
    RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI, std::move(quit_));
  }

 private:
  base::OnceClosure quit_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutStartingWorker) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/while_true_worker.js"));

  // Start a worker, waiting until the script is loaded.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::RunLoop load_run_loop;
  WaitForLoaded wait_for_load(load_run_loop.QuitClosure());
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                                 base::Unretained(version_->embedded_worker()),
                                 &wait_for_load));
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StartOnCoreThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  load_run_loop.Run();
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
                                 base::Unretained(version_->embedded_worker()),
                                 &wait_for_load));

  // The script has loaded but start has not completed yet.
  ASSERT_FALSE(status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunOnCoreThreadWithDelay(
      base::BindOnce(&self::TimeoutWorkerOnCoreThread, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(100));
  start_run_loop.Run();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutWorkerInEvent) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(
      &self::SetUpRegistrationOnCoreThread, base::Unretained(this),
      "/service_worker/while_true_in_install_worker.js"));

  // Start a worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::StartOnCoreThread, base::Unretained(this),
                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Dispatch an event.
  base::RunLoop install_run_loop;
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&self::InstallOnCoreThread, base::Unretained(this),
                     install_run_loop.QuitClosure(), &status));

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunOnCoreThreadWithDelay(
      base::BindOnce(&self::TimeoutWorkerOnCoreThread, base::Unretained(this)),
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
  InstallTestHelper("/service_worker/fetch_event.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  FetchOnRegisteredWorker("/service_worker/empty.html", &result, &response);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(301, response->status_code);
  EXPECT_EQ("Moved Permanently", response->status_text);
  // The response is created from blob, in which case we don't set the
  // response source for now.
  EXPECT_EQ(network::mojom::FetchResponseSource::kUnspecified,
            response->response_source);
  base::flat_map<std::string, std::string> expected_headers;
  expected_headers["content-language"] = "fi";
  expected_headers["content-type"] = "text/html; charset=UTF-8";
  EXPECT_EQ(expected_headers, response->headers);

  mojo::Remote<blink::mojom::Blob> blob(std::move(response->blob->blob));
  EXPECT_EQ("This resource is gone. Gone, gone, gone.",
            storage::BlobToString(blob.get()));
}

// Tests for response type when a service worker does respondWith(fetch()).
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_ResponseNetwork) {
  const char* kPath = "/service_worker/http_cache.html";

  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response1;
  blink::mojom::FetchAPIResponsePtr response2;
  InstallTestHelper("/service_worker/fetch_event_respond_with_fetch.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  // The first fetch() response should come from network.
  FetchOnRegisteredWorker(kPath, &result, &response1);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_FALSE(response1->cache_storage_cache_name.has_value());
  EXPECT_EQ(network::mojom::FetchResponseSource::kNetwork,
            response1->response_source);

  // The second fetch() response should come from HttpCache.
  FetchOnRegisteredWorker(kPath, &result, &response2);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(response1->status_code, response2->status_code);
  EXPECT_EQ(response1->status_text, response2->status_text);
  EXPECT_EQ(response1->response_time, response2->response_time);
  EXPECT_EQ(network::mojom::FetchResponseSource::kHttpCache,
            response2->response_source);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_ResponseViaCache) {
  const char* kPath = "/service_worker/empty.html";
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response1;
  blink::mojom::FetchAPIResponsePtr response2;
  InstallTestHelper("/service_worker/fetch_event_response_via_cache.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  // The first fetch() response should come from network.
  FetchOnRegisteredWorker(kPath, &result, &response1);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_FALSE(response1->cache_storage_cache_name.has_value());
  EXPECT_EQ(network::mojom::FetchResponseSource::kNetwork,
            response1->response_source);

  // The second fetch() response should come from CacheStorage.
  FetchOnRegisteredWorker(kPath, &result, &response2);
  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(response1->status_code, response2->status_code);
  EXPECT_EQ(response1->status_text, response2->status_text);
  EXPECT_EQ(response1->response_time, response2->response_time);
  EXPECT_EQ("cache_name", *response2->cache_storage_cache_name);
  EXPECT_EQ(network::mojom::FetchResponseSource::kCacheStorage,
            response2->response_source);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_respondWithRejection) {
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
  InstallTestHelper("/service_worker/fetch_event_rejected.js",
                    blink::ServiceWorkerStatusCode::kOk);
  ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

  ConsoleListener console_listener;
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::AddObserver,
                                 base::Unretained(version_->embedded_worker()),
                                 &console_listener));

  FetchOnRegisteredWorker("/service_worker/empty.html", &result, &response);
  const base::string16 expected1 = base::ASCIIToUTF16(
      "resulted in a network error response: the promise was rejected.");
  const base::string16 expected2 =
      base::ASCIIToUTF16("Uncaught (in promise) Rejecting respondWith promise");
  console_listener.WaitForConsoleMessages(2);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected1));
  ASSERT_EQ(0u, console_listener.messages()[1].find(expected2));
  RunOnCoreThread(base::BindOnce(&EmbeddedWorkerInstance::RemoveObserver,
                                 base::Unretained(version_->embedded_worker()),
                                 &console_listener));

  ASSERT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            result);
  EXPECT_EQ(0, response->status_code);

  EXPECT_FALSE(response->blob);
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
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(&self::SetLastUpdateCheckOnCoreThread,
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

  // Tidy up.
  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kScope),
      base::BindOnce(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
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
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
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
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
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
      base::BindRepeating(&VerifySaveDataHeaderNotInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  InstallTestHelper("/service_worker/fetch_in_install.js",
                    blink::ServiceWorkerStatusCode::kOk);
  SetBrowserClientForTesting(old_client);
}

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
          EXPECT_FALSE(params->url_request.trusted_params.has_value() &&
                       params->url_request.trusted_params->network_isolation_key
                           .IsFullyPopulated());
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
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
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
    auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
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
    auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
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
      ServiceWorkerContext* context,
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

// An observer that waits for the version to stop.
class StopObserver : public ServiceWorkerVersion::Observer {
 public:
  explicit StopObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    if (version->running_status() == EmbeddedWorkerStatus::STOPPED) {
      DCHECK(quit_closure_);
      RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                            std::move(quit_closure_));
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, RendererCrash) {
  // Start a worker.
  StartServerAndNavigateToSetup();
  RunOnCoreThread(base::BindOnce(&self::SetUpRegistrationOnCoreThread,
                                 base::Unretained(this),
                                 "/service_worker/worker.js"));
  StartWorker(blink::ServiceWorkerStatusCode::kOk);

  // Crash the renderer process. The version should stop.
  RenderProcessHost* process =
      shell()->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  base::RunLoop run_loop;
  StopObserver observer(run_loop.QuitClosure());
  RunOnCoreThread(base::BindOnce(&ServiceWorkerVersion::AddObserver,
                                 base::Unretained(version_.get()), &observer));
  process->Shutdown(content::RESULT_CODE_KILLED);
  run_loop.Run();
  process_watcher.Wait();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
  RunOnCoreThread(base::BindOnce(&ServiceWorkerVersion::RemoveObserver,
                                 base::Unretained(version_.get()), &observer));
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
  void SetUpRegistrationAndListenerOnCoreThread(const std::string& worker_url) {
    SetUpRegistrationOnCoreThread(worker_url);
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
  size_t metadata_size() { return metadata_size_; }

 protected:
  // ServiceWorkerVersion::Observer overrides
  void OnCachedMetadataUpdated(ServiceWorkerVersion* version,
                               size_t size) override {
    DCHECK(cache_updated_closure_);
    metadata_size_ = size;
    RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                          std::move(cache_updated_closure_));
  }

 private:
  base::OnceClosure cache_updated_closure_;
  size_t metadata_size_ = 0;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserV8FullCodeCacheTest,
                       FullCode) {
  StartServerAndNavigateToSetup();
  RunOnCoreThread(
      base::BindOnce(&self::SetUpRegistrationAndListenerOnCoreThread,
                     base::Unretained(this), "/service_worker/worker.js"));

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
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
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
  CacheStorageContextForBadOrigin() : CacheStorageContextImpl(nullptr) {}

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
  EXPECT_TRUE(CheckHeader(*dict, "accept",
                          std::string(network::kFrameAcceptHeader) +
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

class CacheStorageEagerReadingTestBase
    : public ServiceWorkerVersionBrowserTest {
 public:
  explicit CacheStorageEagerReadingTestBase(bool enabled) {
    if (enabled)
      feature_list.InitAndEnableFeature(features::kCacheStorageEagerReading);
    else
      feature_list.InitAndDisableFeature(features::kCacheStorageEagerReading);
  }

  void SetupServiceWorkerAndDoFetch(
      std::string fetch_url,
      blink::mojom::FetchAPIResponsePtr* response_out) {
    StartServerAndNavigateToSetup();
    InstallTestHelper("/service_worker/cached_fetch_event.js",
                      blink::ServiceWorkerStatusCode::kOk);
    ActivateTestHelper(blink::ServiceWorkerStatusCode::kOk);

    ServiceWorkerFetchDispatcher::FetchEventResult result;
    FetchOnRegisteredWorker(fetch_url, &result, response_out);
  }

  void ExpectNormalCacheResponse(blink::mojom::FetchAPIResponsePtr response) {
    EXPECT_EQ(network::mojom::FetchResponseSource::kCacheStorage,
              response->response_source);

    // A normal cache_storage response should have a blob for the body.
    mojo::Remote<blink::mojom::Blob> blob(std::move(response->blob->blob));

    // The blob should contain the expected body content.
    EXPECT_EQ(storage::BlobToString(blob.get()).length(), 1075u);

    // Since this js response was stored in the install event it should have
    // code cache stored in the blob side data.
    EXPECT_GT(BlobSideDataLength(blob.get()),
              static_cast<size_t>(kV8CacheTimeStampDataSize));
  }

  void ExpectEagerlyReadCacheResponse(
      blink::mojom::FetchAPIResponsePtr response) {
    EXPECT_EQ(network::mojom::FetchResponseSource::kCacheStorage,
              response->response_source);

    // An eagerly read cache_storage response should not have a blob.  Instead
    // the body is provided out-of-band in a mojo DataPipe.  The pipe is not
    // surfaced here in this test.
    EXPECT_FALSE(response->blob);

    // An eagerly read response should still have a side_data_blob, though.
    // This is provided so that js resources can still load code cache.
    mojo::Remote<blink::mojom::Blob> side_data_blob(
        std::move(response->side_data_blob->blob));

    // Since this js response was stored in the install event it should have
    // code cache stored in the blob side data.
    EXPECT_GT(BlobSideDataLength(side_data_blob.get()),
              static_cast<size_t>(kV8CacheTimeStampDataSize));
  }

  // The service worker script always matches against this URL.
  static constexpr const char* kCacheMatchURL =
      "/service_worker/v8_cache_test.js";

  // A URL that will be different from the cache.match() executed in
  // the service worker fetch handler.
  static constexpr const char* kOtherURL =
      "/service_worker/non-matching-url.js";

 private:
  base::test::ScopedFeatureList feature_list;
};

class CacheStorageEagerReadingEnabledTest
    : public CacheStorageEagerReadingTestBase {
 public:
  CacheStorageEagerReadingEnabledTest()
      : CacheStorageEagerReadingTestBase(true) {}
};

class CacheStorageEagerReadingDisabledTest
    : public CacheStorageEagerReadingTestBase {
 public:
  CacheStorageEagerReadingDisabledTest()
      : CacheStorageEagerReadingTestBase(false) {}
};

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingDisabledTest,
                       CacheMatchInRelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kCacheMatchURL, &response);
  ExpectNormalCacheResponse(std::move(response));
}

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingDisabledTest,
                       CacheMatchInUnrelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kOtherURL, &response);
  ExpectNormalCacheResponse(std::move(response));
}

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingEnabledTest,
                       CacheMatchInRelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kCacheMatchURL, &response);
  ExpectEagerlyReadCacheResponse(std::move(response));
}

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingEnabledTest,
                       CacheMatchInUnrelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kOtherURL, &response);
  ExpectNormalCacheResponse(std::move(response));
}

}  // namespace content
