// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/feature_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace content {

namespace {

// IndexedDB detects that a client is inactive when it is frozen, or if it
// enters BFCache. Since this test suite is testing the freezing part, disable
// BFCache to ensure this test doesn't depend on that functionality to work.
void DisableBackForwardCache(Shell* tab) {
  DisableBackForwardCacheForTesting(tab->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
}

void HideAndFreezeTab(Shell* tab) {
#if defined(USE_AURA)
  // On platforms that use Aura, simply calling WebContentsImpl::WasHidden() is
  // not sufficient, because a navigation will force a recalculation of the
  // occlusion state of all windows. This would switch back the visibility state
  // of the WebContents to "shown" because its window is still visible.
  tab->window()->Hide();
#else
  tab->web_contents()->WasHidden();
#endif

  tab->web_contents()->SetPageFrozen(true);
}

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

class TestBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  explicit TestBrowserClient(FeatureObserverClient* feature_observer_client)
      : feature_observer_client_(feature_observer_client) {}
  ~TestBrowserClient() override = default;

  TestBrowserClient(const TestBrowserClient&) = delete;
  TestBrowserClient& operator=(const TestBrowserClient&) = delete;

  // ContentBrowserClient:
  FeatureObserverClient* GetFeatureObserverClient() override {
    return feature_observer_client_;
  }

 private:
  raw_ptr<FeatureObserverClient> feature_observer_client_;
};

class MockObserverClient : public FeatureObserverClient {
 public:
  MockObserverClient() = default;
  ~MockObserverClient() override = default;

  // FeatureObserverClient implementation:
  MOCK_METHOD2(OnStartUsing,
               void(GlobalRenderFrameHostId id,
                    blink::mojom::ObservedFeatureType type));
  MOCK_METHOD2(OnStopUsing,
               void(GlobalRenderFrameHostId id,
                    blink::mojom::ObservedFeatureType type));
};

class IndexedDBTransactionFeatureObserverBrowserTest
    : public ContentBrowserTest {
 public:
  IndexedDBTransactionFeatureObserverBrowserTest() = default;
  ~IndexedDBTransactionFeatureObserverBrowserTest() override = default;

  IndexedDBTransactionFeatureObserverBrowserTest(
      const IndexedDBTransactionFeatureObserverBrowserTest&) = delete;
  IndexedDBTransactionFeatureObserverBrowserTest& operator=(
      const IndexedDBTransactionFeatureObserverBrowserTest&) = delete;

  RenderFrameHost* GetPrimaryMainFrame(Shell* shell) {
    return shell->web_contents()->GetPrimaryMainFrame();
  }

  MockObserverClient& mock_observer_client() { return mock_observer_client_; }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    browser_client_ =
        std::make_unique<TestBrowserClient>(&mock_observer_client_);

    host_resolver()->AddRule("a.com", "127.0.0.1");
    server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(server_.Start());
  }

