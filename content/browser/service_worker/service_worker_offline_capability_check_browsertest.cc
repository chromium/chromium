// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

struct FetchResult {
  blink::ServiceWorkerStatusCode status;
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  blink::mojom::FetchAPIResponsePtr response;
};

class FetchEventTestHelper {
 public:
  struct FetchEventDispatchParam {
    std::string path;
    bool is_offline_capability_check;
  };

  struct ExpectedResult {
    blink::ServiceWorkerStatusCode status;
    ServiceWorkerFetchDispatcher::FetchEventResult result;
    network::mojom::FetchResponseSource response_source;
    uint16_t response_status_code;
  };

  struct FetchEventDispatchParamAndExpectedResult {
    FetchEventDispatchParam param;
    ExpectedResult expected_result;
  };

  FetchEventTestHelper(
      const std::vector<FetchEventDispatchParamAndExpectedResult>&
          test_inputs) {
    for (const FetchEventDispatchParamAndExpectedResult&
             param_and_expected_result : test_inputs) {
      fetch_event_dispatches_.push_back(
          FetchEventDispatch{param_and_expected_result, absl::nullopt});
    }
  }

  // `done_barrier_closure` is called each time a fetch event is dispatched.
  void DispatchFetchEvents(base::RepeatingClosure done_barrier_closure,
                           net::EmbeddedTestServer* embedded_test_server,
                           scoped_refptr<ServiceWorkerVersion> version) {
    ASSERT_TRUE(version->status() == ServiceWorkerVersion::ACTIVATING ||
                version->status() == ServiceWorkerVersion::ACTIVATED);

    for (FetchEventDispatch& fetch_event_dispatch : fetch_event_dispatches_) {
      Fetch(done_barrier_closure, embedded_test_server, version,
            &fetch_event_dispatch);
    }
  }

  void CheckResult() {
    for (FetchEventDispatch& dispatch : fetch_event_dispatches_) {
      ExpectedResult& expected =
          dispatch.param_and_expected_result.expected_result;
      ASSERT_TRUE(dispatch.fetch_result.has_value());
      FetchResult& result = *dispatch.fetch_result;

      EXPECT_EQ(expected.status, result.status);
      EXPECT_EQ(expected.result, result.result);
      EXPECT_EQ(expected.response_source, result.response->response_source);
      EXPECT_EQ(expected.response_status_code, result.response->status_code);
    }
  }

 private:
  struct FetchEventDispatch {
    FetchEventDispatchParamAndExpectedResult param_and_expected_result;
    absl::optional<FetchResult> fetch_result;
    std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher;
  };

  void FetchCallback(
      base::RepeatingClosure done_barrier_closure,
      FetchEventDispatch* fetch_event_dispatch,
      blink::ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchDispatcher::FetchEventResult actual_result,
      blink::mojom::FetchAPIResponsePtr actual_response,
      blink::mojom::ServiceWorkerStreamHandlePtr /* stream */,
      blink::mojom::ServiceWorkerFetchEventTimingPtr /* timing */,
      scoped_refptr<ServiceWorkerVersion> /* worker */) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_FALSE(fetch_event_dispatch->fetch_result.has_value());
    fetch_event_dispatch->fetch_result = FetchResult{
        actual_status,
        std::move(actual_result),
        std::move(actual_response),
    };

    done_barrier_closure.Run();
  }

  void Fetch(base::RepeatingClosure done_barrier_closure,
             net::EmbeddedTestServer* embedded_test_server,
             scoped_refptr<ServiceWorkerVersion> version,
             FetchEventDispatch* fetch_event_dispatch) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));

    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback = base::BindOnce(
        &FetchEventTestHelper::FetchCallback, base::Unretained(this),
        done_barrier_closure, fetch_event_dispatch);

    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = embedded_test_server->GetURL(
        fetch_event_dispatch->param_and_expected_result.param.path);
    request->method = "GET";
    request->is_main_resource_load = true;

    fetch_event_dispatch->fetch_dispatcher =
        std::make_unique<ServiceWorkerFetchDispatcher>(
            std::move(request), network::mojom::RequestDestination::kDocument,
            base::GenerateGUID() /* client_id */, std::move(version),
            base::DoNothing() /* prepare callback */, std::move(fetch_callback),
            fetch_event_dispatch->param_and_expected_result.param
                .is_offline_capability_check);
    fetch_event_dispatch->fetch_dispatcher->Run();
  }

  std::vector<FetchEventDispatch> fetch_event_dispatches_;
};

}  // namespace

