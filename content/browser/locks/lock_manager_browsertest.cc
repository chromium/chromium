// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/feature_observer.h"
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
  MOCK_METHOD2(OnStartUsing,
               void(GlobalRenderFrameHostId id,
                    blink::mojom::ObservedFeatureType type));
  MOCK_METHOD2(OnStopUsing,
               void(GlobalRenderFrameHostId id,
                    blink::mojom::ObservedFeatureType type));
};

void RunLoopWithTimeout() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

}  // namespace

class LockManagerBrowserTest : public ContentBrowserTest {
 public:
  LockManagerBrowserTest() = default;
  ~LockManagerBrowserTest() override = default;

  LockManagerBrowserTest(const LockManagerBrowserTest&) = delete;
  LockManagerBrowserTest& operator=(const LockManagerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    test_browser_client_ =
        std::make_unique<TestBrowserClient>(&mock_observer_client_);

    host_resolver()->AddRule("*", "127.0.0.1");
    server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(server_.Start());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    test_browser_client_.reset();
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

  bool CheckShouldRunTestAndNavigate() const {
    EXPECT_TRUE(NavigateToURL(shell(), GetLocksURL("a.com")));
    return true;
  }

  GURL GetLocksURL(const std::string& hostname) const {
    return server_.GetURL(hostname, "/locks/locks.html");
  }

  testing::StrictMock<MockObserverClient> mock_observer_client_;

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<TestBrowserClient> test_browser_client_;
};

// Verify that content::FeatureObserver is notified when a frame acquires a
// single locks.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverSingleLock) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }

  {
    // Release a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "ReleaseLock('lock_a');"));
    // Quit when OnStopUsing is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified when a frame acquires
// multiple locks (notifications only when the number of held locks switches
// between zero and non-zero).
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverTwoLocks) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }

  // Acquire a second lock. Don't expect a notification.
  EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_b');"));
  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();

  // Release a lock. Don't expect a notification.
  EXPECT_TRUE(ExecJs(rfh, "ReleaseLock('lock_a');"));
  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();

  {
    // Release a lock. Expect observer notification, because number of held
    // locks is now zero.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "ReleaseLock('lock_b');"));
    // Quit when OnStopUsing is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified that a frame stopped holding
// locks when it is navigated away.
// TODO(crbug.com/40815542): Flakes on all platforms.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, DISABLED_ObserverNavigate) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }
  {
    // Navigate away. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(shell(), GetLocksURL("b.com")));
    // Quit when OnStopUsing is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified when a frame steals a lock
// from another frame.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverStealLock) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  {
    // Acquire a lock in first WebContents lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }

  // Open another WebContents and navigate.
  Shell* other_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());
  EXPECT_TRUE(NavigateToURL(other_shell, GetLocksURL("a.com")));
  RenderFrameHost* other_rfh =
      other_shell->web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId other_rfh_id = other_rfh->GetGlobalId();

  {
    // Steal the lock from other WebContents. Expect observer notifications.
    //
    base::RunLoop run_loop;

    // Wait for the thief and the victim to be notified, but in any order.
    int callback_count = 0;
    auto callback = [&](GlobalRenderFrameHostId,
                        blink::mojom::ObservedFeatureType) {
      callback_count++;
      if (callback_count == 2)
        run_loop.Quit();
    };
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce(callback);
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(other_rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce(callback);

    EXPECT_TRUE(ExecJs(other_rfh, "StealLock('lock_a');"));
    // Quit after the lock has been released and then grabbed.
    run_loop.Run();
  }

  {
    // Release a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(other_rfh_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalRenderFrameHostId,
                      blink::mojom::ObservedFeatureType) { run_loop.Quit(); });
    EXPECT_TRUE(ExecJs(other_rfh, "ReleaseLock('lock_a');"));
    // Quit when OnStopUsing is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is *not* notified when a lock is
// acquired by a dedicated worker.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverDedicatedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await AcquireReleaseLockFromDedicatedWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}

// SharedWorkers are not enabled on Android. https://crbug.com/154571
#if !BUILDFLAG(IS_ANDROID)
// Verify that content::FeatureObserver is *not* notified when a lock is
// acquired by a shared worker.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverSharedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await AcquireReleaseLockFromSharedWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that content::FeatureObserver is *not* notified when a lock is
// acquired by a service worker.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverServiceWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Use EvalJs() instead of ExecJs() to ensure that this doesn't return before
  // the lock is acquired and released by the worker.
  EXPECT_TRUE(EvalJs(rfh, R"(
      (async () => {
        await AcquireReleaseLockFromServiceWorker();
        return true;
      }) ();
  )")
                  .ExtractBool());

  // Wait a short timeout to make sure that the observer is not notified.
  RunLoopWithTimeout();
}

}  // namespace content
