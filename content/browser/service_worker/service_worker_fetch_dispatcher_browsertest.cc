// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"

namespace content {

namespace {

struct FetchResult {
  blink::ServiceWorkerStatusCode status;
  ServiceWorkerFetchDispatcher::FetchEventResult result;
  network::mojom::FetchResponseSource response_source;
  uint16_t response_status_code;

  bool operator==(const FetchResult& other) const {
    return status == other.status && result == other.result &&
           response_source == other.response_source &&
           response_status_code == other.response_status_code;
  }
};

const FetchResult kNetworkCompleted = FetchResult{
    blink::ServiceWorkerStatusCode::kOk,
    ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
    network::mojom::FetchResponseSource::kNetwork,
    200,
};

const FetchResult kTimeout = FetchResult{
    blink::ServiceWorkerStatusCode::kErrorTimeout,
    ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
    network::mojom::FetchResponseSource::kUnspecified,
    0,
};

}  // namespace

// An observer that waits for the service worker to be running.
class WorkerRunningStatusObserver : public ServiceWorkerContextObserver {
 public:
  explicit WorkerRunningStatusObserver(ServiceWorkerContext* context) {
    scoped_context_observation_.Observe(context);
  }

  WorkerRunningStatusObserver(const WorkerRunningStatusObserver&) = delete;
  WorkerRunningStatusObserver& operator=(const WorkerRunningStatusObserver&) =
      delete;

  ~WorkerRunningStatusObserver() override = default;

  int64_t version_id() { return version_id_; }

  void WaitUntilRunning() {
    if (version_id_ == blink::mojom::kInvalidServiceWorkerVersionId)
      run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ServiceWorkerContext, ServiceWorkerContextObserver>
      scoped_context_observation_{this};
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
};

// An observer that waits until all inflight events complete.
class NoWorkObserver : public ServiceWorkerVersion::Observer {
 public:
  explicit NoWorkObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  void OnNoWork(ServiceWorkerVersion* version) override {
    EXPECT_TRUE(version->HasNoWork());
    DCHECK(closure_);
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
};

class ServiceWorkerFetchDispatcherBrowserTest : public ContentBrowserTest {
 public:
  using self = ServiceWorkerFetchDispatcherBrowserTest;

  ~ServiceWorkerFetchDispatcherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext()));
  }

  void TearDownOnMainThread() override { wrapper_.reset(); }

  std::unique_ptr<ServiceWorkerFetchDispatcher> CreateFetchDispatcher(
      base::OnceClosure done,
      const std::string& path,
      ServiceWorkerVersion* version,
      FetchResult* result) {
    GURL url = embedded_test_server()->GetURL(path);
    network::mojom::RequestDestination destination =
        network::mojom::RequestDestination::kDocument;
    ServiceWorkerFetchDispatcher::FetchCallback fetch_callback =
        CreateResponseReceiver(std::move(done), result);

    auto request = blink::mojom::FetchAPIRequest::New();
    request->url = url;
    request->method = "GET";
    request->is_main_resource_load = true;

    return std::make_unique<ServiceWorkerFetchDispatcher>(
        std::move(request), destination, std::string() /* client_id */,
        std::string() /* resulting_client_id */, version,
        base::DoNothing() /* prepare_result */, std::move(fetch_callback));
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
    out_result->status = actual_status;
    out_result->result = actual_result;
    out_result->response_source = actual_response->response_source;
    out_result->response_status_code = actual_response->status_code;
    if (!quit.is_null())
      std::move(quit).Run();
  }

  ServiceWorkerFetchDispatcher::FetchCallback CreateResponseReceiver(
      base::OnceClosure quit,
      FetchResult* result) {
    return base::BindOnce(&self::ReceiveFetchResult, base::Unretained(this),
                          std::move(quit), result);
  }

  // Starts the test server and navigates the renderer to an empty page. Call
  // this after adding all request handlers to the test server. Adding handlers
  // after the test server has started is not allowed.
  void StartServerAndNavigateToSetup() {
    embedded_test_server()->StartAcceptingConnections();

    // Navigate to the page to set up a renderer page (where we can embed
    // a worker).
    NavigateToURLBlockUntilNavigationsComplete(
        shell(), embedded_test_server()->GetURL("/service_worker/empty.html"),
        1);
  }

