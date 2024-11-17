// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/test/blob_test_utils.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

using ::testing::Eq;
using ::testing::Pointee;

// V8ScriptRunner::setCacheTimeStamp() stores 16 byte data (marker + tag +
// timestamp).
const int kV8CacheTimeStampDataSize =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(double);

size_t BlobSideDataLength(blink::mojom::Blob* actual_blob) {
  size_t result = 0;
  base::RunLoop run_loop;
  actual_blob->ReadSideData(base::BindOnce(
      [](size_t* result, base::OnceClosure continuation,
         const std::optional<mojo_base::BigBuffer> data) {
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

void RunWithDelay(base::OnceClosure closure, base::TimeDelta delay) {
  base::RunLoop run_loop;
  GetUIThreadTaskRunner({})->PostDelayedTask(FROM_HERE,
                                             base::BindLambdaForTesting([&]() {
                                               std::move(closure).Run();
                                               run_loop.Quit();
                                             }),
                                             delay);
  run_loop.Run();
}

void ReceivePrepareResult(bool* is_prepared) {
  *is_prepared = true;
}

base::OnceClosure CreatePrepareReceiver(bool* is_prepared) {
  return base::BindOnce(&ReceivePrepareResult, is_prepared);
}

void ExpectRegisterResultAndRun(blink::ServiceWorkerStatusCode expected,
                                base::RepeatingClosure continuation,
                                blink::ServiceWorkerStatusCode actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

void ExpectUnregisterResultAndRun(
    blink::ServiceWorkerStatusCode expected_status,
    base::RepeatingClosure continuation,
    blink::ServiceWorkerStatusCode actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class WorkerActivatedObserver
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<WorkerActivatedObserver> {
 public:
  explicit WorkerActivatedObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}

  WorkerActivatedObserver(const WorkerActivatedObserver&) = delete;
  WorkerActivatedObserver& operator=(const WorkerActivatedObserver&) = delete;

  void Init() { context_->AddObserver(this); }
  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      run_loop_.Quit();
    }
  }
  void Wait() { run_loop_.Run(); }

  int64_t registration_id() { return registration_id_; }
  int64_t version_id() { return version_id_; }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override = default;

  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  base::RunLoop run_loop_;
  raw_ptr<ServiceWorkerContextWrapper> context_;
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

std::unique_ptr<ServiceWorkerVersion::MainScriptResponse>
CreateMainScriptResponse() {
  network::mojom::URLResponseHead response_head;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  response_head.headers =
      new net::HttpResponseHeaders(std::string(data, std::size(data)));
  return std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
      response_head);
}

// Returns a unique script for each request, to test service worker update.
std::unique_ptr<net::test_server::HttpResponse> RequestHandlerForUpdateWorker(
    const net::test_server::HttpRequest& request) {
  static int counter = 0;

  if (request.relative_url != "/service_worker/update_worker.js")
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      base::StringPrintf("// empty script. counter = %d\n", counter++));
  http_response->set_content_type("text/javascript");
  // Use a large max-age to test the browser cache.
  http_response->AddCustomHeader("Cache-Control", "max-age=31536000");
  return http_response;
}

// Returns a unique script for each request, to test service worker update.
std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForBigWorkerScript(const net::test_server::HttpRequest& request) {
  static int counter = 0;

  if (request.relative_url != "/service_worker/update_worker.js")
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  std::string script =
      base::StringPrintf("// empty script. counter = %d\n// ", counter++);
  script.resize(1E6, 'x');
  http_response->set_content(std::move(script));
  http_response->set_content_type("text/javascript");
  return http_response;
}

// Returns a response with Cross-Origin-Embedder-Policy header matching with
// |coep|.
std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForWorkerScriptWithCoep(
    std::optional<network::mojom::CrossOriginEmbedderPolicyValue> coep,
    const net::test_server::HttpRequest& request) {
  static int counter = 0;
  if (request.relative_url != "/service_worker/generated")
    return nullptr;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(
      base::StringPrintf("// empty. counter = %d\n", counter++));
  response->set_content_type("text/javascript");
  if (coep.has_value()) {
    std::string header_value =
        coep.value() == network::mojom::CrossOriginEmbedderPolicyValue::kNone
            ? "none"
            : "require-corp";
    response->AddCustomHeader("Cross-Origin-Embedder-Policy", header_value);
  }
  return response;
}

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyRequireCorp() {
  network::CrossOriginEmbedderPolicy out;
  out.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  return out;
}

}  // namespace

class ConsoleListener : public EmbeddedWorkerInstance::Listener {
 public:
  void OnReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const std::u16string& message,
                              int line_number,
                              const GURL& source_url) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    messages_.push_back(message);
    if (messages_.size() == expected_message_count_) {
      DCHECK(quit_);
      std::move(quit_).Run();
    }
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

  const std::vector<std::u16string>& messages() const { return messages_; }

 private:
  std::vector<std::u16string> messages_;
  size_t expected_message_count_ = 0;
  base::OnceClosure quit_;
};

