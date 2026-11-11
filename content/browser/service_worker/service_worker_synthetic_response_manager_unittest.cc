// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class MockServiceWorkerStreamCallback
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  MockServiceWorkerStreamCallback() = default;
  ~MockServiceWorkerStreamCallback() override = default;

  void OnCompleted() override {
    is_aborted_ = false;
    run_loop_.Quit();
  }

  void OnAborted() override {
    is_aborted_ = true;
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  mojo::PendingRemote<blink::mojom::ServiceWorkerStreamCallback>
  GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  bool is_aborted() const { return is_aborted_; }

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<blink::mojom::ServiceWorkerStreamCallback> receiver_{this};
  bool is_aborted_ = false;
};

class ServiceWorkerSyntheticResponseManagerTest : public testing::Test {
 public:
  ServiceWorkerSyntheticResponseManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    // Create a ServiceWorkerVersion.
    scoped_refptr<ServiceWorkerRegistration> registration =
        CreateNewServiceWorkerRegistration(
            helper_->context()->registry(),
            blink::mojom::ServiceWorkerRegistrationOptions(
                GURL("https://example.com/scope"),
                blink::mojom::ScriptType::kClassic,
                blink::mojom::ServiceWorkerUpdateViaCache::kImports),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(GURL("https://example.com/scope"))));
    version_ = CreateNewServiceWorkerVersion(
        helper_->context()->registry(), registration.get(),
        GURL("https://example.com/script.js"),
        blink::mojom::ScriptType::kClassic);
  }

  void TearDown() override {
    version_ = nullptr;
    helper_.reset();
  }

  void InitializeWriteBufferManager(
      ServiceWorkerSyntheticResponseManager* manager) {
    manager->write_buffer_manager_.emplace();
  }

  void CloseConsumerHandle(ServiceWorkerSyntheticResponseManager* manager) {
    mojo::ScopedDataPipeConsumerHandle consumer =
        manager->write_buffer_manager_->ReleaseConsumerHandle();
    consumer.reset();
  }

  void SetStreamCallback(
      ServiceWorkerSyntheticResponseManager* manager,
      mojo::PendingRemote<blink::mojom::ServiceWorkerStreamCallback> callback) {
    manager->stream_callback_.Bind(std::move(callback));
  }

  void CallNotifyReloading(ServiceWorkerSyntheticResponseManager* manager) {
    manager->NotifyReloading(
        manager->write_buffer_manager_->ReleaseProducerHandle());
  }

  void CallMaybeSetResponseHead(
      ServiceWorkerSyntheticResponseManager* manager,
      const network::mojom::URLResponseHead& response_head) {
    manager->MaybeSetResponseHead(response_head);
  }

  void CallOnReceiveResponse(ServiceWorkerSyntheticResponseManager* manager,
                             network::mojom::URLResponseHeadPtr response_head,
                             mojo::ScopedDataPipeConsumerHandle body = {}) {
    manager->OnReceiveResponse(std::move(response_head), std::move(body));
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

TEST_F(ServiceWorkerSyntheticResponseManagerTest, Status) {
  // Set a response head without headers (simulating the crash condition).
  // Verify status is kNotReady.
  auto response_head = network::mojom::URLResponseHead::New();
  ASSERT_FALSE(response_head->headers);
  version_->SetResponseHeadForSyntheticResponse(std::move(response_head));
  ServiceWorkerSyntheticResponseManager manager(version_);
  EXPECT_EQ(manager.Status(), ServiceWorkerSyntheticResponseManager::
                                  SyntheticResponseStatus::kNotReady);

  // Update the response head with valid headers, and verify status is still
  // kNotReady, since the manager determines the status at the construction.
  auto valid_head = network::mojom::URLResponseHead::New();
  valid_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
  version_->SetResponseHeadForSyntheticResponse(std::move(valid_head));
  EXPECT_EQ(manager.Status(), ServiceWorkerSyntheticResponseManager::
                                  SyntheticResponseStatus::kNotReady);

  // Create a new manager, and verify the status is kReady, since the version
  // has a valid response head.
  ServiceWorkerSyntheticResponseManager manager_with_ready_status(version_);
  EXPECT_EQ(
      manager_with_ready_status.Status(),
      ServiceWorkerSyntheticResponseManager::SyntheticResponseStatus::kReady);
}

TEST_F(ServiceWorkerSyntheticResponseManagerTest, NotifyReloading_PipeClosed) {
  base::HistogramTester histogram_tester;

  // Set a valid response head.
  auto valid_head = network::mojom::URLResponseHead::New();
  valid_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
  version_->SetResponseHeadForSyntheticResponse(std::move(valid_head));
  ServiceWorkerSyntheticResponseManager manager(version_);

  // Initialize the write buffer manager (which creates a data pipe).
  InitializeWriteBufferManager(&manager);

  // Close the consumer end of the pipe to simulate a cancelled navigation.
  CloseConsumerHandle(&manager);

  // Set up the stream callback.
  auto stream_callback = std::make_unique<MockServiceWorkerStreamCallback>();
  SetStreamCallback(&manager, stream_callback->GetRemote());

  // Call NotifyReloading. This should detect the closed pipe and call OnAborted.
  CallNotifyReloading(&manager);

  stream_callback->Wait();
  EXPECT_TRUE(stream_callback->is_aborted());

  // Verify that the write failure was recorded in UMA.
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.SyntheticResponse.WriteFallbackBodyResult",
      MOJO_RESULT_FAILED_PRECONDITION, 1);
}

