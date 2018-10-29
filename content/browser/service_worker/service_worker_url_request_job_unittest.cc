// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"

namespace content {
namespace service_worker_url_request_job_unittest {

const int kProviderID = 100;
const char kTestData[] = "Here is sample text for the blob.";

// A simple ProtocolHandler implementation to create ServiceWorkerURLRequestJob.
//
// MockProtocolHandler is basically a mock of
// ServiceWorkerControlleeRequestHandler. In production code,
// ServiceWorkerControlleeRequestHandler::MaybeCreateJob() is called by
// ServiceWorkerRequestInterceptor, a custom URLRequestInterceptor, but for
// testing it's easier to make the job via ProtocolHandler.
class MockProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  MockProtocolHandler(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      const ResourceContext* resource_context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ServiceWorkerURLRequestJob::Delegate* delegate)
      : provider_host_(provider_host),
        resource_context_(resource_context),
        blob_storage_context_(blob_storage_context),
        job_(nullptr),
        delegate_(delegate),
        resource_type_(RESOURCE_TYPE_MAIN_FRAME),
        simulate_navigation_preload_(false) {}

  ~MockProtocolHandler() override = default;

  void set_resource_type(ResourceType type) { resource_type_ = type; }
  void set_simulate_navigation_preload() {
    simulate_navigation_preload_ = true;
  }

  // A simple version of
  // ServiceWorkerControlleeRequestHandler::MaybeCreateJob().
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (job_ && job_->ShouldFallbackToNetwork()) {
      // Simulate fallback to network by constructing a valid response.
      return new net::URLRequestTestJob(request, network_delegate,
                                        net::URLRequestTestJob::test_headers(),
                                        "PASS", true);
    }

    job_ = new ServiceWorkerURLRequestJob(
        request, network_delegate, provider_host_->client_uuid(),
        blob_storage_context_, resource_context_,
        network::mojom::FetchRequestMode::kNoCORS,
        network::mojom::FetchCredentialsMode::kOmit,
        network::mojom::FetchRedirectMode::kFollow,
        std::string() /* integrity */, false /* keepalive */, resource_type_,
        blink::mojom::RequestContextType::HYPERLINK,
        network::mojom::RequestContextFrameType::kTopLevel,
        scoped_refptr<network::ResourceRequestBody>(), delegate_);
    if (simulate_navigation_preload_) {
      job_->set_simulate_navigation_preload_for_test();
    }
    job_->ForwardToServiceWorker();
    return job_;
  }
  ServiceWorkerURLRequestJob* job() { return job_; }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const ResourceContext* resource_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  mutable ServiceWorkerURLRequestJob* job_;
  ServiceWorkerURLRequestJob::Delegate* delegate_;
  ResourceType resource_type_;
  bool simulate_navigation_preload_;
};

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
std::unique_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  return std::make_unique<storage::BlobProtocolHandler>(blob_storage_context);
}

base::flat_map<std::string, std::string> MakeHeaders() {
  base::flat_map<std::string, std::string> headers;
  headers["Pineapple"] = "Pen";
  headers["Foo"] = "Bar";
  headers["Set-Cookie"] = "CookieCookieCookie";
  return headers;
}

blink::mojom::FetchAPIResponsePtr MakeOkResponse() {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  return response;
}

void SaveStatusCallback(blink::ServiceWorkerStatusCode* out_status,
                        blink::ServiceWorkerStatusCode status) {
  *out_status = status;
}