class ServiceWorkerVersionBrowserTest : public ContentBrowserTest {
 public:
  using self = ServiceWorkerVersionBrowserTest;

  ~ServiceWorkerVersionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext()));
  }

  void TearDownOnMainThread() override {
    wrapper_.reset();
    service_worker_client_keep_alive_.clear();
  }

  blink::ServiceWorkerStatusCode Install(
      const std::string& worker_url,
      blink::mojom::ScriptType script_type =
          blink::mojom::ScriptType::kClassic) {
    SetUpRegistrationWithScriptType(worker_url, script_type);
    blink::ServiceWorkerStatusCode status;

    // Start the worker.
    base::RunLoop start_run_loop;
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::INSTALL,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode start_status) {
              ASSERT_EQ(start_status, blink::ServiceWorkerStatusCode::kOk);
              start_run_loop.Quit();
            }));
    start_run_loop.Run();

    // Dispatch install event.
    base::RunLoop install_run_loop;
    DispatchInstallEvent(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode install_status) {
          status = install_status;
          install_run_loop.Quit();
        }));
    install_run_loop.Run();

    // Stop the worker.
    base::RunLoop stop_run_loop;
    StopWorker(stop_run_loop.QuitClosure());
    stop_run_loop.Run();

    return status;
  }

  blink::ServiceWorkerStatusCode Activate() {
    blink::ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::ACTIVATE,
        base::BindOnce(&ServiceWorkerVersionBrowserTest::DispatchActivateEvent,
                       base::Unretained(this),
                       base::BindLambdaForTesting(
                           [&](blink::ServiceWorkerStatusCode actual_status) {
                             status = actual_status;
                             run_loop.Quit();
                           })));
    run_loop.Run();
    return status;
  }

  void FetchOnRegisteredWorker(
      const std::string& path,
      ServiceWorkerFetchDispatcher::FetchEventResult* result,
      blink::mojom::FetchAPIResponsePtr* response) {
    FetchOnRegisteredWorker(path, "", result, response);
  }

  void FetchOnRegisteredWorker(
      const std::string& path,
      const std::string& range_header,
      ServiceWorkerFetchDispatcher::FetchEventResult* result,
      blink::mojom::FetchAPIResponsePtr* response) {
    bool prepare_result = false;
    FetchResult fetch_result;
    fetch_result.status = blink::ServiceWorkerStatusCode::kErrorFailed;
    base::RunLoop fetch_run_loop;
    Fetch(fetch_run_loop.QuitClosure(), path, range_header, &prepare_result,
          &fetch_result);
    fetch_run_loop.Run();
    ASSERT_TRUE(prepare_result);
    *result = fetch_result.result;
    *response = std::move(fetch_result.response);
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, fetch_result.status);
  }

  void SetUpRegistration(const std::string& worker_url) {
    SetUpRegistrationWithScriptType(worker_url,
                                    blink::mojom::ScriptType::kClassic);
  }

  void SetUpRegistrationWithScriptType(const std::string& worker_url,
                                       blink::mojom::ScriptType script_type) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const GURL scope = embedded_test_server()->GetURL("/service_worker/");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    options.type = script_type;
    registration_ = CreateNewServiceWorkerRegistration(
        wrapper()->context()->registry(), options,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)));
    // Set the update check time to avoid triggering updates in the middle of
    // tests.
    registration_->set_last_update_check(base::Time::Now());

    version_ = CreateNewServiceWorkerVersion(
        wrapper()->context()->registry(), registration_.get(),
        embedded_test_server()->GetURL(worker_url), script_type);
    // Make the registration findable via storage functions.
    wrapper()->context()->registry()->NotifyInstallingRegistration(
        registration_.get());
  }

  void TimeoutWorker() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(version_->timeout_timer_.IsRunning());
    version_->SimulatePingTimeoutForTesting();
  }

  void AddControllee() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const GURL url = embedded_test_server()->GetURL("/service_worker/host");
    ScopedServiceWorkerClient service_worker_client =
        CreateServiceWorkerClient(wrapper()->context(), url);
    service_worker_client->SetControllerRegistration(
        registration_, false /* notify_controllerchange */);
    service_worker_client_keep_alive_.push_back(
        std::move(service_worker_client));
  }

  void AddWaitingWorker(const std::string& worker_url) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scoped_refptr<ServiceWorkerVersion> waiting_version(
        CreateNewServiceWorkerVersion(
            wrapper()->context()->registry(), registration_.get(),
            embedded_test_server()->GetURL(worker_url),
            blink::mojom::ScriptType::kClassic));
    waiting_version->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    waiting_version->SetStatus(ServiceWorkerVersion::INSTALLED);
    registration_->SetWaitingVersion(waiting_version.get());
    registration_->ActivateWaitingVersionWhenReady();
  }

  blink::ServiceWorkerStatusCode StartWorker() {
    base::RunLoop run_loop;
    blink::ServiceWorkerStatusCode status;
    StartWorkerAsync(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode actual_status) {
          status = actual_status;
          run_loop.Quit();
        }));
    run_loop.Run();
    return status;
  }

  void StopWorker() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::RunLoop stop_run_loop;
    StopWorker(stop_run_loop.QuitClosure());
    stop_run_loop.Run();
  }

  void StoreRegistration(int64_t version_id,
                         blink::ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::RunLoop run_loop;
    blink::ServiceWorkerStatusCode status;
    ServiceWorkerVersion* version =
        wrapper()->context()->GetLiveVersion(version_id);
    wrapper()->context()->registry()->StoreRegistration(
        registration_.get(), version,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode actual_status) {
              ASSERT_EQ(actual_status, expected_status);
              status = actual_status;
              run_loop.Quit();
            }));
    run_loop.Run();

    wrapper()->context()->registry()->NotifyDoneInstallingRegistration(
        registration_.get(), version_.get(), status);
  }

  void UpdateRegistration(int64_t registration_id,
                          blink::ServiceWorkerStatusCode* out_status,
                          bool* out_update_found) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    scoped_refptr<ServiceWorkerRegistration> registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    wrapper()->context()->UpdateServiceWorker(
        registration.get(), false /* force_bypass_cache */,
        false /* skip_script_comparison */,
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status,
                                       const std::string& message,
                                       int64_t registration_id) {
          ASSERT_EQ(wrapper()->context()->GetLiveRegistration(registration_id),
                    registration);
          *out_status = status;
          *out_update_found = !!registration->installing_version();
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForId(
      int64_t id,
      const blink::StorageKey& key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    base::RunLoop run_loop;
    wrapper()->context()->registry()->FindRegistrationForId(
        id, key,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode actual_status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              status = actual_status;
              run_loop.Quit();
            }));
    run_loop.Run();
    return status;
  }

  void StartWorkerAsync(
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>
          callback) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    version_->SetMainScriptResponse(CreateMainScriptResponse());
    version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                          std::move(callback));
  }

  void DispatchInstallEvent(
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>
          callback) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_EQ(blink::EmbeddedWorkerStatus::kRunning,
              version_->running_status());
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);

    auto callback_pair = base::SplitOnceCallback(std::move(callback));
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::INSTALL,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode error_status) {
              std::move(callback_pair.first).Run(error_status);
            }));
    version_->endpoint()->DispatchInstallEvent(base::BindOnce(
        &ServiceWorkerVersionBrowserTest::ReceiveInstallEvent,
        base::Unretained(this), std::move(callback_pair.second), request_id));
  }

  void ReceiveInstallEvent(ServiceWorkerVersion::StatusCallback callback,
                           int request_id,
                           blink::mojom::ServiceWorkerEventStatus status,
                           uint32_t fetch_count) {
    version_->FinishRequestWithFetchCount(
        request_id, status == blink::mojom::ServiceWorkerEventStatus::COMPLETED,
        fetch_count);

    if (callback) {
      std::move(callback).Run(
          mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));
    }
  }

  void Store(base::OnceClosure done,
             std::optional<blink::ServiceWorkerStatusCode>* result,
             int64_t version_id) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void DispatchActivateEvent(
      ServiceWorkerVersion::StatusCallback callback,
      blink::ServiceWorkerStatusCode start_worker_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, start_worker_status);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    registration_->SetActiveVersion(version_.get());
    // Using CreateSimpleEventCallback() causes `callback` to be
    // called when the event finishes successfully, even though
    // it is called the "error callback" to StartRequest().
    // TODO(crbug.com/40792768): Clean up the callback handling for
    // StartRequest().
    int request_id = version_->StartRequest(
        ServiceWorkerMetrics::EventType::ACTIVATE, std::move(callback));
    version_->endpoint()->DispatchActivateEvent(
        version_->CreateSimpleEventCallback(request_id));
  }

  void Fetch(base::OnceClosure done,
             const std::string& path,
             const std::string& range_header,
             bool* prepare_result,
             FetchResult* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    GURL url = embedded_test_server()->GetURL(path);
    network::mojom::RequestDestination destination =
        network::mojom::RequestDestination::kDocument;
    base::OnceClosure prepare_callback = CreatePrepareReceiver(prepare_result);
    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback =
        CreateResponseReceiver(std::move(done), result);
    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    request->method = "GET";
    if (!range_header.empty()) {
      request->headers[net::HttpRequestHeaders::kRange] = range_header;
    }
    fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
        std::move(request), destination, std::string() /* client_id */,
        std::string() /* resulting_client_id */, version_,
        std::move(prepare_callback), std::move(fetch_callback));
    fetch_dispatcher_->Run();
  }

  base::Time GetLastUpdateCheck(int64_t registration_id) {
    scoped_refptr<ServiceWorkerRegistration> registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    return registration->last_update_check();
  }

  void SetLastUpdateCheck(int64_t registration_id,
                          base::Time last_update_time) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scoped_refptr<ServiceWorkerRegistration> registration =
        wrapper()->context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    registration->set_last_update_check(last_update_time);
  }

  // Contrary to the style guide, the output parameter of this function comes
  // before input parameters so Bind can be used on it to create a FetchCallback
  // to pass to DispatchFetchEvent.
  void ReceiveFetchResult(
      base::OnceClosure quit,
      FetchResult* out_result,
      blink::ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchDispatcher::FetchEventResult actual_result,
      blink::mojom::FetchAPIResponsePtr actual_response,
      blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
      blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
      scoped_refptr<ServiceWorkerVersion> worker) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_TRUE(fetch_dispatcher_);
    fetch_dispatcher_.reset();
    out_result->status = actual_status;
    out_result->result = actual_result;
    out_result->response = std::move(actual_response);
    if (quit)
      std::move(quit).Run();
  }

  ServiceWorkerFetchDispatcher::FetchCallback CreateResponseReceiver(
      base::OnceClosure quit,
      FetchResult* result) {
    return base::BindOnce(&self::ReceiveFetchResult, base::Unretained(this),
                          std::move(quit), result);
  }

  void StopWorker(base::OnceClosure done) {
    ASSERT_TRUE(version_.get());
    version_->StopWorker(std::move(done));
  }

  void SetActiveVersion(ServiceWorkerRegistration* registration,
                        ServiceWorkerVersion* version) {
    version->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    version->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration->SetActiveVersion(version);
  }

  void SetResources(
      ServiceWorkerVersion* version,
      std::unique_ptr<
          std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>>
          resources) {
    version->script_cache_map()->resource_map_.clear();
    version->script_cache_map()->SetResources(*resources);
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

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::vector<ScopedServiceWorkerClient> service_worker_client_keep_alive_;
};