TEST_F(ServiceWorkerSyntheticResponseManagerTest, MaybeSetResponseHead) {
  ServiceWorkerSyntheticResponseManager manager(version_);

  // 1. Successful response with BOTH opt-in and CSP headers -> header is
  // stored.
  auto head_with_both = network::mojom::URLResponseHead::New();
  head_with_both->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  head_with_both->headers->AddHeader("Service-Worker-Synthetic-Response", "?1");
  head_with_both->headers->AddHeader("Content-Security-Policy",
                                     "default-src 'none'");
  CallMaybeSetResponseHead(&manager, *head_with_both);
  EXPECT_TRUE(version_->GetResponseHeadForSyntheticResponse());
  version_->ResetResponseHeadForSyntheticResponse();

  // 2. Successful response with ONLY opt-in header -> header is NOT stored.
  auto head_with_opt_in = network::mojom::URLResponseHead::New();
  head_with_opt_in->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  head_with_opt_in->headers->AddHeader("Service-Worker-Synthetic-Response",
                                       "?1");
  CallMaybeSetResponseHead(&manager, *head_with_opt_in);
  EXPECT_FALSE(version_->GetResponseHeadForSyntheticResponse());

  // 3. Successful response with ONLY CSP header -> header is NOT stored.
  auto head_with_csp = network::mojom::URLResponseHead::New();
  head_with_csp->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  head_with_csp->headers->AddHeader("Content-Security-Policy",
                                    "default-src 'none'");
  CallMaybeSetResponseHead(&manager, *head_with_csp);
  EXPECT_FALSE(version_->GetResponseHeadForSyntheticResponse());

  // 4. Successful response with NO headers -> header is NOT stored.
  auto head_with_no_headers = network::mojom::URLResponseHead::New();
  head_with_no_headers->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  CallMaybeSetResponseHead(&manager, *head_with_no_headers);
  EXPECT_FALSE(version_->GetResponseHeadForSyntheticResponse());
}

TEST_F(ServiceWorkerSyntheticResponseManagerTest,
       MaybeStartSyntheticResponse_TimingAllowPassed) {
  for (bool expected_timing_allow_passed : {false, true}) {
    auto valid_head = network::mojom::URLResponseHead::New();
    valid_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
    valid_head->headers->AddHeader("Service-Worker-Synthetic-Response", "?1");
    valid_head->headers->AddHeader("Content-Security-Policy",
                                   "default-src 'none'");
    valid_head->timing_allow_passed = expected_timing_allow_passed;
    version_->SetResponseHeadForSyntheticResponse(std::move(valid_head));

    ServiceWorkerSyntheticResponseManager manager(version_);

    blink::mojom::FetchAPIResponsePtr received_response;
    EXPECT_TRUE(manager.MaybeStartSyntheticResponse(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            ServiceWorkerFetchDispatcher::FetchEventResult result,
            blink::mojom::FetchAPIResponsePtr response,
            blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
            blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
            blink::mojom::ServiceWorkerFetchHandlerErrorsPtr errors,
            scoped_refptr<ServiceWorkerVersion> version) {
          received_response = std::move(response);
        })));
    ASSERT_TRUE(received_response);
    EXPECT_EQ(expected_timing_allow_passed,
              received_response->timing_allow_passed);
  }
}