// ServiceWorkerURLRequestJobTest is for testing the handling of URL requests by
// a service worker.
//
// To use it, call SetUpWithHelper() in your test. This sets up the service
// worker and the scaffolding to make the worker handle https URLRequests.  (Of
// course, no actual service worker runs in the unit test, it is simulated via
// EmbeddedWorkerTestHelper receiving IPC messages from the browser and
// responding as if a service worker is running in the renderer.) Example:
//
//    auto request = url_request_context_.CreateRequest(
//        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
//        &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
//    request->set_method("GET");
//    request->Start();
//    base::RunLoop().RunUntilIdle();
//    // Now the request was handled by a ServiceWorkerURLRequestJob.
//
// ServiceWorkerURLRequestJobTest is also a
// ServiceWorkerURLRequestJob::Delegate. In production code,
// ServiceWorkerControlleeRequestHandler is the Delegate (for non-"foreign
// fetch" request interceptions). So this class also basically mocks that part
// of ServiceWorkerControlleeRequestHandler.
class ServiceWorkerURLRequestJobTest
    : public testing::Test,
      public ServiceWorkerURLRequestJob::Delegate {
 public:
  MockProtocolHandler* handler() { return protocol_handler_; }

 protected:
  ServiceWorkerURLRequestJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerURLRequestJobTest() override {}

  void SetUp() override {
    // ServiceWorkerURLRequestJob is a non-S13nServiceWorker specific class
    // and we don't use it when S13nServiceWorker is enabled.
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kServiceWorkerServicification);
    browser_context_.reset(new TestBrowserContext);
    InitializeResourceContext(browser_context_.get());
  }

  void SetUpWithHelper(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_ = std::move(helper);
    helper_->context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // Prepare HTTP response info for the version.
    auto http_info = std::make_unique<net::HttpResponseInfo>();
    http_info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    EXPECT_TRUE(http_info->ssl_info.is_valid());
    http_info->ssl_info.security_bits = 0x100;
    // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    http_info->ssl_info.connection_status = 0x300039;
    http_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    // Create a registration and service worker version.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL("https://example.com/");
    registration_ = new ServiceWorkerRegistration(
        options, 1L, helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), GURL("https://example.com/service_worker.js"),
        blink::mojom::ScriptType::kClassic, 1L,
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheWithCustomResponseInfoSync(
        helper_->context()->storage(), version_->script_url(), 10,
        std::move(http_info), "I'm the body", "I'm the meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

    // Make the registration findable via storage functions.
    base::Optional<blink::ServiceWorkerStatusCode> status;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

    // Create a controlled client.
    std::unique_ptr<ServiceWorkerProviderHost> provider_host =
        CreateProviderHostForWindow(
            helper_->mock_render_process_id(), kProviderID,
            true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
            &remote_endpoint_);
    provider_host_ = provider_host->AsWeakPtr();
    provider_host->SetDocumentUrl(GURL("https://example.com/"));
    registration_->SetActiveVersion(version_);
    provider_host->SetControllerRegistration(
        registration_, false /* notify_controllerchange */);

    // Set up scaffolding for handling URL requests.
    ChromeBlobStorageContext* chrome_blob_storage_context =
        ChromeBlobStorageContext::GetFor(browser_context_.get());
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    storage::BlobStorageContext* blob_storage_context =
        chrome_blob_storage_context->context();
    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    std::unique_ptr<MockProtocolHandler> handler(new MockProtocolHandler(
        provider_host->AsWeakPtr(), browser_context_->GetResourceContext(),
        blob_storage_context->AsWeakPtr(), this));
    protocol_handler_ = handler.get();
    url_request_job_factory_->SetProtocolHandler("https", std::move(handler));
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context));
    url_request_context_.set_job_factory(url_request_job_factory_.get());

    helper_->context()->AddProviderHost(std::move(provider_host));
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    request_.reset();
  }

  void TestRequestResult(int expected_status_code,
                         const std::string& expected_status_text,
                         const std::string& expected_response,
                         bool expect_valid_ssl) {
    EXPECT_EQ(net::OK, url_request_delegate_.request_status());
    EXPECT_EQ(expected_status_code,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_status_text,
              request_->response_headers()->GetStatusText());
    EXPECT_EQ(expected_response, url_request_delegate_.data_received());
    const net::SSLInfo& ssl_info = request_->response_info().ssl_info;
    if (expect_valid_ssl) {
      EXPECT_TRUE(ssl_info.is_valid());
      EXPECT_EQ(ssl_info.security_bits, 0x100);
      EXPECT_EQ(ssl_info.connection_status, 0x300039);
    } else {
      EXPECT_FALSE(ssl_info.is_valid());
    }
  }

  void CheckHeaders(const net::HttpResponseHeaders* headers) {
    size_t iter = 0;
    std::string name;
    std::string value;
    EXPECT_TRUE(headers->EnumerateHeaderLines(&iter, &name, &value));
    EXPECT_EQ("Foo", name);
    EXPECT_EQ("Bar", value);
    EXPECT_TRUE(headers->EnumerateHeaderLines(&iter, &name, &value));
    EXPECT_EQ("Pineapple", name);
    EXPECT_EQ("Pen", value);
    EXPECT_TRUE(headers->EnumerateHeaderLines(&iter, &name, &value));
    EXPECT_EQ("Set-Cookie", name);
    EXPECT_EQ("CookieCookieCookie", value);
    EXPECT_FALSE(headers->EnumerateHeaderLines(&iter, &name, &value));
  }

  void TestRequest(int expected_status_code,
                   const std::string& expected_status_text,
                   const std::string& expected_response,
                   bool expect_valid_ssl) {
    request_ = url_request_context_.CreateRequest(
        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
        &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();
    TestRequestResult(expected_status_code, expected_status_text,
                      expected_response, expect_valid_ssl);
  }

  bool HasWork() { return !version_->HasNoWork(); }

  // Runs a request where the active worker starts a request in ACTIVATING state
  // and fails to reach ACTIVATED.
  void RunFailToActivateTest(ResourceType resource_type) {
    protocol_handler_->set_resource_type(resource_type);

    // Start a request with an activating worker.
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    request_ = url_request_context_.CreateRequest(
        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
        &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    request_->set_method("GET");
    request_->Start();

    // Proceed until the job starts waiting for the worker to activate.
    base::RunLoop().RunUntilIdle();

    // Simulate another worker kicking out the incumbent worker.  PostTask since
    // it might respond synchronously, and the TestDelegate would complain that
    // the message loop isn't being run.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerVersion::SetStatus, version_,
                                  ServiceWorkerVersion::REDUNDANT));
    base::RunLoop().RunUntilIdle();
  }

  // Starts a navigation request with navigation preload enabled.
  void SetUpNavigationPreloadTest(ResourceType resource_type) {
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    protocol_handler_->set_resource_type(resource_type);
    protocol_handler_->set_simulate_navigation_preload();
    request_ = url_request_context_.CreateRequest(
        GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
        &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    ResourceRequestInfo::AllocateForTesting(
        request_.get(), resource_type, browser_context_->GetResourceContext(),
        -1, -1, -1, resource_type == RESOURCE_TYPE_MAIN_FRAME, true, true,
        PREVIEWS_OFF, nullptr);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();
  }

  // ServiceWorkerURLRequestJob::Delegate -------------------------------------
  void OnPrepareToRestart() override { times_prepare_to_restart_invoked_++; }

  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    if (!provider_host_) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
      return nullptr;
    }
    if (!provider_host_->controller()) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION;
      return nullptr;
    }
    return provider_host_->controller();
  }

  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    if (!provider_host_) {
      *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
      return false;
    }
    return true;
  }

  void MainResourceLoadFailed() override {
    ASSERT_TRUE(provider_host_);
    // Detach the controller so subresource requests also skip the worker.
    provider_host_->NotifyControllerLost();
  }
  // ---------------------------------------------------------------------------

  // |scoped_feature_list_| must be before |thread_bundle_|.
  // See comments in ServiceWorkerProviderHostTest.
  base::test::ScopedFeatureList scoped_feature_list_;

  TestBrowserThreadBundle thread_bundle_;
  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;

  int times_prepare_to_restart_invoked_ = 0;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;

  // Not owned.
  // The ProtocolHandler for https requests, which creates a
  // ServiceWorkerURLRequestJob.
  MockProtocolHandler* protocol_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJobTest);
};