class WaitForLoaded : public EmbeddedWorkerInstance::Listener {
 public:
  explicit WaitForLoaded(base::OnceClosure quit) : quit_(std::move(quit)) {}

  void OnScriptEvaluationStart() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(quit_);
    std::move(quit_).Run();
  }

 private:
  base::OnceClosure quit_;
};

class MockContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  MockContentBrowserClient() : data_saver_enabled_(false) {}

  ~MockContentBrowserClient() override = default;

  void set_data_saver_enabled(bool enabled) { data_saver_enabled_ = enabled; }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override {
    return data_saver_enabled_;
  }

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  bool data_saver_enabled_;
};

// An observer that waits for the version to stop.
class StopObserver : public ServiceWorkerVersion::Observer {
 public:
  explicit StopObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    if (version->running_status() == blink::EmbeddedWorkerStatus::kStopped) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartAndStop) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/worker.js");

  // Start a worker.
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);

  // Stop the worker.
  base::RunLoop stop_run_loop;
  StopWorker(stop_run_loop.QuitClosure());
  stop_run_loop.Run();
}

// TODO(lunalu): remove this test when blink side use counter is removed
// (crbug.com/811948).
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       DropCountsOnBlinkUseCounter) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/worker.js");

  // Start a worker.
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);

  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));

  // Stop the worker.
  base::RunLoop stop_run_loop;
  StopWorker(stop_run_loop.QuitClosure());
  stop_run_loop.Run();
  // Expect no PageVisits count.
  EXPECT_EQ(nullptr, base::StatisticsRecorder::FindHistogram(
                         "Blink.UseCounter.Features"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartNotFound) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/nonexistent.js");

  // Start a worker for nonexistent URL.
  EXPECT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kErrorNetwork);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, ReadResourceFailure) {
  StartServerAndNavigateToSetup();

  // Create a registration with an active version.
  SetUpRegistration("/service_worker/worker.js");
  SetActiveVersion(registration_.get(), version_.get());

  // Add a non-existent resource to the version.
  auto records = std::make_unique<
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>>();
  records->push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      30, version_->script_url(), 100, /*sha256_checksum=*/""));
  SetResources(version_.get(), std::move(records));

  // Store the registration.
  StoreRegistration(version_->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the worker. We'll fail to read the resource.
  EXPECT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should be deleted from storage.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(registration_->id(), registration_->key()));
  EXPECT_TRUE(registration_->is_uninstalled());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ReadResourceFailure_WaitingWorker) {
  StartServerAndNavigateToSetup();
  // Create a registration and active version.
  ASSERT_EQ(Install("/service_worker/worker.js"),
            blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);
  ASSERT_TRUE(registration_->active_version());

  // Give the version a controllee.
  AddControllee();

  // Set a non-existent resource to the version.
  auto records1 = std::make_unique<
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>>();
  records1->push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      30, version_->script_url(), 100, /*sha256_checksum=*/""));
  SetResources(version_.get(), std::move(records1));

  // Make a waiting version and store it.
  AddWaitingWorker("/service_worker/worker.js");
  auto records2 = std::make_unique<
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>>();
  records2->push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      31, version_->script_url(), 100, /*sha256_checksum=*/""));
  SetResources(registration_->waiting_version(), std::move(records2));
  StoreRegistration(registration_->waiting_version()->version_id(),
                    blink::ServiceWorkerStatusCode::kOk);

  // Start the broken worker. We'll fail to read from disk and the worker should
  // be doomed.
  StopWorker();  // in case it's already running
  EXPECT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kErrorDiskCache);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The whole registration should be deleted from storage even though the
  // waiting version was not the broken one.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(registration_->id(), registration_->key()));
  EXPECT_TRUE(registration_->is_uninstalled());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Install) {
  StartServerAndNavigateToSetup();
  EXPECT_EQ(Install("/service_worker/worker.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Fulfilled) {
  StartServerAndNavigateToSetup();
  EXPECT_EQ(Install("/service_worker/worker_install_fulfilled.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithFetchHandler) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/fetch_event.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kNotSkippable,
            version_->fetch_handler_type());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithoutFetchHandler) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/worker.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kNoHandler,
            version_->fetch_handler_type());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallEmptyFetchHandler) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/empty_fetch_event.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler,
            version_->fetch_handler_type());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       NonFunctionFetchHandler) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/non_function_fetch_event.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kNotSkippable,
            version_->fetch_handler_type());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       NonFunctionFetchHandlerWithHandleEventProperty) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(
      Install("/service_worker/fetch_event_with_handle_event_property.js"),
      blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kNotSkippable,
            version_->fetch_handler_type());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       RemoveFetchEventListenersInGet) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/fetch_event_object_removing_itself.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            version_->fetch_handler_existence());
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerType::kNotSkippable,
            version_->fetch_handler_type());
}