TEST_F(ServiceWorkerSyntheticResponseManagerTest,
       MaybeStartSyntheticResponse_ResponseSourceIsNetwork) {
  auto valid_head = network::mojom::URLResponseHead::New();
  valid_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  valid_head->headers->AddHeader("Service-Worker-Synthetic-Response", "?1");
  valid_head->headers->AddHeader("Content-Security-Policy",
                                 "default-src 'none'");
  version_->SetResponseHeadForSyntheticResponse(std::move(valid_head));

  ServiceWorkerSyntheticResponseManager manager(version_);
  ASSERT_EQ(
      manager.Status(),
      ServiceWorkerSyntheticResponseManager::SyntheticResponseStatus::kReady);

  blink::mojom::FetchAPIResponsePtr received_response;
  bool started = manager.MaybeStartSyntheticResponse(base::BindOnce(
      [](blink::mojom::FetchAPIResponsePtr* out_response,
         blink::ServiceWorkerStatusCode status,
         ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
         blink::mojom::FetchAPIResponsePtr response,
         blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
         blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
         blink::mojom::ServiceWorkerFetchHandlerErrorsPtr errors,
         scoped_refptr<ServiceWorkerVersion> version) {
        *out_response = std::move(response);
      },
      &received_response));
  EXPECT_TRUE(started);
  ASSERT_TRUE(received_response);
  EXPECT_EQ(received_response->response_source,
            network::mojom::FetchResponseSource::kNetwork);
}

// Verifies that on a header mismatch during `OnReceiveResponse`, the stored
// header remains cleared when `refresh_header_on_mismatch` is disabled, is
// immediately updated when enabled and the new response still opts in, and
// stays cleared when the new response drops opt-in.
TEST_F(ServiceWorkerSyntheticResponseManagerTest,
       HeaderMismatchRefreshesStoredHeaderWhenEnabled) {
  auto make_head = [](std::string_view csp, bool opt_in) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (opt_in) {
      head->headers->AddHeader("Service-Worker-Synthetic-Response", "?1");
    }
    head->headers->AddHeader("Content-Security-Policy", csp);
    return head;
  };

  auto run_mismatch = [&](network::mojom::URLResponseHeadPtr network_head) {
    ServiceWorkerSyntheticResponseManager manager(version_);
    InitializeWriteBufferManager(&manager);
    auto stream_callback = std::make_unique<MockServiceWorkerStreamCallback>();
    SetStreamCallback(&manager, stream_callback->GetRemote());
    CallOnReceiveResponse(&manager, std::move(network_head));
    stream_callback->Wait();
  };

  // 1. Default parameter (`refresh_header_on_mismatch` = false) drops the
  // stored header even if the mismatched response still opts in. Scope
  // `kServiceWorkerSyntheticResponse` with default parameters so the
  // `off_main_thread=network_service` testing config override does not bypass
  // browser-side mismatch handling.
  {
    base::test::ScopedFeatureList default_feature_list(
        blink::features::kServiceWorkerSyntheticResponse);
    version_->SetResponseHeadForSyntheticResponse(
        make_head("default-src 'none'", /*opt_in=*/true));
    run_mismatch(make_head("default-src 'self'", /*opt_in=*/true));
    EXPECT_FALSE(version_->GetResponseHeadForSyntheticResponse());
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kServiceWorkerSyntheticResponse,
      {{blink::features::kServiceWorkerSyntheticResponseRefreshHeaderOnMismatch
            .name,
        "true"}});

  // 2. When enabled, a mismatched response that still opts in replaces the
  // stored header immediately.
  version_->SetResponseHeadForSyntheticResponse(
      make_head("default-src 'none'", /*opt_in=*/true));
  run_mismatch(make_head("default-src 'self'", /*opt_in=*/true));
  ASSERT_TRUE(version_->GetResponseHeadForSyntheticResponse());
  EXPECT_TRUE(
      version_->GetResponseHeadForSyntheticResponse()->headers->HasHeaderValue(
          "Content-Security-Policy", "default-src 'self'"));

  // 3. A mismatched response without opt-in leaves the stored header cleared.
  run_mismatch(make_head("default-src 'none'", /*opt_in=*/false));
  EXPECT_FALSE(version_->GetResponseHeadForSyntheticResponse());
}

}  // namespace content