TEST_F(ServiceWorkerURLRequestJobTest, Simple) {
  SetUpWithHelper(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", std::string(), true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_TRUE(info->url_list_via_service_worker().empty());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
  EXPECT_FALSE(info->response_is_in_cache_storage());
  EXPECT_EQ(std::string(), info->response_cache_storage_cache_name());
}

// Helper for controlling when to start a worker and respond to a fetch event.
class DelayHelper : public EmbeddedWorkerTestHelper {
 public:
  DelayHelper(ServiceWorkerURLRequestJobTest* test)
      : EmbeddedWorkerTestHelper(base::FilePath()), test_(test) {}
  ~DelayHelper() override {}

  void CompleteNavigationPreload() {
    test_->handler()->job()->OnNavigationPreloadResponse();
  }

  void CompleteStartWorker() {
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id_, service_worker_version_id_, scope_, script_url_,
        pause_after_download_, std::move(start_worker_request_),
        std::move(controller_request_), std::move(start_worker_instance_host_),
        std::move(provider_info_), std::move(installed_scripts_info_));
  }

  void Respond() {
    response_callback_->OnResponse(
        MakeOkResponse(), blink::mojom::ServiceWorkerFetchEventTiming::New());
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

 protected:
  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerRequest service_worker_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    embedded_worker_id_ = embedded_worker_id;
    service_worker_version_id_ = service_worker_version_id;
    scope_ = scope;
    script_url_ = script_url;
    pause_after_download_ = pause_after_download;
    start_worker_request_ = std::move(service_worker_request);
    controller_request_ = std::move(controller_request);
    start_worker_instance_host_ = std::move(instance_host);
    provider_info_ = std::move(provider_info);
    installed_scripts_info_ = std::move(installed_scripts_info);
  }

  void OnFetchEvent(
      int embedded_worker_id,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    embedded_worker_id_ = embedded_worker_id;
    response_callback_ = std::move(response_callback);
    finish_callback_ = std::move(finish_callback);
    preload_handle_ = std::move(preload_handle);
  }

 private:
  int64_t service_worker_version_id_;
  GURL scope_;
  GURL script_url_;
  bool pause_after_download_;
  mojom::ServiceWorkerRequest start_worker_request_;
  mojom::ControllerServiceWorkerRequest controller_request_;
  mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo
      start_worker_instance_host_;
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info_;
  blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info_;
  int embedded_worker_id_ = 0;
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_;
  blink::mojom::FetchEventPreloadHandlePtr preload_handle_;
  mojom::ServiceWorker::DispatchFetchEventCallback finish_callback_;
  ServiceWorkerURLRequestJobTest* test_;
  DISALLOW_COPY_AND_ASSIGN(DelayHelper);
};

