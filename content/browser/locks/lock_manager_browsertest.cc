// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/feature_observer.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
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

class TestBrowserClient : public ContentBrowserClient {
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
  FeatureObserverClient* feature_observer_client_;
};

class MockObserverClient : public FeatureObserverClient {
 public:
  MockObserverClient() = default;
  ~MockObserverClient() override = default;

  // PerformanceManagerFeatureObserver implementation:
  MOCK_METHOD2(OnStartUsing,
               void(GlobalFrameRoutingId id,
                    blink::mojom::ObservedFeatureType type));
  MOCK_METHOD2(OnStopUsing,
               void(GlobalFrameRoutingId id,
                    blink::mojom::ObservedFeatureType type));
};

void RunLoopWithTimeout() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // This is required to allow navigation to test https:// URLs. Web Locks are
    // not exposed to http:// URLs.
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
    EXPECT_TRUE(NavigateToURL(shell(), GetLocksURL("a.com")));
    return true;
  }

  GURL GetLocksURL(const std::string& hostname) const {
    return server_.GetURL(hostname, "/locks/locks.html");
  }

  testing::StrictMock<MockObserverClient> mock_observer_client_;

 private:
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  ContentBrowserClient* original_client_ = nullptr;
  TestBrowserClient test_browser_client_{&mock_observer_client_};
};

// Verify that content::FeatureObserver is notified when a frame acquires a
// single locks.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverSingleLock) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  GlobalFrameRoutingId routing_id(rfh->GetProcess()->GetID(),
                                  rfh->GetRoutingID());

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }

  {
    // Release a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
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

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  GlobalFrameRoutingId routing_id(rfh->GetProcess()->GetID(),
                                  rfh->GetRoutingID());

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
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
        OnStopUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
    EXPECT_TRUE(ExecJs(rfh, "ReleaseLock('lock_b');"));
    // Quit when OnStopUsing is invoked.
    run_loop.Run();
  }
}

// Verify that content::FeatureObserver is notified that a frame stopped holding
// locks when it is navigated away.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverNavigate) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  GlobalFrameRoutingId routing_id(rfh->GetProcess()->GetID(),
                                  rfh->GetRoutingID());

  {
    // Acquire a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }
  {
    // Navigate away. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
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

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();
  GlobalFrameRoutingId routing_id(rfh->GetProcess()->GetID(),
                                  rfh->GetRoutingID());

  {
    // Acquire a lock in first WebContents lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client_,
        OnStartUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
    EXPECT_TRUE(ExecJs(rfh, "AcquireLock('lock_a');"));
    // Quit when OnStartUsing is invoked.
    run_loop.Run();
  }

  // Open another WebContents and navigate.
  Shell* other_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());
  EXPECT_TRUE(NavigateToURL(other_shell, GetLocksURL("a.com")));
  RenderFrameHost* other_rfh = other_shell->web_contents()->GetMainFrame();
  GlobalFrameRoutingId other_routing_id(other_rfh->GetProcess()->GetID(),
                                        other_rfh->GetRoutingID());

  {
    // Steal the lock from other WebContents. Expect observer notifications.
    //
    base::RunLoop run_loop;

    // Wait for the thief and the victim to be notified, but in any order.
    int callback_count = 0;
    auto callback = [&](GlobalFrameRoutingId,
                        blink::mojom::ObservedFeatureType) {
      callback_count++;
      if (callback_count == 2)
        run_loop.Quit();
    };
    EXPECT_CALL(
        mock_observer_client_,
        OnStopUsing(routing_id, blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce(callback);
    EXPECT_CALL(mock_observer_client_,
                OnStartUsing(other_routing_id,
                             blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce(callback);

    EXPECT_TRUE(ExecJs(other_rfh, "StealLock('lock_a');"));
    // Quit after the lock has been released and then grabbed.
    run_loop.Run();
  }

  {
    // Release a lock. Expect observer notification.
    base::RunLoop run_loop;
    EXPECT_CALL(mock_observer_client_,
                OnStopUsing(other_routing_id,
                            blink::mojom::ObservedFeatureType::kWebLock))
        .WillOnce([&](GlobalFrameRoutingId, blink::mojom::ObservedFeatureType) {
          run_loop.Quit();
        });
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

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
#if !defined(OS_ANDROID)
// Verify that content::FeatureObserver is *not* notified when a lock is
// acquired by a shared worker.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverSharedWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
#endif  // !defined(OS_ANDROID)

// Verify that content::FeatureObserver is *not* notified when a lock is
// acquired by a service worker.
IN_PROC_BROWSER_TEST_F(LockManagerBrowserTest, ObserverServiceWorker) {
  if (!CheckShouldRunTestAndNavigate())
    return;

  RenderFrameHost* rfh = shell()->web_contents()->GetMainFrame();

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
