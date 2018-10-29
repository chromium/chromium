// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_read_from_cache_job.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

const int64_t kRegistrationId = 1;
const int64_t kVersionId = 2;
const int64_t kMainScriptResourceId = 10;
const int64_t kImportedScriptResourceId = 11;
const int64_t kNonExistentResourceId = 12;
const int64_t kResourceSize = 100;

void DidStoreRegistration(blink::ServiceWorkerStatusCode* status_out,
                          base::OnceClosure quit_closure,
                          blink::ServiceWorkerStatusCode status) {
  *status_out = status;
  std::move(quit_closure).Run();
}

void DidFindRegistration(
    blink::ServiceWorkerStatusCode* status_out,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  *status_out = status;
  std::move(quit_closure).Run();
}

}  // namespace

class ServiceWorkerReadFromCacheJobTest : public testing::Test {
 public:
  ServiceWorkerReadFromCacheJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        main_script_(kMainScriptResourceId,
                     GURL("http://example.com/main.js"),
                     kResourceSize),
        imported_script_(kImportedScriptResourceId,
                         GURL("http://example.com/imported.js"),
                         kResourceSize),
        test_job_interceptor_(nullptr) {}
  ~ServiceWorkerReadFromCacheJobTest() override {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    InitializeStorage();

    url_request_context_.reset(new net::TestURLRequestContext(true));

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));
    url_request_context_->set_job_factory(&test_job_factory_);

    url_request_context_->Init();
  }

  void InitializeStorage() {
    base::RunLoop run_loop;
    context()->storage()->LazyInitializeForTest(run_loop.QuitClosure());
    run_loop.Run();

    // Populate a registration in the storage.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL("http://example.com/scope");
    registration_ = new ServiceWorkerRegistration(options, kRegistrationId,
                                                  context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), main_script_.url,
                                        blink::mojom::ScriptType::kClassic,
                                        kVersionId, context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
    resources.push_back(main_script_);
    resources.push_back(imported_script_);
    version_->script_cache_map()->SetResources(resources);
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, StoreRegistration());
    ASSERT_TRUE(WriteResource(main_script_.resource_id));
    ASSERT_TRUE(WriteResource(imported_script_.resource_id));
  }

  bool WriteResource(int64_t resource_id) {
    const char kHttpHeaders[] = "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
    const char kHttpBody[] = "Hello";
    const int length = arraysize(kHttpBody);
    std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
    scoped_refptr<net::IOBuffer> body =
        base::MakeRefCounted<net::WrappedIOBuffer>(kHttpBody);

    std::unique_ptr<ServiceWorkerResponseWriter> writer =
        context()->storage()->CreateResponseWriter(resource_id);

    std::unique_ptr<net::HttpResponseInfo> info =
        std::make_unique<net::HttpResponseInfo>();
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = new net::HttpResponseHeaders(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(info));
    {
      net::TestCompletionCallback cb;
      writer->WriteInfo(info_buffer.get(), cb.callback());
      int rv = cb.WaitForResult();
      if (rv < 0)
        return false;
    }
    {
      net::TestCompletionCallback cb;
      writer->WriteData(body.get(), length, cb.callback());
      int rv = cb.WaitForResult();
      if (rv < 0)
        return false;
    }
    return true;
  }

  blink::ServiceWorkerStatusCode StoreRegistration() {
    base::RunLoop run_loop;
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        base::BindOnce(&DidStoreRegistration, &status, run_loop.QuitClosure()));
    run_loop.Run();
    return status;
  }

  blink::ServiceWorkerStatusCode FindRegistration() {
    base::RunLoop run_loop;
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    context()->storage()->FindRegistrationForId(
        registration_->id(), registration_->scope().GetOrigin(),
        base::BindOnce(&DidFindRegistration, &status, run_loop.QuitClosure()));
    run_loop.Run();
    return status;
  }

  void StartAndWaitForRequest(net::URLRequest* request) {
    request->Start();
    // net::TestDelegate quits the loop when the request is completed.
    base::RunLoop().RunUntilIdle();
  }

  blink::ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      blink::ServiceWorkerStatusCode default_code) {
    return version_->DeduceStartWorkerFailureReason(default_code);
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  ServiceWorkerDatabase::ResourceRecord main_script_;
  ServiceWorkerDatabase::ResourceRecord imported_script_;

  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;

  std::unique_ptr<net::TestURLRequestContext> url_request_context_;
  net::TestDelegate delegate_;
};

TEST_F(ServiceWorkerReadFromCacheJobTest, ReadMainScript) {
  // Read the main script from the diskcache.
  std::unique_ptr<net::URLRequest> request =
      url_request_context_->CreateRequest(main_script_.url,
                                          net::DEFAULT_PRIORITY, &delegate_,
                                          TRAFFIC_ANNOTATION_FOR_TESTS);
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<ServiceWorkerReadFromCacheJob>(
          request.get(), nullptr /* NetworkDelegate */,
          RESOURCE_TYPE_SERVICE_WORKER, context()->AsWeakPtr(), version_,
          main_script_.resource_id));
  StartAndWaitForRequest(request.get());

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
  EXPECT_EQ(0, request->status().error());
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      DeduceStartWorkerFailureReason(blink::ServiceWorkerStatusCode::kOk));
}

TEST_F(ServiceWorkerReadFromCacheJobTest, ReadImportedScript) {
  // Read the imported script from the diskcache.
  std::unique_ptr<net::URLRequest> request =
      url_request_context_->CreateRequest(imported_script_.url,
                                          net::DEFAULT_PRIORITY, &delegate_,
                                          TRAFFIC_ANNOTATION_FOR_TESTS);
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<ServiceWorkerReadFromCacheJob>(
          request.get(), nullptr /* NetworkDelegate */, RESOURCE_TYPE_SCRIPT,
          context()->AsWeakPtr(), version_, imported_script_.resource_id));
  StartAndWaitForRequest(request.get());

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
  EXPECT_EQ(0, request->status().error());
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      DeduceStartWorkerFailureReason(blink::ServiceWorkerStatusCode::kOk));
}

TEST_F(ServiceWorkerReadFromCacheJobTest, ResourceNotFound) {
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, FindRegistration());

  // Populate the script cache map with a nonexistent resource.
  ServiceWorkerScriptCacheMap* script_cache_map = version_->script_cache_map();
  script_cache_map->resource_map_.clear();
  using Record = ServiceWorkerDatabase::ResourceRecord;
  std::vector<Record> resources = {
      Record(kNonExistentResourceId, main_script_.url, main_script_.size_bytes),
      Record(imported_script_.resource_id, imported_script_.url,
             imported_script_.size_bytes)};
  script_cache_map->SetResources(resources);

  // Attempt to read it from the disk cache.
  std::unique_ptr<net::URLRequest> request =
      url_request_context_->CreateRequest(main_script_.url,
                                          net::DEFAULT_PRIORITY, &delegate_,
                                          TRAFFIC_ANNOTATION_FOR_TESTS);
  const int64_t kNonexistentResourceId = 100;
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<ServiceWorkerReadFromCacheJob>(
          request.get(), nullptr /* NetworkDelegate */,
          RESOURCE_TYPE_SERVICE_WORKER, context()->AsWeakPtr(), version_,
          kNonexistentResourceId));
  StartAndWaitForRequest(request.get());

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_CACHE_MISS, request->status().error());
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorDiskCache,
      DeduceStartWorkerFailureReason(blink::ServiceWorkerStatusCode::kOk));

  // The version should be doomed by the job.
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound, FindRegistration());
}

}  // namespace content