TEST_F(ServiceWorkerURLRequestJobTest,
       NavPreloadMetrics_WorkerAlreadyStarted_MainFrame) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  // Start the worker before the navigation.
  blink::ServiceWorkerStatusCode status =
      blink::ServiceWorkerStatusCode::kErrorFailed;
  base::HistogramTester histogram_tester;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::BindOnce(&SaveStatusCallback, &status));
  base::RunLoop().RunUntilIdle();
  helper->CompleteStartWorker();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);

  // Do the navigation.
  SetUpNavigationPreloadTest(RESOURCE_TYPE_MAIN_FRAME);
  helper->CompleteNavigationPreload();
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
      "NavigationPreloadEnabled",
      static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::RUNNING),
      1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", false, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "StartWorkerExistingProcess",
      0);
}

TEST_F(ServiceWorkerURLRequestJobTest,
       NavPreloadMetrics_WorkerFirst_MainFrame) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  base::HistogramTester histogram_tester;
  SetUpNavigationPreloadTest(RESOURCE_TYPE_MAIN_FRAME);

  // Worker finishes first.
  helper->CompleteStartWorker();
  helper->CompleteNavigationPreload();
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
      "NavigationPreloadEnabled",
      static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::
                           START_IN_EXISTING_READY_PROCESS),
      1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", false, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_WorkerStartOccurred",
      false, 1);
}