class ServiceWorkerOfflineCapabilityCheckBrowserTest
    : public ContentBrowserTest {
 public:
  ServiceWorkerOfflineCapabilityCheckBrowserTest() = default;

  ServiceWorkerOfflineCapabilityCheckBrowserTest(
      const ServiceWorkerOfflineCapabilityCheckBrowserTest&) = delete;
  ServiceWorkerOfflineCapabilityCheckBrowserTest& operator=(
      const ServiceWorkerOfflineCapabilityCheckBrowserTest&) = delete;

  void SetUp() override { ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }

  void SetupFetchEventDispatchTargetVersion() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!version_);
    base::RunLoop run_loop;
    GURL url = embedded_test_server()->GetURL("/service_worker/");
    blink::StorageKey key = blink::StorageKey(url::Origin::Create(url));
    wrapper()->context()->registry()->FindRegistrationForScope(
        embedded_test_server()->GetURL("/service_worker/"), key,
        base::BindOnce(&ServiceWorkerOfflineCapabilityCheckBrowserTest::
                           DidFindRegistration,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    DCHECK(version_);
  }

  void DidFindRegistration(
      base::OnceClosure done,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration) {
    DCHECK_EQ(status, blink::ServiceWorkerStatusCode::kOk);
    DCHECK(!registration->waiting_version());
    version_ = registration->active_version();
    std::move(done).Run();
  }

  void RunFetchEventDispatchTest(
      std::vector<
          FetchEventTestHelper::FetchEventDispatchParamAndExpectedResult>
          fetch_event_dispatches) {
    base::RunLoop fetch_run_loop;

    // Use BarrierClosure to wait for all fetch event dispatches to finish.
    auto barrier_closure = base::BarrierClosure(fetch_event_dispatches.size(),
                                                fetch_run_loop.QuitClosure());

    FetchEventTestHelper test_helper(fetch_event_dispatches);
    test_helper.DispatchFetchEvents(barrier_closure, embedded_test_server(),
                                    version_);
    fetch_run_loop.Run();
    test_helper.CheckResult();
  }

  OfflineCapability CheckOfflineCapability(const std::string& path) {
    base::RunLoop fetch_run_loop;
    absl::optional<OfflineCapability> out_offline_capability;
    GURL url = embedded_test_server()->GetURL(path);
    wrapper()->CheckOfflineCapability(
        url, blink::StorageKey(url::Origin::Create(url)),
        base::BindLambdaForTesting(
            [&out_offline_capability, &fetch_run_loop](
                OfflineCapability offline_capability, int64_t registration_id) {
              out_offline_capability = offline_capability;
              if (offline_capability == OfflineCapability::kSupported) {
                EXPECT_NE(registration_id,
                          blink::mojom::kInvalidServiceWorkerRegistrationId);
              }
              fetch_run_loop.Quit();
            }));
    fetch_run_loop.Run();
    DCHECK(out_offline_capability.has_value());
    return *out_offline_capability;
  }

 protected:
  // Expected results which are used in the tests.
  const FetchEventTestHelper::ExpectedResult kNetworkCompleted =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
          network::mojom::FetchResponseSource::kNetwork,
          200,
      };

  const FetchEventTestHelper::ExpectedResult kOfflineCompleted =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
          network::mojom::FetchResponseSource::kUnspecified,
          200,
      };

  const FetchEventTestHelper::ExpectedResult kRedirect =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
          network::mojom::FetchResponseSource::kUnspecified,
          301,
      };

  const FetchEventTestHelper::ExpectedResult kFailed =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
          network::mojom::FetchResponseSource::kUnspecified,
          0,
      };

  const FetchEventTestHelper::ExpectedResult kNotFound =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
          network::mojom::FetchResponseSource::kNetwork,
          404,
      };

  const FetchEventTestHelper::ExpectedResult kShouldFallback =
      FetchEventTestHelper::ExpectedResult{
          blink::ServiceWorkerStatusCode::kOk,
          ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
          network::mojom::FetchResponseSource::kUnspecified,
          0,
      };

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       DispatchOfflineCapabilityCheckFetchEvent) {
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('maybe_offline_support.js');"));
  SetupFetchEventDispatchTargetVersion();

  // For a better readability in this test.
  bool normal = false;
  bool is_offline_capability_check = true;

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html",
          normal,
      },
      kShouldFallback,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html",
          is_offline_capability_check,
      },
      kShouldFallback,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?fetch",
          normal,
      },
      kNetworkCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?fetch",
          is_offline_capability_check,
      },
      kFailed,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/not-found.html?fetch",
          normal,
      },
      kNotFound,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/not-found.html?fetch",
          is_offline_capability_check,
      },
      kFailed,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?offline",
          normal,
      },
      kOfflineCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?offline",
          is_offline_capability_check,
      },
      kOfflineCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?fetch_or_offline",
          normal,
      },
      kNetworkCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?fetch_or_offline",
          is_offline_capability_check,
      },
      kOfflineCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?sleep_then_offline",
          normal,
      },
      kOfflineCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?sleep_then_offline",
          is_offline_capability_check,
      },
      kOfflineCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?sleep_then_fetch",
          normal,
      },
      kNetworkCompleted,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?sleep_then_fetch",
          is_offline_capability_check,
      },
      kFailed,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?redirect",
          normal,
      },
      kRedirect,
  }});

  RunFetchEventDispatchTest({{
      {
          "/service_worker/empty.html?redirect",
          is_offline_capability_check,
      },
      kRedirect,
  }});
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       DispatchOfflineCapabilityCheckFetchEventMoreThanOnce) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('maybe_offline_support.js');"));
  SetupFetchEventDispatchTargetVersion();

  // For a better readability in this test.
  bool normal = false;
  bool is_offline_capability_check = true;

  // 1. normal -> 2. normal test case.
  RunFetchEventDispatchTest({{
                                 {
                                     "/service_worker/empty.html?fetch",
                                     normal,
                                 },
                                 kNetworkCompleted,
                             },
                             {
                                 {
                                     "/service_worker/empty.html?fetch",
                                     normal,
                                 },
                                 kNetworkCompleted,
                             }});

  // // 1. offline -> 2. normal test cases.
  RunFetchEventDispatchTest({{
                                 {
                                     "/service_worker/empty.html?fetch",
                                     is_offline_capability_check,
                                 },
                                 kFailed,
                             },
                             {
                                 {
                                     "/service_worker/empty.html?fetch",
                                     normal,
                                 },
                                 kNetworkCompleted,
                             }});

  // TODO(hayato): Find a reliable way to control the order of
  // the execution. Currently, maybe_offline_support.js uses setTimeout so that
  // 1st fetch event is still running when 2nd fetch event comes.
  RunFetchEventDispatchTest(
      {{
           {
               "/service_worker/empty.html?sleep_then_fetch",
               is_offline_capability_check,
           },
           kFailed,
       },
       {
           {
               "/service_worker/empty.html?fetch",
               normal,
           },
           // This fetch event should be enqueued before 1st fetch event
           // finishes.
           kNetworkCompleted,
       }});

  // 1. normal -> 2. offline test cases.
  RunFetchEventDispatchTest({{
                                 {
                                     "/service_worker/empty.html?fetch",
                                     normal,
                                 },
                                 kNetworkCompleted,
                             },
                             {
                                 {
                                     "/service_worker/empty.html?fetch",
                                     is_offline_capability_check,
                                 },
                                 kFailed,
                             }});

  RunFetchEventDispatchTest(
      {{
           {
               "/service_worker/empty.html?sleep_then_fetch",
               normal,
           },
           kNetworkCompleted,
       },
       {
           {
               "/service_worker/empty.html?fetch",
               is_offline_capability_check,
           },
           kFailed,
       }});

  // 1. offline -> 2. offline test cases
  RunFetchEventDispatchTest({{
                                 {
                                     "/service_worker/empty.html?offline",
                                     is_offline_capability_check,
                                 },
                                 kOfflineCompleted,
                             },
                             {
                                 {
                                     "/service_worker/empty.html?offline",
                                     is_offline_capability_check,
                                 },
                                 kOfflineCompleted,
                             }});

  RunFetchEventDispatchTest(
      {{
           {
               "/service_worker/empty.html?sleep_then_offline",
               is_offline_capability_check,
           },
           kOfflineCompleted,
       },
       {{
            "/service_worker/empty.html?offline",
            is_offline_capability_check,
        },
        kOfflineCompleted}});

  // 1. normal -> 2. offline -> 3. normal
  RunFetchEventDispatchTest(
      {{
           {"/service_worker/empty.html?sleep_then_fetch", normal},
           kNetworkCompleted,
       },
       {
           {
               "/service_worker/empty.html?sleep_then_fetch",
               is_offline_capability_check,
           },
           kFailed,
       },
       {{
            "/service_worker/empty.html?fetch",
            normal,
        },
        kNetworkCompleted}});
}