// Check that fetch event handler added in the install event should result in a
// service worker that doesn't count as having a fetch event handler.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchHandlerSetInInstallEvent) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/fetch_event_set_in_install_event.js"),
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
  EXPECT_EQ(Install("/service_worker/generated_sw.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       Activate_NoEventListener) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/worker.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Activate_Rejected) {
  StartServerAndNavigateToSetup();
  ASSERT_EQ(Install("/service_worker/worker_activate_rejected.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(),
            blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Rejected) {
  StartServerAndNavigateToSetup();
  EXPECT_EQ(Install("/service_worker/worker_install_rejected.js"),
            blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_RejectConsoleMessage) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/worker_install_rejected.js");

  ConsoleListener console_listener;
  version_->embedded_worker()->AddObserver(&console_listener);

  // Start the worker.
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);

  // Dispatch install.
  base::RunLoop run_loop;
  DispatchInstallEvent(
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status,
                  blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
        run_loop.Quit();
      }));

  const std::u16string expected = u"Rejecting oninstall event";
  console_listener.WaitForConsoleMessages(1);
  run_loop.Run();

  ASSERT_NE(std::u16string::npos,
            console_listener.messages()[0].find(expected));
  version_->embedded_worker()->RemoveObserver(&console_listener);
}

// Tests starting an installed classic service worker while offline.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       StartInstalledClassicScriptWhileOffline) {
  StartServerAndNavigateToSetup();

  // Install a service worker.
  EXPECT_EQ(Install("/service_worker/worker_with_one_import.js",
                    blink::mojom::ScriptType::kClassic),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());

  // Emulate offline by stopping the test server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(embedded_test_server()->Started());

  // Restart the worker while offline.
  EXPECT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);
}