TEST_F(ServiceWorkerURLRequestJobTest,
       NavPreloadMetrics_NavPreloadFirst_MainFrame) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  base::HistogramTester histogram_tester;
  SetUpNavigationPreloadTest(RESOURCE_TYPE_MAIN_FRAME);

  // Nav preload finishes first.
  helper->CompleteNavigationPreload();
  helper->CompleteStartWorker();
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
      "NavigationPreloadEnabled",
      static_cast<int>(ServiceWorkerMetrics::WorkerPreparationType::
                           START_IN_EXISTING_READY_PROCESS),
      1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", true, 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_WorkerStartOccurred",
      true, 1);
}

TEST_F(ServiceWorkerURLRequestJobTest, NavPreloadMetrics_WorkerFirst_SubFrame) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  base::HistogramTester histogram_tester;
  SetUpNavigationPreloadTest(RESOURCE_TYPE_SUB_FRAME);

  // Worker finishes first.
  helper->CompleteStartWorker();
  helper->CompleteNavigationPreload();
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
      "NavigationPreloadEnabled",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "StartWorkerExistingProcess",
      0);
}

TEST_F(ServiceWorkerURLRequestJobTest,
       NavPreloadMetrics_NavPreloadFirst_SubFrame) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  base::HistogramTester histogram_tester;
  SetUpNavigationPreloadTest(RESOURCE_TYPE_SUB_FRAME);

  // Nav preload finishes first.
  helper->CompleteNavigationPreload();
  helper->CompleteStartWorker();
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
      "NavigationPreloadEnabled",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
      "StartWorkerExistingProcess",
      0);
}

class ProviderDeleteHelper : public EmbeddedWorkerTestHelper {
 public:
  ProviderDeleteHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~ProviderDeleteHelper() override {}

