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
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_url_loader.h"
#include "content/browser/appcache/mock_appcache_policy.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

static const int kMockProcessId = 1;

// TODO(michaeln/ananta)
// Build on the abstractions provided by the request and the job classes to
// provide mock request and job classes to the AppCacheRequestHandler class
// which would make it testable. It would also allow us to avoid the URLRequest
// and URLLoader semantics in the test cases here,
class AppCacheRequestHandlerTest : public ::testing::Test {
 public:
  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  // Test harness --------------------------------------------------

  AppCacheRequestHandlerTest() : host_(nullptr), request_(nullptr) {
    AppCacheRequestHandler::SetRunningInTests(true);
  }

  ~AppCacheRequestHandlerTest() override {
    AppCacheRequestHandler::SetRunningInTests(false);
  }

  template <class Method>
  void RunTestOnUIThread(Method method) {
    base::RunLoop run_loop;
    test_finished_cb_ = run_loop.QuitClosure();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheRequestHandlerTest::MethodWrapper<Method>,
                       base::Unretained(this), method));
    run_loop.Run();
  }

  void SetUpTest() {
    mock_service_ = std::make_unique<MockAppCacheService>();
    mock_policy_ = std::make_unique<MockAppCachePolicy>();
    mock_service_->set_appcache_policy(mock_policy_.get());
    const auto kHostId = base::UnguessableToken::Create();
    const int kRenderFrameId = 2;
    mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend_remote;
    ignore_result(frontend_remote.InitWithNewPipeAndPassReceiver());
    mock_service_->RegisterHost(
        host_remote_.BindNewPipeAndPassReceiver(), std::move(frontend_remote),
        kHostId, kRenderFrameId, kMockProcessId,
        ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
            kMockProcessId),
        GetBadMessageCallback());
    host_ = mock_service_->GetHost(kHostId);
  }

  void TearDownTest() {
    if (appcache_url_loader_)
      appcache_url_loader_->DeleteIfNeeded();
    appcache_url_loader_.reset();
    handler_.reset();
    request_ = nullptr;
    mock_service_.reset();
    mock_policy_.reset();
    host_remote_.reset();
    host_ = nullptr;
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheRequestHandlerTest::TestFinishedUnwound,
                       base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    std::move(test_finished_cb_).Run();
  }

  void PushNextTask(base::OnceClosure task) {
    task_stack_.push(std::move(task));
  }

  void ScheduleNextTask() {
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(task_stack_.top()));
    task_stack_.pop();
  }

  void SetAppCacheURLLoader(AppCacheURLLoader* loader) {
    if (appcache_url_loader_) {
      appcache_url_loader_->DeleteIfNeeded();
      appcache_url_loader_ = nullptr;
    }
    if (!loader)
      return;
    appcache_url_loader_ = loader->GetWeakPtr();
  }

  AppCacheURLLoader* loader() { return appcache_url_loader_.get(); }

  // MainResource_Miss --------------------------------------------------

  void MainResource_Miss() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Miss,
                       base::Unretained(this)));

    CreateRequestAndHandler(GURL("http://blah"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Miss() {
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_TRUE(loader()->IsDeliveringNetworkResponse());

    int64_t cache_id = blink::mojom::kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(blink::mojom::kAppCacheNoCacheId, cache_id);
    EXPECT_EQ(GURL(), manifest_url);
    EXPECT_EQ(0, handler_->found_group_id_);

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    EXPECT_TRUE(host_->preferred_manifest_url().is_empty());

    TestFinished();
  }

  // MainResource_Hit --------------------------------------------------

  void MainResource_Hit() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Hit,
                       base::Unretained(this)));

    CreateRequestAndHandler(GURL("http://blah"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), GURL(), AppCacheEntry(), 1,
        2, GURL("http://blah/manifest/"));

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Hit() {
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_TRUE(loader()->IsDeliveringAppCacheResponse());

    int64_t cache_id = blink::mojom::kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_EQ(2, handler_->found_group_id_);

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    EXPECT_EQ(GURL("http://blah/manifest/"), host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_Fallback --------------------------------------------------

  void MainResource_Fallback() {
    PushNextTask(base::BindOnce(
        &AppCacheRequestHandlerTest::Verify_MainResource_Fallback,
        base::Unretained(this)));

    CreateRequestAndHandler(GURL("http://blah"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(), GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), 1, 2,
        GURL("http://blah/manifest/"));

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void SimulateResponseCode(int response_code) {
    net::HttpResponseInfo info;
    std::string headers =
        base::StringPrintf("HTTP/1.1 %i Muffin\r\n\r\n", response_code);
    info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));

    auto response = network::mojom::URLResponseHead::New();
    response->headers = info.headers;
    request_->set_response(std::move(response));
    DCHECK_EQ(response_code, request_->GetResponseCode());
  }

  void SimulateResponseInfo(const net::HttpResponseInfo& info) {
    auto response = network::mojom::URLResponseHead::New();
    response->headers = info.headers;
    request_->set_response(std::move(response));
  }

  void Verify_MainResource_Fallback() {
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_TRUE(loader()->IsDeliveringNetworkResponse());

    // Simulate an http error of the real network job.
    SimulateResponseCode(500);

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsDeliveringAppCacheResponse());

    int64_t cache_id = blink::mojom::kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);
    EXPECT_TRUE(host_->main_resource_was_namespace_entry_);
    EXPECT_EQ(GURL("http://blah/fallbackurl"), host_->namespace_entry_url_);

    EXPECT_EQ(GURL("http://blah/manifest/"), host_->preferred_manifest_url());

    TestFinished();
  }

  // MainResource_FallbackOverride --------------------------------------------

  void MainResource_FallbackOverride() {
    PushNextTask(base::BindOnce(
        &AppCacheRequestHandlerTest::Verify_MainResource_FallbackOverride,
        base::Unretained(this)));

    CreateRequestAndHandler(GURL("http://blah/fallback-override"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(), GURL("http://blah/fallbackurl"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), 1, 2,
        GURL("http://blah/manifest/"));

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_FallbackOverride() {
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_TRUE(loader()->IsDeliveringNetworkResponse());

    // Simulate an http error of the real network job, but with custom
    // headers that override the fallback behavior.
    const char kOverrideHeaders[] =
        "HTTP/1.1 404 BOO HOO\0"
        "x-chromium-appcache-fallback-override: disallow-fallback\0"
        "\0";
    net::HttpResponseInfo info;
    info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(kOverrideHeaders, base::size(kOverrideHeaders)));
    SimulateResponseInfo(info);

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    // GetExtraResponseInfo should return no information.
    int64_t cache_id = blink::mojom::kAppCacheNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(blink::mojom::kAppCacheNoCacheId, cache_id);
    EXPECT_TRUE(manifest_url.is_empty());

    TestFinished();
  }

  // SubResource_Miss_WithNoCacheSelected ----------------------------------

  void SubResource_Miss_WithNoCacheSelected() {
    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
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

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsDeliveringErrorResponse());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // SubResource_Miss_WithWaitForCacheSelection -----------------------------

  void SubResource_Miss_WithWaitForCacheSelection() {
    // Precondition, the host is waiting on cache selection.
    scoped_refptr<AppCache> cache(MakeNewCache());
    host_->pending_selected_cache_id_ = cache->cache_id();
    host_->set_preferred_manifest_url(cache->owning_group()->manifest_url());

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    host_->FinishCacheSelection(cache.get(), nullptr, base::DoNothing());
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_TRUE(loader()->IsDeliveringErrorResponse());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // SubResource_Hit -----------------------------

  void SubResource_Hit() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsDeliveringAppCacheResponse());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // SubResource_RedirectFallback -----------------------------

  void SubResource_RedirectFallback() {
    // Redirects to resources in the a different origin are subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(loader());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://not_blah/redirect")));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsDeliveringAppCacheResponse());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // SubResource_NoRedirectFallback -----------------------------

  void SubResource_NoRedirectFallback() {
    // Redirects to resources in the same-origin are not subject to
    // fallback namespaces.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(loader());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());

    SimulateResponseCode(200);
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // SubResource_Network -----------------------------

  void SubResource_Network() {
    // A sub-resource load where the resource is in a network namespace,
    // should result in the system using a 'real' job to do the network
    // retrieval.
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(AppCacheEntry(), AppCacheEntry(),
                                            true);

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(loader());

    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("http://blah/redirect")));
    EXPECT_FALSE(loader());
    SetAppCacheURLLoader(handler_->MaybeLoadFallbackForResponse(nullptr));
    EXPECT_FALSE(loader());

    TestFinished();
  }

  // DestroyedHost -----------------------------

  void DestroyedHost() {
    host_->AssociateCompleteCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());

    mock_service_->EraseHost(host_->host_id());
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

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    mock_service_->EraseHost(host_->host_id());
    host_ = nullptr;

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

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());
    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());

    mock_service_.reset();
    mock_policy_.reset();
    host_ = nullptr;

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

    CreateRequestAndHandler(GURL("ftp://blah/"), host_,
                            blink::mojom::ResourceType::kSubResource);
    EXPECT_TRUE(handler_.get());  // we could redirect to http (conceivably)

    EXPECT_FALSE(handler_->MaybeLoadResource(nullptr));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        nullptr, GURL("ftp://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // CanceledRequest -----------------------------

  void CanceledRequest() {
    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());
    EXPECT_FALSE(loader()->IsStarted());

    base::WeakPtr<AppCacheURLLoader> weak_job = loader()->GetWeakPtr();

    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(nullptr));

    TestFinished();
  }

  // MainResource_Blocked --------------------------------------------------

  void MainResource_Blocked() {
    PushNextTask(
        base::BindOnce(&AppCacheRequestHandlerTest::Verify_MainResource_Blocked,
                       base::Unretained(this)));

    CreateRequestAndHandler(GURL("http://blah/"), host_,
                            blink::mojom::ResourceType::kMainFrame);
    EXPECT_TRUE(handler_.get());

    mock_policy_->can_load_return_value_ = false;
    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), GURL(), AppCacheEntry(), 1,
        2, GURL("http://blah/manifest/"));

    SetAppCacheURLLoader(handler_->MaybeLoadResource(nullptr));
    EXPECT_TRUE(loader());
    EXPECT_TRUE(loader()->IsWaiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Blocked() {
    EXPECT_FALSE(loader()->IsWaiting());
    EXPECT_FALSE(loader()->IsDeliveringAppCacheResponse());

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
    AppCache* cache =
        new AppCache(mock_storage(), mock_storage()->NewCacheId());
    cache->set_complete(true);
    AppCacheGroup* group =
        new AppCacheGroup(mock_storage(), GURL("http://blah/manifest"),
                          mock_storage()->NewGroupId());
    group->AddCache(cache);
    return cache;
  }

  mojo::ReportBadMessageCallback GetBadMessageCallback() {
    return base::BindOnce(&AppCacheRequestHandlerTest::OnBadMessage,
                          base::Unretained(this));
  }

  void OnBadMessage(const std::string& reason) { NOTREACHED(); }

  MockAppCacheStorage* mock_storage() {
    return static_cast<MockAppCacheStorage*>(mock_service_->storage());
  }

  void CreateRequestAndHandler(const GURL& url,
                               AppCacheHost* host,
                               blink::mojom::ResourceType resource_type) {
    network::ResourceRequest resource_request;
    resource_request.url = url;
    resource_request.method = "GET";
    auto request = std::make_unique<AppCacheRequest>(resource_request);
    request_ = request.get();
    handler_ =
        host->CreateRequestHandler(std::move(request), resource_type, false);
  }

  // Data members --------------------------------------------------
  BrowserTaskEnvironment task_environment_;

  base::OnceClosure test_finished_cb_;
  base::stack<base::OnceClosure> task_stack_;
  std::unique_ptr<MockAppCacheService> mock_service_;
  std::unique_ptr<MockAppCachePolicy> mock_policy_;
  AppCacheHost* host_;
  mojo::Remote<blink::mojom::AppCacheHost> host_remote_;
  AppCacheRequest* request_;
  std::unique_ptr<AppCacheRequestHandler> handler_;
  base::WeakPtr<AppCacheURLLoader> appcache_url_loader_;
};

TEST_F(AppCacheRequestHandlerTest, MainResource_Miss) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::MainResource_Miss);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Hit) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::MainResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Fallback) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::MainResource_Fallback);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_FallbackOverride) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::MainResource_FallbackOverride);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithNoCacheSelected) {
  RunTestOnUIThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithNoCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithCacheSelected) {
  RunTestOnUIThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithWaitForCacheSelection) {
  RunTestOnUIThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithWaitForCacheSelection);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Hit) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::SubResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_RedirectFallback) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::SubResource_RedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_NoRedirectFallback) {
  RunTestOnUIThread(
      &AppCacheRequestHandlerTest::SubResource_NoRedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Network) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::SubResource_Network);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHost) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::DestroyedHost);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHostWithWaitingJob) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::DestroyedHostWithWaitingJob);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedService) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::DestroyedService);
}

TEST_F(AppCacheRequestHandlerTest, UnsupportedScheme) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::UnsupportedScheme);
}

TEST_F(AppCacheRequestHandlerTest, CanceledRequest) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::CanceledRequest);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Blocked) {
  RunTestOnUIThread(&AppCacheRequestHandlerTest::MainResource_Blocked);
}

}  // namespace content