// Tests starting an installed module service worker while offline.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       StartInstalledModuleScriptWhileOffline) {
  StartServerAndNavigateToSetup();

  // Install a service worker.
  ASSERT_EQ(Install("/service_worker/static_import_worker.js",
                    blink::mojom::ScriptType::kModule),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());

  // Emulate offline by stopping the test server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(embedded_test_server()->Started());

  // Restart the worker while offline.
  EXPECT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutStartingWorker) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/while_true_worker.js");

  // Start a worker, waiting until the script is loaded.
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop start_run_loop;
  base::RunLoop load_run_loop;
  WaitForLoaded wait_for_load(load_run_loop.QuitClosure());
  version_->embedded_worker()->AddObserver(&wait_for_load);

  StartWorkerAsync(base::BindLambdaForTesting(
      [&](blink::ServiceWorkerStatusCode actual_status) {
        status = actual_status;
        start_run_loop.Quit();
      }));

  load_run_loop.Run();
  version_->embedded_worker()->RemoveObserver(&wait_for_load);

  // The script has loaded but start has not completed yet.
  ASSERT_FALSE(status);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunWithDelay(base::BindOnce(&self::TimeoutWorker, base::Unretained(this)),
               base::Milliseconds(100));
  start_run_loop.Run();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutWorkerInEvent) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/while_true_in_install_worker.js");

  // Start a worker.
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);

  // Dispatch an event.
  base::RunLoop install_run_loop;
  blink::ServiceWorkerStatusCode status;
  DispatchInstallEvent(base::BindLambdaForTesting(
      [&](blink::ServiceWorkerStatusCode actual_status) {
        status = actual_status;
        install_run_loop.Quit();
      }));

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  RunWithDelay(base::BindOnce(&self::TimeoutWorker, base::Unretained(this)),
               base::Milliseconds(100));
  install_run_loop.Run();

  // Terminating a worker, even one in an infinite loop, is treated as if
  // waitUntil was rejected in the renderer code.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected,
            status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchEvent_Response) {
  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
  ASSERT_EQ(Install("/service_worker/fetch_event.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);

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

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FetchEvent_ResponseNetwork DISABLED_FetchEvent_ResponseNetwork
#else
#define MAYBE_FetchEvent_ResponseNetwork FetchEvent_ResponseNetwork
#endif
// Tests for response type when a service worker does respondWith(fetch()).
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       MAYBE_FetchEvent_ResponseNetwork) {
  const char* kPath = "/service_worker/http_cache.html";

  StartServerAndNavigateToSetup();
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response1;
  blink::mojom::FetchAPIResponsePtr response2;
  ASSERT_EQ(Install("/service_worker/fetch_event_respond_with_fetch.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);

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
  ASSERT_EQ(Install("/service_worker/fetch_event_response_via_cache.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);

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
  ASSERT_EQ(Install("/service_worker/fetch_event_rejected.js"),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);

  ConsoleListener console_listener;
  version_->embedded_worker()->AddObserver(&console_listener);

  FetchOnRegisteredWorker("/service_worker/empty.html", &result, &response);
  const std::u16string expected1 =
      u"resulted in a network error response: the promise was rejected.";
  const std::u16string expected2 =
      u"Uncaught (in promise) Rejecting respondWith promise";
  console_listener.WaitForConsoleMessages(2);
  ASSERT_NE(std::u16string::npos,
            console_listener.messages()[0].find(expected1));
  ASSERT_EQ(0u, console_listener.messages()[1].find(expected2));
  version_->embedded_worker()->RemoveObserver(&console_listener);

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
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));

  // Register and wait for activation.
  auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
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
    last_update_time = base::Time::Now() - base::Hours(24);
    SetLastUpdateCheck(registration_id, last_update_time);
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
      embedded_test_server()->GetURL(kScope), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Regression test for https://crbug.com/1032517.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       UpdateWithScriptLargerThanMojoDataPipeBuffer) {
  const char kScope[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/update_worker.js";

  // Tell the server to return a new script for each `update_worker.js` request.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&RequestHandlerForBigWorkerScript));
  StartServerAndNavigateToSetup();

  // Register a service worker and wait for activation.
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kScope),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer->Wait();
  int64_t registration_id = observer->registration_id();

  // Try to update. Update should have succeeded.
  {
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    bool update_found = true;
    UpdateRegistration(registration_id, &status, &update_found);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_TRUE(update_found);
  }

  // Tidy up.
  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kScope), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  shell()->web_contents()->OnWebPreferencesChanged();
  EXPECT_EQ(Install("/service_worker/fetch_in_install.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       RequestWorkerScriptWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  shell()->web_contents()->OnWebPreferencesChanged();
  EXPECT_EQ(Install("/service_worker/generated_sw.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithoutSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataHeaderNotInRequest));
  StartServerAndNavigateToSetup();
  MockContentBrowserClient content_browser_client;
  EXPECT_EQ(Install("/service_worker/fetch_in_install.js"),
            blink::ServiceWorkerStatusCode::kOk);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, RendererCrash) {
  // Start a worker.
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/worker.js");
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);

  // Crash the renderer process. The version should stop.
  RenderProcessHost* process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  base::RunLoop run_loop;
  StopObserver observer(run_loop.QuitClosure());
  version_->AddObserver(&observer);
  process->Shutdown(content::RESULT_CODE_KILLED);
  run_loop.Run();
  process_watcher.Wait();

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
  version_->RemoveObserver(&observer);
}

// Checks if ServiceWorkerVersion has the correct value of COEP when a new
// worker is installed.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       CrossOriginEmbedderPolicyValue_Install) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &RequestHandlerForWorkerScriptWithCoep,
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp));
  StartServerAndNavigateToSetup();

  // The version cannot get the proper COEP value until the worker is started.
  SetUpRegistration("/service_worker/generated");
  EXPECT_FALSE(version_->cross_origin_embedder_policy());

  // Once it's started, the worker script is read from the network and the COEP
  // value is set to the version.
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);
  EXPECT_THAT(version_->cross_origin_embedder_policy(),
              Pointee(Eq(CrossOriginEmbedderPolicyRequireCorp())));
}

// Tests that JS can be executed in the context of a running service worker.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ExecuteScriptForTesting) {
  StartServerAndNavigateToSetup();
  SetUpRegistration("/service_worker/execute_script_worker.js");
  ASSERT_EQ(StartWorker(), blink::ServiceWorkerStatusCode::kOk);
  ASSERT_TRUE(version_);

  auto execute_script_helper = [this](const std::string& script,
                                      base::Value* value_out,
                                      std::string* error_out) {
    base::RunLoop run_loop;
    auto callback = [&run_loop, value_out, error_out](
                        base::Value value,
                        const std::optional<std::string>& error) {
      *value_out = std::move(value);
      *error_out = error.value_or("<no error>");
      run_loop.Quit();
    };

    version_->ExecuteScriptForTest(script,
                                   base::BindLambdaForTesting(callback));
    run_loop.Run();
  };

  {
    base::Value value;
    std::string error;
    execute_script_helper("self.workerFlag;", &value, &error);
    EXPECT_THAT(value, base::test::IsJson(R"("worker flag")"));
    EXPECT_EQ("<no error>", error);
  }
  {
    base::Value value;
    std::string error;
    // Execute a script that will hit an exception.
    execute_script_helper("foo = bar + baz;", &value, &error);
    EXPECT_TRUE(value.is_none());
    EXPECT_EQ("Uncaught ReferenceError: bar is not defined", error);
  }
  {
    base::Value value;
    std::string error;
    // Execute a script that evaluates to undefined. This converts to an empty
    // base::Value, and should not throw an error.
    execute_script_helper("(function() { })();", &value, &error);
    EXPECT_TRUE(value.is_none());
    EXPECT_EQ("<no error>", error);
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
  void SetUpRegistrationAndListener(const std::string& worker_url) {
    SetUpRegistration(worker_url);
    version_->AddObserver(this);
  }
  void StartWorkerAndWaitUntilCachedMetadataUpdated(
      blink::ServiceWorkerStatusCode status) {
    DCHECK(!cache_updated_closure_);

    base::RunLoop run_loop;
    cache_updated_closure_ = run_loop.QuitClosure();

    // Start a worker.
    ASSERT_EQ(StartWorker(), status);

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
    std::move(cache_updated_closure_).Run();
  }

 private:
  base::OnceClosure cache_updated_closure_;
  size_t metadata_size_ = 0;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserV8FullCodeCacheTest,
                       FullCode) {
  StartServerAndNavigateToSetup();
  SetUpRegistrationAndListener("/service_worker/worker.js");

  StartWorkerAndWaitUntilCachedMetadataUpdated(
      blink::ServiceWorkerStatusCode::kOk);

  // The V8 code cache should be stored to the storage. It must have size
  // greater than 16 bytes.
  EXPECT_GT(static_cast<int>(metadata_size()), kV8CacheTimeStampDataSize);

  // Stop the worker.
  StopWorker();
}

class CacheStorageEagerReadingTest : public ServiceWorkerVersionBrowserTest {
 public:
  void SetupServiceWorkerAndDoFetch(
      const std::string& fetch_url,
      blink::mojom::FetchAPIResponsePtr* response_out) {
    SetupServiceWorkerAndDoFetch(fetch_url, "", response_out);
  }

  void SetupServiceWorkerAndDoFetch(
      const std::string& fetch_url,
      const std::string& range_header,
      blink::mojom::FetchAPIResponsePtr* response_out) {
    StartServerAndNavigateToSetup();
    ASSERT_EQ(Install("/service_worker/cached_fetch_event.js"),
              blink::ServiceWorkerStatusCode::kOk);
    ASSERT_EQ(Activate(), blink::ServiceWorkerStatusCode::kOk);

    ServiceWorkerFetchDispatcher::FetchEventResult result;
    FetchOnRegisteredWorker(fetch_url, range_header, &result, response_out);
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
};

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingTest,
                       CacheMatchInRelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kCacheMatchURL, &response);
  ExpectEagerlyReadCacheResponse(std::move(response));
}

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingTest,
                       CacheMatchInUnrelatedFetchEvent) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kOtherURL, &response);
  ExpectNormalCacheResponse(std::move(response));
}

IN_PROC_BROWSER_TEST_F(CacheStorageEagerReadingTest,
                       CacheMatchInRelatedFetchEventWithRangeRequest) {
  blink::mojom::FetchAPIResponsePtr response;
  SetupServiceWorkerAndDoFetch(kCacheMatchURL, "bytes=0-8", &response);
  EXPECT_TRUE(response);
  ExpectNormalCacheResponse(std::move(response));
}

}  // namespace content