 protected:
  void OnFetchEvent(
      int /* embedded_worker_id */,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    context()->RemoveProviderHost(mock_render_process_id(), kProviderID);
    response_callback->OnResponse(
        MakeOkResponse(), blink::mojom::ServiceWorkerFetchEventTiming::New());
    std::move(finish_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProviderDeleteHelper);
};

// Shouldn't crash if the ProviderHost is deleted prior to completion of the
// fetch event.
TEST_F(ServiceWorkerURLRequestJobTest, DeletedProviderHostOnFetchEvent) {
  SetUpWithHelper(std::make_unique<ProviderDeleteHelper>());

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(500, "Service Worker Response Error", std::string(),
              false /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, DeletedProviderHostBeforeFetchEvent) {
  SetUpWithHelper(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

  request_->set_method("GET");
  request_->Start();
  helper_->context()->RemoveProviderHost(helper_->mock_render_process_id(),
                                         kProviderID);
  base::RunLoop().RunUntilIdle();
  TestRequestResult(500, "Service Worker Response Error", std::string(),
                    false /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_TRUE(info->service_worker_start_time().is_null());
  EXPECT_TRUE(info->service_worker_ready_time().is_null());
}

// Responds to fetch events with a blob.
class BlobResponder : public EmbeddedWorkerTestHelper {
 public:
  BlobResponder(const std::string& blob_uuid, uint64_t blob_size)
      : EmbeddedWorkerTestHelper(base::FilePath()),
        blob_uuid_(blob_uuid),
        blob_size_(blob_size) {}
  ~BlobResponder() override = default;

 protected:
  void OnFetchEvent(
      int /* embedded_worker_id */,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    blink::mojom::FetchAPIResponsePtr response = MakeOkResponse();
    response->headers = MakeHeaders();
    response->blob = blink::mojom::SerializedBlob::New();
    response->blob->uuid = blob_uuid_;
    response->blob->size = blob_size_;
    // As |response->blob->blob| must have a non-null value to be passed via
    // Mojo, we give it a dummy value.
    auto dummy_request = mojo::MakeRequest(&response->blob->blob);

    response_callback->OnResponse(
        std::move(response),
        blink::mojom::ServiceWorkerFetchEventTiming::New());
    std::move(finish_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

  std::string blob_uuid_;
  uint64_t blob_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, BlobResponse) {
  ChromeBlobStorageContext* blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context_.get());
  // Wait for blob_storage_context to finish initializing.
  base::RunLoop().RunUntilIdle();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);

  auto blob_data = std::make_unique<storage::BlobDataBuilder>("blob-id:myblob");
  for (int i = 0; i < 1024; ++i) {
    blob_data->AppendData(kTestData);
    expected_response += kTestData;
  }
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_storage_context->context()->AddFinishedBlob(std::move(blob_data));
  SetUpWithHelper(std::make_unique<BlobResponder>(blob_handle->uuid(),
                                                  expected_response.size()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", expected_response, true /* expect_valid_ssl */);
  CheckHeaders(request_->response_headers());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, NonExistentBlobUUIDResponse) {
  SetUpWithHelper(
      std::make_unique<BlobResponder>("blob-id:nothing-is-here", 0));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(500, "Service Worker Response Error", std::string(),
              true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

// Responds to fetch events with a stream.
class StreamResponder : public EmbeddedWorkerTestHelper {
 public:
  explicit StreamResponder(
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request,
      mojo::ScopedDataPipeConsumerHandle consumer_handle)
      : EmbeddedWorkerTestHelper(base::FilePath()) {
    EXPECT_TRUE(stream_handle_.is_null());
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_request = std::move(callback_request);
    stream_handle_->stream = std::move(consumer_handle);
  }
  ~StreamResponder() override {}

 protected:
  void OnFetchEvent(
      int /* embedded_worker_id */,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    ASSERT_FALSE(stream_handle_.is_null());
    blink::mojom::FetchAPIResponsePtr response = MakeOkResponse();
    response->headers = MakeHeaders();
    response_callback->OnResponseStream(
        std::move(response), std::move(stream_handle_),
        blink::mojom::ServiceWorkerFetchEventTiming::New());
    std::move(finish_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);
  }
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  EXPECT_EQ(net::OK, url_request_delegate_.request_status());
  net::HttpResponseHeaders* headers = request_->response_headers();
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ("OK", headers->GetStatusText());
  CheckHeaders(headers);
  EXPECT_EQ(expected_response, url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());

  request_.reset();
  EXPECT_FALSE(HasWork());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_ConsecutiveRead) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_response, url_request_delegate_.data_received());
  }
  stream_callback->OnCompleted();
  base::RunLoop().RunUntilIdle();

  // IO is still pending since |producer_handle| is not yet reset, but the data
  // should have been received by now.
  EXPECT_EQ(net::ERR_IO_PENDING, url_request_delegate_.request_status());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponseAndCancel) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());

  for (int i = 0; i < 512; ++i) {
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);
  }
  EXPECT_TRUE(data_pipe.producer_handle.is_valid());
  request_->Cancel();
  EXPECT_FALSE(HasWork());

  // Fail to write the data pipe because it's already canceled.
  uint32_t written_bytes = sizeof(kTestData) - 1;
  MojoResult result = data_pipe.producer_handle->WriteData(
      kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  stream_callback->OnAborted();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(data_pipe.consumer_handle.is_valid());
  EXPECT_EQ(net::ERR_ABORTED, url_request_delegate_.request_status());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_Abort) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);
  }
  stream_callback->OnAborted();
  data_pipe.producer_handle.reset();

