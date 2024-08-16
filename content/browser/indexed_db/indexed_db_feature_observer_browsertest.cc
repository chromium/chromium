// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/feature_observer.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

namespace {

void RunLoopWithTimeout() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

class TestBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  explicit TestBrowserClient(FeatureObserverClient* feature_observer_client)
      : feature_observer_client_(feature_observer_client) {}
  ~TestBrowserClient() override = default;

  FeatureObserverClient* GetFeatureObserverClient() override {
    return feature_observer_client_;
  }

  TestBrowserClient(const TestBrowserClient&) = delete;
  TestBrowserClient& operator=(const TestBrowserClient&) = delete;

 private:
  raw_ptr<FeatureObserverClient> feature_observer_client_;
};

class MockObserverClient : public FeatureObserverClient {
 public:
  MockObserverClient() = default;
  ~MockObserverClient() override = default;

  // PerformanceManagerFeatureObserver implementation:
  MOCK_METHOD(void,
              OnStartUsing,
              (GlobalRenderFrameHostId id,
               blink::mojom::ObservedFeatureType type,
               uint32_t name_hash));
  MOCK_METHOD(void,
              OnStopUsing,
              (GlobalRenderFrameHostId id,
               blink::mojom::ObservedFeatureType type,
               uint32_t name_hash));
};

class IndexedDBFeatureObserverBrowserTest : public ContentBrowserTest {
 public:
  IndexedDBFeatureObserverBrowserTest() = default;
  ~IndexedDBFeatureObserverBrowserTest() override = default;

  // Check if the test can run on the current system. If the test can run,
  // navigates to the test page and returns true. Otherwise, returns false.
  bool CheckShouldRunTestAndNavigate() const {
    EXPECT_TRUE(NavigateToURL(shell(), GetTestURL("a.com")));
    return true;
  }

  GURL GetTestURL(const std::string& hostname) const {
    return server_.GetURL(hostname,
                          "/indexeddb/open_connection/open_connection.html");
  }

 protected:
  testing::StrictMock<MockObserverClient> mock_observer_client_;

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    browser_client_ =
        std::make_unique<TestBrowserClient>(&mock_observer_client_);

    host_resolver()->AddRule("*", "127.0.0.1");
    server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(server_.Start());
  }

  void TearDownOnMainThread() override {
    browser_client_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<TestBrowserClient> browser_client_;

  IndexedDBFeatureObserverBrowserTest(
      const IndexedDBFeatureObserverBrowserTest&) = delete;
  IndexedDBFeatureObserverBrowserTest& operator=(
      const IndexedDBFeatureObserverBrowserTest&) = delete;
};

bool OpenConnection(RenderFrameHost* rfh, std::string_view name) {
  static const char kScript[] = R"(
    (async () => {
      return await OpenConnection($1)
    }) ();
  )";

  return EvalJs(rfh, JsReplace(kScript, name)).ExtractBool();
}

bool CloseConnection(RenderFrameHost* rfh, std::string_view name) {
  return ExecJs(rfh, JsReplace("CloseConnection($1)", name));
}

static const char kNameA[] = "A";
static uint32_t kNameHashA = 7850657u;
static const char kNameB[] = "B";
static uint32_t kNameHashB = 13313561u;

}  // namespace

// Verify that content::FeatureObserver is notified when a frame opens/closes an
// IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest,
                       ObserverSingleConnection) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id,
                     blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                     kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnection(rfh, kNameA));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Close the connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                    kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(CloseConnection(rfh, kNameA));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified when a frame opens multiple
// IndexedDB connections (notifications only when the number of held connections
// switches between zero and non-zero).
// Disabled on ChromeOS release build for flakiness. See crbug.com/1030733.
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(NDEBUG)
#define MAYBE_ObserverTwoLocks DISABLED_ObserverTwoLocks
#else
#define MAYBE_ObserverTwoLocks ObserverTwoLocks
#endif
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest,
                       MAYBE_ObserverTwoLocks) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id,
                     blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                     kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnection(rfh, kNameA));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Open a second connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id,
                     blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                     kNameHashB))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnection(rfh, kNameB));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Close the connection B. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                    kNameHashB))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(CloseConnection(rfh, kNameB));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Close the connection A. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                    kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(CloseConnection(rfh, kNameA));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified when a frame with active
// IndexedDB connections is navigated away.
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest, ObserverNavigate) {
  // The test expects the OnStopUsing() method to be called, which won't happen
  // if the BackForwardCache is enabled.
  // TODO(crbug.com/40777894): Figure out why this is happening.
  shell()
      ->web_contents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id,
                     blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                     kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnection(rfh, kNameA));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Navigate away. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kIndexedDBConnection,
                    kNameHashA))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType,
                      uint32_t) { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(shell(), GetTestURL("b.com")));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(rfh_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is *not* notified when a dedicated
// worker opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest,
                       ObserverDedicatedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await OpenConnectionFromDedicatedWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}

// SharedWorkers are not enabled on Android. https://crbug.com/154571
#if !BUILDFLAG(IS_ANDROID)
// Verify that content::FeatureObserver is *not* notified when a shared worker
// opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest,
                       ObserverSharedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await OpenConnectionFromSharedWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that content::FeatureObserver is *not* notified when a service worker
// opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBFeatureObserverBrowserTest,
                       ObserverServiceWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await OpenConnectionFromServiceWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}

}  // namespace content
