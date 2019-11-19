// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"

#include "base/macros.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/lock_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

namespace {

void RunLoopWithTimeout() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

class TestBrowserClient : public ContentBrowserClient {
 public:
  explicit TestBrowserClient(LockObserver* lock_observer)
      : lock_observer_(lock_observer) {}
  ~TestBrowserClient() override = default;

  LockObserver* GetLockObserver() override { return lock_observer_; }

 private:
  LockObserver* const lock_observer_;

  TestBrowserClient(const TestBrowserClient&) = delete;
  TestBrowserClient& operator=(const TestBrowserClient&) = delete;
};

class MockObserver : public LockObserver {
 public:
  MockObserver() = default;
  ~MockObserver() = default;

  // LockObserver:
  MOCK_METHOD2(OnFrameStartsHoldingWebLocks,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(OnFrameStopsHoldingWebLocks,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(OnFrameStartsHoldingIndexedDBConnections,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(OnFrameStopsHoldingIndexedDBConnections,
               void(int render_process_id, int render_frame_id));
};

class IndexedDBExecutionContextConnectionTrackerBrowserTest
    : public ContentBrowserTest {
 public:
  IndexedDBExecutionContextConnectionTrackerBrowserTest() = default;
  ~IndexedDBExecutionContextConnectionTrackerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    original_client_ = SetBrowserClientForTesting(&test_browser_client_);

    host_resolver()->AddRule("*", "127.0.0.1");
    server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(server_.Start());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  // Check if the test can run on the current system. If the test can run,
  // navigates to the test page and returns true. Otherwise, returns false.
  bool CheckShouldRunTestAndNavigate() const {
#if defined(OS_ANDROID)
    // Don't run the test if we couldn't override BrowserClient. It happens only
    // on Android Kitkat or older systems.
    if (!original_client_)
      return false;

    // TODO(https://crbug.com/1011765, https://crbug.com/1019659):
    // Navigation fails on Android Kit Kat.
    if (base::android::BuildInfo::GetInstance()->sdk_int() <=
        base::android::SDK_VERSION_KITKAT) {
      return false;
    }
#endif  // defined(OS_ANDROID)
    EXPECT_TRUE(NavigateToURL(shell(), GetTestURL("a.com")));
    return true;
  }

  GURL GetTestURL(const std::string& hostname) const {
    return server_.GetURL(hostname,
                          "/indexeddb/open_connection/open_connection.html");
  }

  testing::StrictMock<MockObserver> mock_observer_;

 private:
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  ContentBrowserClient* original_client_ = nullptr;
  TestBrowserClient test_browser_client_{&mock_observer_};

  IndexedDBExecutionContextConnectionTrackerBrowserTest(
      const IndexedDBExecutionContextConnectionTrackerBrowserTest&) = delete;
  IndexedDBExecutionContextConnectionTrackerBrowserTest& operator=(
      const IndexedDBExecutionContextConnectionTrackerBrowserTest&) = delete;
};

bool OpenConnectionA(RenderFrameHost* rfh) {
  return EvalJs(rfh, R"(
      (async () => {
        return await OpenConnection('A');
      }) ();
  )")
      .ExtractBool();
}

bool OpenConnectionB(RenderFrameHost* rfh) {
  return EvalJs(rfh, R"(
      (async () => {
        return await OpenConnection('B');
      }) ();
  )")
      .ExtractBool();
}

}  // namespace

// Verify that content::LockObserver is notified when a frame opens/closes an
// IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverSingleConnection) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  int frame_id = rfh->GetRoutingID();
  int process_id = rfh->GetProcess()->GetID();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnectionA(rfh));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Close the connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "CloseConnection('A');"));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::LockObserver is notified when a frame opens multiple
// IndexedDB connections (notifications only when the number of held connections
// switches between zero and non-zero).
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverTwoLocks) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  int frame_id = rfh->GetRoutingID();
  int process_id = rfh->GetProcess()->GetID();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnectionA(rfh));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }

  // Open a second connection. Don't expect a notification.
  EXPECT_TRUE(OpenConnectionB(rfh));
  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();

  // Close the connection. Don't expect a notification.
  EXPECT_TRUE(ExecJs(rfh, "CloseConnection('B');"));
  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();

  {
    // Close the connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "CloseConnection('A');"));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::LockObserver is notified when a frame with active
// IndexedDB connections is navigated away.
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverNavigate) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  int frame_id = rfh->GetRoutingID();
  int process_id = rfh->GetProcess()->GetID();

  {
    // Open a connection. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(OpenConnectionA(rfh));
    // Quit when OnFrameStartsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }

  {
    // Navigate away. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_,
                OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id))
        .WillOnce([&](int, int) { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(shell(), GetTestURL("b.com")));
    // Quit when OnFrameStopsHoldingIndexedDBConnections(process_id, frame_id)
    // is invoked.
    run_loop.Run();
  }
}

// Verify that content::LockObserver is *not* notified when a dedicated worker
// opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverDedicatedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
#if !defined(OS_ANDROID)
// Verify that content::LockObserver is *not* notified when a shared worker
// opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverSharedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
#endif  // !defined(OS_ANDROID)

// Verify that content::LockObserver is *not* notified when a service worker
// opens/closes an IndexedDB connection.
IN_PROC_BROWSER_TEST_F(IndexedDBExecutionContextConnectionTrackerBrowserTest,
                       ObserverServiceWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