  EXPECT_FALSE(HasWork());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWork());
  EXPECT_EQ(net::ERR_CONNECTION_RESET, url_request_delegate_.request_status());

  net::HttpResponseHeaders* headers = request_->response_headers();
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ("OK", headers->GetStatusText());
  CheckHeaders(headers);
  EXPECT_EQ(expected_response, url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());

  request_.reset();
  EXPECT_FALSE(HasWork());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_AbortBeforeData) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  base::RunLoop().RunUntilIdle();
  stream_callback->OnAborted();
  base::RunLoop().RunUntilIdle();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_response, url_request_delegate_.data_received());
  }

  data_pipe.producer_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_CONNECTION_RESET, url_request_delegate_.request_status());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_AbortAfterData) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  base::RunLoop().RunUntilIdle();
  data_pipe.producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  stream_callback->OnAborted();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::ERR_CONNECTION_RESET, url_request_delegate_.request_status());
  EXPECT_EQ(200, request_->response_headers()->response_code());
  EXPECT_EQ("OK", request_->response_headers()->GetStatusText());
  EXPECT_EQ("", url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_ConsecutiveReadAndAbort) {
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  SetUpWithHelper(
      std::make_unique<StreamResponder>(mojo::MakeRequest(&stream_callback),
                                        std::move(data_pipe.consumer_handle)));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 512; ++i) {
    expected_response += kTestData;
    uint32_t written_bytes = sizeof(kTestData) - 1;
    MojoResult result = data_pipe.producer_handle->WriteData(
        kTestData, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(sizeof(kTestData) - 1, written_bytes);

    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_response, url_request_delegate_.data_received());
  }
  stream_callback->OnAborted();

  base::RunLoop().RunUntilIdle();

  // IO is still pending since |producer_handle| is not yet reset, but the data
  // should have been received by now.
  EXPECT_EQ(net::ERR_IO_PENDING, url_request_delegate_.request_status());
  EXPECT_EQ(200, request_->response_headers()->response_code());
  EXPECT_EQ("OK", request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.data_received());

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

// Helper to simulate failing to dispatch a fetch event to a worker.
class FailFetchHelper : public EmbeddedWorkerTestHelper {
 public:
  FailFetchHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~FailFetchHelper() override {}

 protected:
  void OnFetchEvent(
      int embedded_worker_id,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
      blink::mojom::
          ServiceWorkerFetchResponseCallbackPtr /* response_callback */,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    SimulateWorkerStopped(embedded_worker_id);
    std::move(finish_callback)
        .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED,
             base::TimeTicks::Now());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailFetchHelper);
};

TEST_F(ServiceWorkerURLRequestJobTest, FailFetchDispatch) {
  SetUpWithHelper(std::make_unique<FailFetchHelper>());

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, url_request_delegate_.request_status());
  // We should have fallen back to network.
  EXPECT_EQ(200, request_->GetResponseCode());
  EXPECT_EQ("PASS", url_request_delegate_.data_received());
  EXPECT_FALSE(HasWork());
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controller(), nullptr);

  EXPECT_EQ(1, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
}

TEST_F(ServiceWorkerURLRequestJobTest, FailToActivate_MainResource) {
  SetUpWithHelper(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  RunFailToActivateTest(RESOURCE_TYPE_MAIN_FRAME);

  // The load should fail and we should have fallen back to network because
  // this is a main resource request.
  EXPECT_EQ(net::OK, url_request_delegate_.request_status());
  EXPECT_EQ(200, request_->GetResponseCode());
  EXPECT_EQ("PASS", url_request_delegate_.data_received());

  // The controller should be reset since the main resource request failed.
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controller(), nullptr);
}

