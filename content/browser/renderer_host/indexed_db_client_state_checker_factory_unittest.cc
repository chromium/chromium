// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class IndexedDBClientStateCheckerFactoryTest
    : public RenderViewHostTestHarness {
 public:
  IndexedDBClientStateCheckerFactoryTest() {
    // Set up the features so that we can test the behaviour of the
    // `DocumentIndexedDBClientStateChecker` with document in different states
    // such as prerendering or back/forward cache.
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2, features::kBackForwardCache},
        {blink::features::kPrerender2MemoryControls});
  }

  IndexedDBClientStateCheckerFactoryTest(
      const IndexedDBClientStateCheckerFactoryTest&) = delete;
  IndexedDBClientStateCheckerFactoryTest& operator=(
      const IndexedDBClientStateCheckerFactoryTest&) = delete;

  ~IndexedDBClientStateCheckerFactoryTest() override = default;

  // A helper function used for testing the `DisallowInactiveClient` method
  // with callback.
  void TestDisallowInactiveClient(
      storage::mojom::IndexedDBClientStateChecker* checker,
      storage::mojom::DisallowInactiveClientReason reason,
      bool expected_was_active) {
    mojo::Remote<storage::mojom::IndexedDBClientKeepActive> test_remote;
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    checker->DisallowInactiveClient(
        /*connection_id=*/0, reason, test_remote.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::OnceClosure closure, bool expected_was_active,
               bool was_active) {
              EXPECT_EQ(was_active, expected_was_active);
              std::move(closure).Run();
            },
            std::move(quit_closure), expected_was_active));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(IndexedDBClientStateCheckerFactoryTest,
       DisallowInactiveClient_DocumentBFCache) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  ASSERT_TRUE(rfh);
  storage::mojom::IndexedDBClientStateChecker* checker =
      IndexedDBClientStateCheckerFactory::
          GetOrCreateIndexedDBClientStateCheckerForTesting(rfh->GetGlobalId());

  // The document is initially in active state.
  EXPECT_EQ(rfh->GetLifecycleState(), RenderFrameHost::LifecycleState::kActive);
  // For the active document, the client state checker should claim that it's
  // active.
  TestDisallowInactiveClient(
      checker,
      storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent,
      /*expected_was_active=*/true);
  // There is no side effect to the active document.
  EXPECT_EQ(rfh->GetLifecycleState(),
            RenderFrameHostImpl::LifecycleState::kActive);

  // Set the lifecycle state to `kInBackForwardCache`.
  rfh->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  // Now the client state check should report that the document is not active.
  TestDisallowInactiveClient(
      checker,
      storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent,
      /*expected_was_active=*/false);
  // The page will be evicted from back/forward cache as the side effect.
  EXPECT_TRUE(rfh->is_evicted_from_back_forward_cache());
}

TEST_F(IndexedDBClientStateCheckerFactoryTest,
       DisallowInactiveClient_DocumentPrerendering) {
  // Set up a `RenderFrameHost` that's in `kPrerendering`.
  test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());
  GURL url = GURL("https://example.com");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url);
  RenderFrameHost* rfh = WebContentsTester::For(web_contents())
                             ->AddPrerenderAndCommitNavigation(GURL(url));
  EXPECT_EQ(rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kPrerendering);

  storage::mojom::IndexedDBClientStateChecker* checker =
      IndexedDBClientStateCheckerFactory::
          GetOrCreateIndexedDBClientStateCheckerForTesting(rfh->GetGlobalId());
  // For prerendering case, the client state checker should claim it as
  // active, since IndexedDB is supported in prerendering page.
  TestDisallowInactiveClient(
      checker,
      storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent,
      /*expected_was_active=*/true);
  EXPECT_EQ(rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kPrerendering);
}

namespace {

class MockFeatureObserverClient : public FeatureObserverClient {
 public:
  MockFeatureObserverClient() = default;
  ~MockFeatureObserverClient() override = default;

  MOCK_METHOD(void,
              OnStartUsing,
              (GlobalRenderFrameHostId, blink::mojom::ObservedFeatureType),
              (override));
  MOCK_METHOD(void,
              OnStopUsing,
              (GlobalRenderFrameHostId, blink::mojom::ObservedFeatureType),
              (override));
};

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient(FeatureObserverClient* feature_observer_client)
      : feature_observer_client_(feature_observer_client) {}
  ~TestBrowserClient() override = default;

  FeatureObserverClient* GetFeatureObserverClient() override {
    return feature_observer_client_;
  }

 private:
  raw_ptr<FeatureObserverClient> feature_observer_client_;
};

}  // namespace

class IndexedDBClientStateCheckerFactoryTestWithFeatureObserverClient
    : public IndexedDBClientStateCheckerFactoryTest {
 public:
  MockFeatureObserverClient& feature_observer_client() {
    return feature_observer_client_;
  }

 private:
  MockFeatureObserverClient feature_observer_client_;
  TestBrowserClient test_browser_client_{&feature_observer_client_};
  ScopedContentBrowserClientSetting scoped_content_browser_client_setting_{
      &test_browser_client_};
};

TEST_F(IndexedDBClientStateCheckerFactoryTestWithFeatureObserverClient,
       DisallowInactiveClient_Frozen_Twice) {
  // Freeze the page. Note that it has to be made visible first before hiding it
  // works.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->SetPageFrozen(true);

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  ASSERT_TRUE(rfh);
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  storage::mojom::IndexedDBClientStateChecker* checker =
      IndexedDBClientStateCheckerFactory::
          GetOrCreateIndexedDBClientStateCheckerForTesting(rfh->GetGlobalId());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        feature_observer_client(),
        OnStartUsing(
            rfh_id, blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
    EXPECT_CALL(
        feature_observer_client(),
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    TestDisallowInactiveClient(checker,
                               storage::mojom::DisallowInactiveClientReason::
                                   kTransactionIsStartingWhileBlockingOthers,
                               /*expected_was_active=*/true);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        feature_observer_client(),
        OnStartUsing(
            rfh_id, blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
    EXPECT_CALL(
        feature_observer_client(),
        OnStopUsing(rfh_id,
                    blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    TestDisallowInactiveClient(checker,
                               storage::mojom::DisallowInactiveClientReason::
                                   kTransactionIsStartingWhileBlockingOthers,
                               /*expected_was_active=*/true);
    run_loop.Run();
  }
}

}  // namespace content