// Sites without a service worker are identified as having no offline capability
// support.
IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       CheckOfflineCapabilityForNoServiceWorker) {
  // We don't install ServiceWorker in this test.
  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html"));
  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/not_found.html"));
}

// Sites with a no-fetch-handler service worker are identified as having no
// offline capability support.
IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       CheckOfflineCapabilityForNoFetchHandler) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  // Install ServiceWorker which does not have any event handler.
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js')"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html"));
  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/not_found.html"));
}

// Sites with a service worker are identified as supporting offline capability
// only when it returns a valid response in the offline mode.

// TODO(crbug.com/1365409): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       DISABLED_CheckOfflineCapability) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('maybe_offline_support.js')"));

  // At this point, a service worker's status is ACTIVATING or ACTIVATED because
  // register() awaits navigator.serviceWorker.ready promise.

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/out_of_scope.html"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/out_of_scope.html?offline"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html?fetch"));

  EXPECT_EQ(OfflineCapability::kSupported,
            CheckOfflineCapability("/service_worker/empty.html?offline"));

  EXPECT_EQ(
      OfflineCapability::kSupported,
      CheckOfflineCapability("/service_worker/empty.html?fetch_or_offline"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html?cache_add"));

  EXPECT_EQ(
      OfflineCapability::kSupported,
      CheckOfflineCapability("/service_worker/empty.html?sleep_then_offline"));

  // Site that takes more than the timeout is considered as "offline capable".
  EXPECT_EQ(OfflineCapability::kSupported,
            CheckOfflineCapability(
                "/service_worker/empty.html?sleep_then_offline&sleep=20000"));

  EXPECT_EQ(
      OfflineCapability::kUnsupported,
      CheckOfflineCapability("/service_worker/empty.html?sleep_then_fetch"));

  // Site that takes more than the timeout is considered as "offline capable".
  EXPECT_EQ(OfflineCapability::kSupported,
            CheckOfflineCapability(
                "/service_worker/empty.html?sleep_then_fetch&sleep=20000"));

  EXPECT_EQ(OfflineCapability::kSupported,
            CheckOfflineCapability("/service_worker/empty.html?redirect"));
}

// Sites with a service worker which is not activated yet are identified as
// having no offline capability support.
IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       CheckOfflineCapabilityForInstallingServiceWorker) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  // Appends |pendingInstallEvent| URL param to prevent a service worker from
  // being activated.
  EXPECT_EQ("DONE",
            EvalJs(shell(),
                   "registerWithoutAwaitingReady('maybe_offline_support.js?"
                   "pendingInstallEvent')"));
  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html?offline"));
}

// Sites with a service worker that enable navigation preload are identified
// as supporting offline capability only when they return a valid response in
// offline mode.
IN_PROC_BROWSER_TEST_F(ServiceWorkerOfflineCapabilityCheckBrowserTest,
                       CheckOfflineCapabilityForNavigationPreload) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(),
                           "register('navigation_preload_worker.js')"));

  EXPECT_EQ(OfflineCapability::kUnsupported,
            CheckOfflineCapability("/service_worker/empty.html"));

  EXPECT_EQ(
      OfflineCapability::kSupported,
      CheckOfflineCapability(
          "/service_worker/empty.html?navpreload_or_offline"));
}

}  // namespace content