TEST_F(ServiceWorkerURLRequestJobTest, FailToActivate_Subresource) {
  SetUpWithHelper(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath()));
  RunFailToActivateTest(RESOURCE_TYPE_IMAGE);

  // The load should fail and we should not fall back to network because
  // this is a subresource request.
  EXPECT_EQ(net::OK, url_request_delegate_.request_status());
  EXPECT_EQ(500, request_->GetResponseCode());
  EXPECT_EQ("Service Worker Response Error",
            request_->response_headers()->GetStatusText());

  // The controller should still be set after a subresource request fails.
  ServiceWorkerProviderHost* host = helper_->context()->GetProviderHost(
      helper_->mock_render_process_id(), kProviderID);
  ASSERT_TRUE(host);
  EXPECT_EQ(host->controller(), version_);
}

class EarlyResponseHelper : public EmbeddedWorkerTestHelper {
 public:
  EarlyResponseHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~EarlyResponseHelper() override {}

  void FinishWaitUntil() {
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

 protected:
  void OnFetchEvent(
      int /* embedded_worker_id */,
      const network::ResourceRequest& /* request */,
      blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    finish_callback_ = std::move(finish_callback);
    response_callback->OnResponse(
        MakeOkResponse(), blink::mojom::ServiceWorkerFetchEventTiming::New());
  }

 private:
  mojom::ServiceWorker::DispatchFetchEventCallback finish_callback_;
  DISALLOW_COPY_AND_ASSIGN(EarlyResponseHelper);
};

// This simulates the case when a response is returned and the fetch event is
// still in flight.
TEST_F(ServiceWorkerURLRequestJobTest, EarlyResponse) {
  SetUpWithHelper(std::make_unique<EarlyResponseHelper>());
  EarlyResponseHelper* helper =
      static_cast<EarlyResponseHelper*>(helper_.get());

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", std::string(), true /* expect_valid_ssl */);

  EXPECT_EQ(0, times_prepare_to_restart_invoked_);
  ServiceWorkerResponseInfo* info =
      ServiceWorkerResponseInfo::ForRequest(request_.get());
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->was_fetched_via_service_worker());
  EXPECT_FALSE(info->was_fallback_required());
  EXPECT_EQ(0u, info->url_list_via_service_worker().size());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault,
            info->response_type_via_service_worker());
  EXPECT_FALSE(info->service_worker_start_time().is_null());
  EXPECT_FALSE(info->service_worker_ready_time().is_null());
  EXPECT_FALSE(info->response_is_in_cache_storage());
  EXPECT_EQ(std::string(), info->response_cache_storage_cache_name());

  EXPECT_FALSE(version_->HasNoWork());
  helper->FinishWaitUntil();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->HasNoWork());
}

// Test cancelling the URLRequest while the fetch event is in flight.
TEST_F(ServiceWorkerURLRequestJobTest, CancelRequest) {
  SetUpWithHelper(std::make_unique<DelayHelper>(this));
  DelayHelper* helper = static_cast<DelayHelper*>(helper_.get());

  // Start the URL request. The job will be waiting for the
  // worker to respond to the fetch event.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("https://example.com/foo.html"), net::DEFAULT_PRIORITY,
      &url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->set_method("GET");
  request_->Start();
  base::RunLoop().RunUntilIdle();
  helper->CompleteStartWorker();
  base::RunLoop().RunUntilIdle();

  // Cancel the URL request.
  request_->Cancel();
  base::RunLoop().RunUntilIdle();

  // Respond to the fetch event.
  EXPECT_FALSE(version_->HasNoWork());
  helper->Respond();
  base::RunLoop().RunUntilIdle();

  // The fetch event request should no longer be in-flight.
  EXPECT_TRUE(version_->HasNoWork());
}

// TODO(kinuko): Add more tests with different response data and also for
// FallbackToNetwork case.

}  // namespace service_worker_url_request_job_unittest
}  // namespace content