  ServiceWorkerVersion* CreateVersion() {
    WorkerRunningStatusObserver observer(wrapper());
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('maybe_offline_support.js');"));
    observer.WaitUntilRunning();
    return wrapper()->GetLiveVersion(observer.version_id());
  }

  void WaitForNoWork(ServiceWorkerVersion* version) {
    // Set up a custom timeout for waiting for the service worker becomes an
    // idle state on the renderer and it has no work on the browser. The default
    // delay to become an idle is 30 seconds
    // (kServiceWorkerDefaultIdleDelayInSeconds).
    base::test::ScopedRunLoopTimeout specific_timeout(FROM_HERE,
                                                      base::Seconds(35));
    base::RunLoop run_loop;
    NoWorkObserver observer(run_loop.QuitClosure());
    version->AddObserver(&observer);
    run_loop.Run();
    version->RemoveObserver(&observer);
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }

 protected:
  base::SimpleTestTickClock tick_clock_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Regression test for https://crbug.com/1145551.
// This is the normal case that the lifetime of a fetch event is longer than
// the response finishes. ServiceWorkerFetchDispatcher::HandleResponse() is
// called first.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFetchDispatcherBrowserTest, FetchEvent) {
  StartServerAndNavigateToSetup();
  ServiceWorkerVersion* version = CreateVersion();

  FetchResult fetch_result;
  base::RunLoop fetch_run_loop;
  std::unique_ptr<ServiceWorkerFetchDispatcher> dispatcher =
      CreateFetchDispatcher(
          fetch_run_loop.QuitClosure(),
          "/service_worker/empty.html?sleep_then_fetch&sleep=0", version,
          &fetch_result);
  dispatcher->Run();
  fetch_run_loop.Run();

  EXPECT_FALSE(version->HasNoWork());
  EXPECT_EQ(kNetworkCompleted, fetch_result);

  // Destruction of FetchDispatcher must not prevent FinishRequest() from being
  // called.
  dispatcher.reset();
  WaitForNoWork(version);

  EXPECT_TRUE(version->HasNoWork());
}

// Regression test for https://crbug.com/1145551.
// This is the timeout case that the lifetime of a fetch event is shorter than
// the response finishes. ServiceWorkerFetchDispatcher::OnFetchEventFinished is
// called first.
IN_PROC_BROWSER_TEST_F(ServiceWorkerFetchDispatcherBrowserTest,
                       FetchEventTimeout) {
  StartServerAndNavigateToSetup();
  ServiceWorkerVersion* version = CreateVersion();
  version->SetTickClockForTesting(&tick_clock_);

  FetchResult fetch_result;
  base::RunLoop fetch_run_loop;
  std::unique_ptr<ServiceWorkerFetchDispatcher> dispatcher =
      CreateFetchDispatcher(
          fetch_run_loop.QuitClosure(),
          // The time out (500sec) longer than
          // `kTestTimeoutBeyondRequestTimeout` below to make the request not
          // served before the fetch event timeout.
          "/service_worker/empty.html?sleep_then_fetch&sleep=500000", version,
          &fetch_result);
  dispatcher->Run();

  constexpr base::TimeDelta kTestTimeoutBeyondRequestTimeout =
      // Value of kRequestTimeout in service_worker_version.cc and
      // `ServiceWorkerEventQueue::kEventTimeout`.
      base::Minutes(5) +
      // A little past that.
      base::Minutes(1);

  // Now advance time to make the fetch event to timeout.
  // This seems trigger the timeout in `ServiceWorkerVersion` (not in
  // `ServiceWorkerEventQueue` in the renderer process), but probably it's
  // sufficient to test the code in `ServiceWorkerFetchDispatcher` in the
  // browser process.
  tick_clock_.Advance(kTestTimeoutBeyondRequestTimeout);
  version->timeout_timer_.user_task().Run();

  fetch_run_loop.Run();

  EXPECT_FALSE(version->HasNoWork());
  EXPECT_EQ(kTimeout, fetch_result);

  // Destruction of FetchDispatcher must not prevent FinishRequest() from being
  // called.
  dispatcher.reset();
  WaitForNoWork(version);

  EXPECT_TRUE(version->HasNoWork());
}

}  // namespace content