  testing::StrictMock<MockObserverClient> mock_observer_client_;
  std::unique_ptr<TestBrowserClient> browser_client_;
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace

// Validates that we don't get kBlockingIndexedDBLock
// notification when the indexedDB transaction is not blocking.
IN_PROC_BROWSER_TEST_F(
    IndexedDBTransactionFeatureObserverBrowserTest,
    ObserveNoIndexedDBTransactionWhenTransactionIsNotBlocking) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_CALL(
      mock_observer_client(),
      OnStartUsing(testing::_,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
      .Times(0);
  EXPECT_CALL(
      mock_observer_client(),
      OnStopUsing(testing::_,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
      .Times(0);

  Shell* tab_holding_locks = shell();
  Shell* tab_acquiring_locks = CreateBrowser();

  DisableBackForwardCache(tab_holding_locks);
  DisableBackForwardCache(tab_acquiring_locks);

  // Navigate the tab holding locks to A and use IndexedDB.
  ASSERT_TRUE(NavigateToURL(
      tab_holding_locks,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));

  ASSERT_TRUE(ExecJs(tab_holding_locks, "setupIndexedDBConnection()"));

  // Navigate the tab waiting for locks to A as well and make it request
  // for the same lock.
  ASSERT_TRUE(NavigateToURL(
      tab_acquiring_locks,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));

  // There is no kBlockingIndexedDBLock notified to the observer
  ASSERT_TRUE(ExecJs(tab_acquiring_locks, "setupIndexedDBConnection()"));
  ASSERT_TRUE(ExecJs(tab_acquiring_locks, "startIndexedDBTransaction()"));

  // Close the tab holding locks
  tab_acquiring_locks->Close();
}

IN_PROC_BROWSER_TEST_F(IndexedDBTransactionFeatureObserverBrowserTest,
                       ObserveIndexedDBTransactionOnBlockingOthers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Shell* tab_holding_locks = shell();
  Shell* tab_waiting_for_locks = CreateBrowser();

  DisableBackForwardCache(tab_holding_locks);
  DisableBackForwardCache(tab_waiting_for_locks);

  // Navigate the tab holding locks to A and use IndexedDB.
  ASSERT_TRUE(NavigateToURL(
      tab_holding_locks,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));
  GlobalRenderFrameHostId document_holding_locks_id =
      GetPrimaryMainFrame(tab_holding_locks)->GetGlobalId();

  ASSERT_TRUE(ExecJs(tab_holding_locks, "setupIndexedDBConnection()"));

  // Make sure the page keeps holding the lock by running infinite tasks on the
  // object store.
  ASSERT_TRUE(
      ExecJs(tab_holding_locks, "runInfiniteIndexedDBTransactionLoop()"));

  // Freeze the page running the infinite transaction loop.
  HideAndFreezeTab(tab_holding_locks);

  // Navigate the tab waiting for locks to A as well and make it request
  // for the same lock. Since the other tab is holding the lock, this
  // tab will be blocked and waiting for the lock to be released.
  ASSERT_TRUE(NavigateToURL(
      tab_waiting_for_locks,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));

  ASSERT_TRUE(ExecJs(tab_waiting_for_locks, "setupIndexedDBConnection()"));

  {
    // Initiating the transaction on the waiting locks, which will raise the
    // OnStartUsing(kBlockingIndexedDBLock) event.
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client(),
        OnStartUsing(document_holding_locks_id,
                     blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
        .WillOnce(InvokeClosure(run_loop.QuitClosure()));
    ASSERT_TRUE(ExecJs(tab_waiting_for_locks, "startIndexedDBTransaction()"));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_observer_client(),
        OnStopUsing(document_holding_locks_id,
                    blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
        .WillOnce(InvokeClosure(run_loop.QuitClosure()));

    // Close the tab holding the lock.
    tab_holding_locks->Close();

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(IndexedDBTransactionFeatureObserverBrowserTest,
                       ObserveIndexedDBTransactionOnVersionChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Shell* tab_receiving_version_change = shell();
  Shell* tab_sending_version_change = CreateBrowser();

  DisableBackForwardCache(tab_receiving_version_change);
  DisableBackForwardCache(tab_sending_version_change);

  // Navigate the tab receiving version change to A and use IndexedDB.
  ASSERT_TRUE(NavigateToURL(
      tab_receiving_version_change,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));

  GlobalRenderFrameHostId document_receiving_version_change_id =
      GetPrimaryMainFrame(tab_receiving_version_change)->GetGlobalId();

  // Create two connection with the same version here
  ASSERT_TRUE(
      ExecJs(tab_receiving_version_change, "setupIndexedDBConnection()"));
  ASSERT_TRUE(ExecJs(tab_receiving_version_change,
                     "setupNewIndexedDBConnectionWithSameVersion()"));

  // Navigate the tab sending version change to the same page, and create a new
  // IndexedDB connection with a higher version.
  ASSERT_TRUE(NavigateToURL(
      tab_sending_version_change,
      embedded_test_server()->GetURL(
          "a.com", "/back_forward_cache/page_with_indexedDB.html")));

  // Running `setupNewIndexedDBConnectionWithHigherVersion()` will trigger the
  // `versionchange` event. This will raise kBlockingIndexedDBLock
  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_observer_client(),
      OnStartUsing(document_receiving_version_change_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
      .WillOnce(InvokeClosure(run_loop.QuitClosure()));
  ExecuteScriptAsync(tab_sending_version_change,
                     "setupNewIndexedDBConnectionWithHigherVersion()");
  run_loop.Run();

  // The RenderFrameHost release will notify the observer
  EXPECT_CALL(
      mock_observer_client(),
      OnStopUsing(document_receiving_version_change_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  tab_receiving_version_change->Close();
}

}  // namespace content
