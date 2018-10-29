// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_request_handler.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/stack.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_job.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/appcache/appcache_url_request.h"
#include "content/browser/appcache/appcache_url_request_job.h"
#include "content/browser/appcache/mock_appcache_policy.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kMockProcessId = 1;

// Controls whether we instantiate the URLRequest based AppCache handler or
// the URLLoader based one.
enum RequestHandlerType {
  URLREQUEST,
  URLLOADER,
};

// TODO(michaeln/ananta)
// Build on the abstractions provided by the request and the job classes to
// provide mock request and job classes to the AppCacheRequestHandler class
// which would make it testable. It would also allow us to avoid the URLRequest
// and URLLoader semantics in the test cases here,
class AppCacheRequestHandlerTest
    : public testing::TestWithParam<RequestHandlerType> {
 public:
  class MockFrontend : public AppCacheFrontend {
   public:
    void OnCacheSelected(int host_id, const AppCacheInfo& info) override {}

    void OnStatusChanged(const std::vector<int>& host_ids,
                         AppCacheStatus status) override {}

    void OnEventRaised(const std::vector<int>& host_ids,
                       AppCacheEventID event_id) override {}

    void OnErrorEventRaised(const std::vector<int>& host_ids,
                            const AppCacheErrorDetails& details) override {}

    void OnProgressEventRaised(const std::vector<int>& host_ids,
                               const GURL& url,
                               int num_total,
                               int num_complete) override {}

    void OnLogMessage(int host_id,
                      AppCacheLogLevel log_level,
                      const std::string& message) override {}

    void OnContentBlocked(int host_id, const GURL& manifest_url) override {}

    void OnSetSubresourceFactory(
        int host_id,
        network::mojom::URLLoaderFactoryPtr url_loader_factory) override {}
  };

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  // Subclasses to simulate particular responses so test cases can
  // exercise fallback code paths.

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
   public:
    MockURLRequestDelegate() : request_status_(1) {}

    void OnResponseStarted(net::URLRequest* request, int net_error) override {
      DCHECK_NE(net::ERR_IO_PENDING, net_error);
      request_status_ = net_error;
    }

    void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
      DCHECK_NE(net::ERR_IO_PENDING, bytes_read);
      if (bytes_read >= 0)
        request_status_ = net::OK;
      else
        request_status_ = bytes_read;
    }

    int request_status() const { return request_status_; }

   private:
    int request_status_;
  };

  class MockURLRequestJob : public net::URLRequestJob {
   public:
    MockURLRequestJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      const net::HttpResponseInfo& info)
        : net::URLRequestJob(request, network_delegate),
          has_response_info_(true),
          response_info_(info) {}

    ~MockURLRequestJob() override {}

   protected:
    void Start() override { NotifyHeadersComplete(); }
    void GetResponseInfo(net::HttpResponseInfo* info) override {
      if (!has_response_info_)
        return;
      *info = response_info_;
    }

   private:
    bool has_response_info_;
    net::HttpResponseInfo response_info_;
  };

  class MockURLRequestJobFactory : public net::URLRequestJobFactory {
   public:
    MockURLRequestJobFactory() {}

    ~MockURLRequestJobFactory() override { DCHECK(!request_job_); }

    void SetJob(std::unique_ptr<net::URLRequestJob> job) {
      request_job_ = std::move(job);
    }

    net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
        const std::string& scheme,
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      if (request_job_)
        return request_job_.release();

      // Some of these tests trigger UpdateJobs which start URLRequests.
      // We short circuit those be returning error jobs.
      return new net::URLRequestErrorJob(request, network_delegate,
                                         net::ERR_INTERNET_DISCONNECTED);
    }

    net::URLRequestJob* MaybeInterceptRedirect(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate,
        const GURL& location) const override {
      return nullptr;
    }

    net::URLRequestJob* MaybeInterceptResponse(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      return nullptr;
    }

    bool IsHandledProtocol(const std::string& scheme) const override {
      return scheme == "http";
    };

    bool IsSafeRedirectTarget(const GURL& location) const override {
      return false;
    }

   private:
    mutable std::unique_ptr<net::URLRequestJob> request_job_;
  };

  static void SetUpTestCase() {
    thread_bundle_.reset(
        new TestBrowserThreadBundle(TestBrowserThreadBundle::REAL_IO_THREAD));
    io_task_runner_ =
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  }

  static void TearDownTestCase() {
    thread_bundle_.reset();
    io_task_runner_ = nullptr;
  }

  // Test harness --------------------------------------------------

  AppCacheRequestHandlerTest()
      : host_(nullptr), request_(nullptr), request_handler_type_(GetParam()) {
    AppCacheRequestHandler::SetRunningInTests(true);
    if (request_handler_type_ == URLLOADER)
      feature_list_.InitAndEnableFeature(network::features::kNetworkService);
  }

  ~AppCacheRequestHandlerTest() {
    AppCacheRequestHandler::SetRunningInTests(false);
  }

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_.reset(new base::WaitableEvent(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED));
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheRequestHandlerTest::MethodWrapper<Method>,
                       base::Unretained(this), method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    mock_service_.reset(new MockAppCacheService);
    // Initializes URLRequestContext on the IO thread.
    empty_context_.reset(new net::URLRequestContext);
    mock_service_->set_request_context(empty_context_.get());
    mock_policy_.reset(new MockAppCachePolicy);
    mock_service_->set_appcache_policy(mock_policy_.get());
    mock_frontend_.reset(new MockFrontend);
    backend_impl_.reset(new AppCacheBackendImpl);
    backend_impl_->Initialize(mock_service_.get(), mock_frontend_.get(),
                              kMockProcessId);
    const int kHostId = 1;
    backend_impl_->RegisterHost(kHostId);
    host_ = backend_impl_->GetHost(kHostId);
    job_factory_.reset(new MockURLRequestJobFactory());
    empty_context_->set_job_factory(job_factory_.get());
  }

  void TearDownTest() {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    appcache_url_request_job_.reset();
    appcache_url_loader_job_.reset();
    handler_.reset();
    request_ = nullptr;
    url_request_.reset();
    backend_impl_.reset();
    mock_frontend_.reset();
    mock_service_.reset();
    mock_policy_.reset();
    job_factory_.reset();
    empty_context_.reset();
    host_ = nullptr;
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheRequestHandlerTest::TestFinishedUnwound,
                       base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(base::OnceClosure task) {
    task_stack_.push(std::move(task));
  }

  void ScheduleNextTask() {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(task_stack_.top()));
    task_stack_.pop();
  }

  void SetAppCacheJob(AppCacheJob* job) {
    if (!job) {
      appcache_url_request_job_.reset();
      appcache_url_loader_job_ = nullptr;
      return;
    }
    if (request_handler_type_ == URLREQUEST)
      appcache_url_request_job_.reset(job->AsURLRequestJob());
    else
      appcache_url_loader_job_ = job->AsURLLoaderJob()->GetDerivedWeakPtr();
  }

  AppCacheJob* job() {
    if (request_handler_type_ == URLREQUEST)
      return appcache_url_request_job_.get();
    else
      return appcache_url_loader_job_.get();
  }

  // MainResource_Miss --------------------------------------------------

  void MainResource_Miss() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Miss,
                       base::Unretained(this)));

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah"), host_,
                                        RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Miss() {
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_TRUE(job()->IsDeliveringNetworkResponse());

    int64_t cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(kAppCacheNoCacheId, cache_id);
    EXPECT_EQ(GURL(), manifest_url);
    EXPECT_EQ(0, handler_->found_group_id_);

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    EXPECT_TRUE(host_->preferred_manifest_url().is_empty());

    TestFinished();
  }

  // MainResource_Hit --------------------------------------------------

  void MainResource_Hit() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Hit,
                       base::Unretained(this)));

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah"), host_,
                                        RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        GURL(), AppCacheEntry(),
        1, 2, GURL("http://blah/manifest/"));

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Hit() {
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_TRUE(job()->IsDeliveringAppCacheResponse());

    int64_t cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_EQ(2, handler_->found_group_id_);

    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    EXPECT_EQ(GURL("http://blah/manifest/"),
              host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_Fallback --------------------------------------------------

  void MainResource_Fallback() {
    PushNextTask(base::BindOnce(
        &AppCacheRequestHandlerTest::Verify_MainResource_Fallback,
        base::Unretained(this)));

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah"), host_,
                                        RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(),
        GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        1, 2, GURL("http://blah/manifest/"));

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void SimulateResponseCode(int response_code) {
    net::HttpResponseInfo info;
    std::string headers =
        base::StringPrintf("HTTP/1.1 %i Muffin\r\n\r\n", response_code);
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));

    if (request_handler_type_ == URLREQUEST) {
      job_factory_->SetJob(std::make_unique<MockURLRequestJob>(
          url_request_.get(), nullptr, info));
      request_->AsURLRequest()->GetURLRequest()->Start();
      // All our simulation needs to satisfy are the DCHECK's for the request
      // status and the response code.
      DCHECK_EQ(net::OK, delegate_.request_status());
    } else {
      network::ResourceResponseHead response;
      response.headers = info.headers;
      request_->AsURLLoaderRequest()->set_response(response);
    }
    DCHECK_EQ(response_code, request_->GetResponseCode());
  }

  void SimulateResponseInfo(const net::HttpResponseInfo& info) {
    if (request_handler_type_ == URLREQUEST) {
      job_factory_->SetJob(std::make_unique<MockURLRequestJob>(
          url_request_.get(), nullptr, info));
      request_->AsURLRequest()->GetURLRequest()->Start();
    } else {
      network::ResourceResponseHead response;
      response.headers = info.headers;
      request_->AsURLLoaderRequest()->set_response(response);
    }
  }

  void Verify_MainResource_Fallback() {
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_TRUE(job()->IsDeliveringNetworkResponse());

    // The handler expects to the job to tell it that the request is going to
    // be restarted before it sees the next request.
    if (request_handler_type_ == URLREQUEST) {
      handler_->OnPrepareToRestartURLRequest();

      // When the request is restarted, the existing job is dropped so a
      // real network job gets created. We expect NULL here which will cause
      // the net library to create a real job.
      SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
      EXPECT_FALSE(job());
    }

    // Simulate an http error of the real network job.
    SimulateResponseCode(500);

    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsDeliveringAppCacheResponse());

    int64_t cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_TRUE(host_->main_resource_was_namespace_entry_);
    EXPECT_EQ(GURL("http://blah/fallbackurl"), host_->namespace_entry_url_);

    EXPECT_EQ(GURL("http://blah/manifest/"),
              host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_FallbackOverride --------------------------------------------

  void MainResource_FallbackOverride() {
    PushNextTask(base::BindOnce(
        &AppCacheRequestHandlerTest::Verify_MainResource_FallbackOverride,
        base::Unretained(this)));

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/fallback-override"),
                                        host_, RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(),
        GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        1, 2, GURL("http://blah/manifest/"));

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_FallbackOverride() {
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_TRUE(job()->IsDeliveringNetworkResponse());

    // The handler expects to the job to tell it that the request is going to
    // be restarted before it sees the next request.
    if (request_handler_type_ == URLREQUEST) {
      handler_->OnPrepareToRestartURLRequest();

      // When the request is restarted, the existing job is dropped so a
      // real network job gets created. We expect NULL here which will cause
      // the net library to create a real job.
      SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
      EXPECT_FALSE(job());
    }

    // Simulate an http error of the real network job, but with custom
    // headers that override the fallback behavior.
    const char kOverrideHeaders[] =
        "HTTP/1.1 404 BOO HOO\0"
        "x-chromium-appcache-fallback-override: disallow-fallback\0"
        "\0";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        std::string(kOverrideHeaders, arraysize(kOverrideHeaders)));
    SimulateResponseInfo(info);

    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    // GetExtraResponseInfo should return no information.
    int64_t cache_id = kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(kAppCacheNoCacheId, cache_id);
    EXPECT_TRUE(manifest_url.is_empty());

    TestFinished();
  }

  // SubResource_Miss_WithNoCacheSelected ----------------------------------

  void SubResource_Miss_WithNoCacheSelected() {
    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    // We avoid creating handler when possible, sub-resource requests are not
    // subject to retrieval from an appcache when there's no associated cache.
    EXPECT_FALSE(handler_.get());

    TestFinished();
  }

  // SubResource_Miss_WithCacheSelected ----------------------------------

  void SubResource_Miss_WithCacheSelected() {
    // A sub-resource load where the resource is not in an appcache, or
    // in a network or fallback namespace, should result in a failed request.
    host_->AssociateCompleteCache(MakeNewCache());

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsDeliveringErrorResponse());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // SubResource_Miss_WithWaitForCacheSelection -----------------------------

  void SubResource_Miss_WithWaitForCacheSelection() {
    // Precondition, the host is waiting on cache selection.
    scoped_refptr<AppCache> cache(MakeNewCache());
    host_->pending_selected_cache_id_ = cache->cache_id();
    host_->set_preferred_manifest_url(cache->owning_group()->manifest_url());

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    host_->FinishCacheSelection(cache.get(), nullptr);
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_TRUE(job()->IsDeliveringErrorResponse());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // SubResource_Hit -----------------------------

  void SubResource_Hit() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsDeliveringAppCacheResponse());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // SubResource_RedirectFallback -----------------------------

  void SubResource_RedirectFallback() {
    // Redirects to resources in the a different origin are subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(job());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://not_blah/redirect")));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsDeliveringAppCacheResponse());

    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // SubResource_NoRedirectFallback -----------------------------

  void SubResource_NoRedirectFallback() {
    // Redirects to resources in the same-origin are not subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(job());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());

    SimulateResponseCode(200);
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // SubResource_Network -----------------------------

  void SubResource_Network() {
    // A sub-resource load where the resource is in a network namespace,
    // should result in the system using a 'real' job to do the network
    // retrieval.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(), true);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(job());

    SetAppCacheJob(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(job());
    SetAppCacheJob(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(job());

    TestFinished();
  }

  // DestroyedHost -----------------------------

  void DestroyedHost() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    backend_impl_->UnregisterHost(1);
    host_ = nullptr;

    EXPECT_FALSE(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // DestroyedHostWithWaitingJob -----------------------------

  void DestroyedHostWithWaitingJob() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    backend_impl_->UnregisterHost(1);
    host_ = nullptr;

    if (request_handler_type_ == URLREQUEST) {
      EXPECT_TRUE(appcache_url_request_job_->has_been_killed());
    }
    EXPECT_FALSE(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // DestroyedService -----------------------------

  void DestroyedService() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());
    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());

    backend_impl_.reset();
    mock_frontend_.reset();
    mock_service_.reset();
    mock_policy_.reset();
    host_ = nullptr;

    if (request_handler_type_ == URLREQUEST) {
      EXPECT_TRUE(appcache_url_request_job_->has_been_killed());
    }
    EXPECT_FALSE(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // UnsupportedScheme -----------------------------

  void UnsupportedScheme() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    EXPECT_TRUE(CreateRequestAndHandler(GURL("ftp://blah/"), host_,
                                        RESOURCE_TYPE_SUB_RESOURCE));
    EXPECT_TRUE(handler_.get());  // we could redirect to http (conceivably)

    EXPECT_FALSE(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("ftp://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // CanceledRequest -----------------------------

  void CanceledRequest() {
    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());
    EXPECT_FALSE(job()->IsStarted());

    base::WeakPtr<AppCacheJob> weak_job = job()->GetWeakPtr();

    // TODO(ananta/michaeln)
    // Rewrite this test for URLLoader.
    if (request_handler_type_ == URLREQUEST) {
      job_factory_->SetJob(std::move(appcache_url_request_job_));

      request_->AsURLRequest()->GetURLRequest()->Start();
      ASSERT_TRUE(weak_job);
      EXPECT_TRUE(weak_job->IsStarted());

      request_->AsURLRequest()->GetURLRequest()->Cancel();
      ASSERT_FALSE(weak_job);
    }

    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // MainResource_Blocked --------------------------------------------------

  void MainResource_Blocked() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Blocked,
                       base::Unretained(this)));

    EXPECT_TRUE(CreateRequestAndHandler(GURL("http://blah/"), host_,
                                        RESOURCE_TYPE_MAIN_FRAME));
    EXPECT_TRUE(handler_.get());

    mock_policy_->can_load_return_value_ = false;
    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        GURL(), AppCacheEntry(),
        1, 2, GURL("http://blah/manifest/"));

    SetAppCacheJob(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(job());
    EXPECT_TRUE(job()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Blocked() {
    EXPECT_FALSE(job()->IsWaiting());
    EXPECT_FALSE(job()->IsDeliveringAppCacheResponse());

    EXPECT_EQ(0, handler_->found_cache_id_);
    EXPECT_EQ(0, handler_->found_group_id_);
    EXPECT_TRUE(handler_->found_manifest_url_.is_empty());
    EXPECT_TRUE(host_->preferred_manifest_url().is_empty());
    EXPECT_TRUE(host_->main_resource_blocked_);
    EXPECT_EQ(host_->blocked_manifest_url_, "http://blah/manifest/");

    TestFinished();
  }

  // Test case helpers --------------------------------------------------

  AppCache* MakeNewCache() {
    AppCache* cache = new AppCache(
        mock_storage(), mock_storage()->NewCacheId());
    cache->set_complete(true);
    AppCacheGroup* group = new AppCacheGroup(
        mock_storage(), GURL("http://blah/manifest"),
        mock_storage()->NewGroupId());
    group->AddCache(cache);
    return cache;
  }

  MockAppCacheStorage* mock_storage() {
    return reinterpret_cast<MockAppCacheStorage*>(mock_service_->storage());
  }

  bool CreateRequestAndHandler(const GURL& url,
                               AppCacheHost* host,
                               ResourceType resource_type) {
    if (request_handler_type_ == URLREQUEST) {
      url_request_ = empty_context_->CreateRequest(
          url, net::DEFAULT_PRIORITY, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

      std::unique_ptr<AppCacheRequest> request =
          AppCacheURLRequest::Create(url_request_.get());
      request_ = request.get();
      handler_ =
          host->CreateRequestHandler(std::move(request), resource_type, false);
      return true;
    } else if (request_handler_type_ == URLLOADER) {
      network::ResourceRequest resource_request;
      resource_request.url = url;
      resource_request.method = "GET";
      std::unique_ptr<AppCacheRequest> request =
          AppCacheURLLoaderRequest::Create(resource_request);
      request_ = request.get();
      handler_ =
          host->CreateRequestHandler(std::move(request), resource_type, false);
      return true;
    }
    return false;
  }

  // Data members --------------------------------------------------

  std::unique_ptr<base::WaitableEvent> test_finished_event_;
  base::stack<base::OnceClosure> task_stack_;
  std::unique_ptr<MockAppCacheService> mock_service_;
  std::unique_ptr<AppCacheBackendImpl> backend_impl_;
  std::unique_ptr<MockFrontend> mock_frontend_;
  std::unique_ptr<MockAppCachePolicy> mock_policy_;
  AppCacheHost* host_;
  std::unique_ptr<net::URLRequestContext> empty_context_;
  std::unique_ptr<MockURLRequestJobFactory> job_factory_;
  MockURLRequestDelegate delegate_;
  AppCacheRequest* request_;
  std::unique_ptr<net::URLRequest> url_request_;
  std::unique_ptr<AppCacheRequestHandler> handler_;
  std::unique_ptr<AppCacheURLRequestJob> appcache_url_request_job_;
  base::WeakPtr<AppCacheURLLoaderJob> appcache_url_loader_job_;

  static std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  static scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  RequestHandlerType request_handler_type_;
  base::test::ScopedFeatureList feature_list_;
};

// static
std::unique_ptr<TestBrowserThreadBundle>
    AppCacheRequestHandlerTest::thread_bundle_;
scoped_refptr<base::SingleThreadTaskRunner>
    AppCacheRequestHandlerTest::io_task_runner_;

TEST_P(AppCacheRequestHandlerTest, MainResource_Miss) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Miss);
}

TEST_P(AppCacheRequestHandlerTest, MainResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Hit);
}

TEST_P(AppCacheRequestHandlerTest, MainResource_Fallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Fallback);
}

TEST_P(AppCacheRequestHandlerTest, MainResource_FallbackOverride) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::MainResource_FallbackOverride);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_Miss_WithNoCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithNoCacheSelected);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_Miss_WithCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithCacheSelected);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_Miss_WithWaitForCacheSelection) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithWaitForCacheSelection);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Hit);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_RedirectFallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_RedirectFallback);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_NoRedirectFallback) {
  RunTestOnIOThread(
    &AppCacheRequestHandlerTest::SubResource_NoRedirectFallback);
}

TEST_P(AppCacheRequestHandlerTest, SubResource_Network) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Network);
}

TEST_P(AppCacheRequestHandlerTest, DestroyedHost) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHost);
}

TEST_P(AppCacheRequestHandlerTest, DestroyedHostWithWaitingJob) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHostWithWaitingJob);
}

TEST_P(AppCacheRequestHandlerTest, DestroyedService) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedService);
}

TEST_P(AppCacheRequestHandlerTest, UnsupportedScheme) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::UnsupportedScheme);
}

TEST_P(AppCacheRequestHandlerTest, CanceledRequest) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::CanceledRequest);
}

TEST_P(AppCacheRequestHandlerTest, MainResource_Blocked) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Blocked);
}

INSTANTIATE_TEST_CASE_P(,
                        AppCacheRequestHandlerTest,
                        ::testing::Values(URLREQUEST, URLLOADER));

}  // namespace content
