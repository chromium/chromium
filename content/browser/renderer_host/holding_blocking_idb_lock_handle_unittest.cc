// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/holding_blocking_idb_lock_handle.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/feature_observer_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom.h"

namespace content {

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
  explicit TestBrowserClient(FeatureObserverClient* feature_observer_client)
      : feature_observer_client_(feature_observer_client) {}
  ~TestBrowserClient() override = default;

  FeatureObserverClient* GetFeatureObserverClient() override {
    return feature_observer_client_;
  }

 private:
  raw_ptr<FeatureObserverClient> feature_observer_client_;
};

}  // namespace

class HoldingBlockingIDBLockHandleTest : public RenderViewHostImplTestHarness {
 public:
  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  MockFeatureObserverClient& feature_observer_client() {
    return feature_observer_client_;
  }

 private:
  MockFeatureObserverClient feature_observer_client_;
  TestBrowserClient test_browser_client_{&feature_observer_client_};
  ScopedContentBrowserClientSetting scoped_content_browser_client_setting_{
      &test_browser_client_};
};

TEST_F(HoldingBlockingIDBLockHandleTest, DefaultConstructor) {
  HoldingBlockingIDBLockHandle handle;
  EXPECT_FALSE(handle.IsValid());

  // Resetting an invalid handle should be a safe no-op.
  handle.Reset();
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(HoldingBlockingIDBLockHandleTest, ConstructorAndDestructor) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  EXPECT_CALL(
      feature_observer_client(),
      OnStartUsing(rfh_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
  EXPECT_CALL(
      feature_observer_client(),
      OnStopUsing(rfh_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  {
    HoldingBlockingIDBLockHandle handle =
        rfh->RegisterHoldingBlockingIDBLockHandle();
    EXPECT_TRUE(handle.IsValid());
  }
}

TEST_F(HoldingBlockingIDBLockHandleTest, Reset) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  EXPECT_CALL(
      feature_observer_client(),
      OnStartUsing(rfh_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
  EXPECT_CALL(
      feature_observer_client(),
      OnStopUsing(rfh_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  HoldingBlockingIDBLockHandle handle =
      rfh->RegisterHoldingBlockingIDBLockHandle();
  EXPECT_TRUE(handle.IsValid());

  handle.Reset();
  EXPECT_FALSE(handle.IsValid());

  // Further calls to `Reset()` should be a no-op.
  handle.Reset();
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(HoldingBlockingIDBLockHandleTest, MoveConstructor) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  EXPECT_CALL(
      feature_observer_client(),
      OnStartUsing(rfh_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
  EXPECT_CALL(
      feature_observer_client(),
      OnStopUsing(rfh_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  HoldingBlockingIDBLockHandle handle1 =
      rfh->RegisterHoldingBlockingIDBLockHandle();
  EXPECT_TRUE(handle1.IsValid());

  HoldingBlockingIDBLockHandle handle2(std::move(handle1));
  EXPECT_FALSE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());

  // Resetting the moved-from `handle1` should not affect `handle2` or `rfh`.
  handle1.Reset();
  EXPECT_TRUE(handle2.IsValid());

  handle2.Reset();
  EXPECT_FALSE(handle2.IsValid());
}

TEST_F(HoldingBlockingIDBLockHandleTest, MoveAssignmentToEmptyHandle) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  EXPECT_CALL(
      feature_observer_client(),
      OnStartUsing(rfh_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));
  EXPECT_CALL(
      feature_observer_client(),
      OnStopUsing(rfh_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  HoldingBlockingIDBLockHandle handle1 =
      rfh->RegisterHoldingBlockingIDBLockHandle();
  HoldingBlockingIDBLockHandle handle2;
  EXPECT_FALSE(handle2.IsValid());

  handle2 = std::move(handle1);
  EXPECT_FALSE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());

  handle2.Reset();
  EXPECT_FALSE(handle2.IsValid());
}

TEST_F(HoldingBlockingIDBLockHandleTest, MoveAssignmentToActiveHandle) {
  RenderFrameHostImpl* rfh = main_rfh_impl();
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  // `OnStartUsing()` is only called once when transitioning from 0 to 1 lock.
  EXPECT_CALL(
      feature_observer_client(),
      OnStartUsing(rfh_id,
                   blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock));

  HoldingBlockingIDBLockHandle handle1 =
      rfh->RegisterHoldingBlockingIDBLockHandle();
  HoldingBlockingIDBLockHandle handle2 =
      rfh->RegisterHoldingBlockingIDBLockHandle();

  EXPECT_TRUE(handle1.IsValid());
  EXPECT_TRUE(handle2.IsValid());

  // Move-assign `handle2` into `handle1` while `handle1` already holds an
  // active handle.
  handle1 = std::move(handle2);
  EXPECT_FALSE(handle2.IsValid());
  EXPECT_TRUE(handle1.IsValid());

  // TODO(thestig): Defaulting the move-assignment operator causes a resource
  // leak if `this` already holds an active handle. Because `= default` does not
  // call `Reset()`, any previously held lock registration on
  // `render_frame_host_` is overwritten without calling
  // `OnStopHoldingBlockingIDBLock()`, permanently leaking the lock count on
  // the frame and preventing it from being notified as stopped.
  //
  // `operator=` should call `Reset()` before taking ownership of `other`'s
  // members (after checking `this != &other`).
  //
  // Currently, the lock was leaked on `rfh`. Even after `handle1.Reset()` is
  // called, `OnStopUsing()` is not called because the count never returns to 0.
  // Once the bug is fixed, `OnStopUsing()` should be called once here.
  EXPECT_CALL(
      feature_observer_client(),
      OnStopUsing(rfh_id,
                  blink::mojom::ObservedFeatureType::kBlockingIndexedDBLock))
      .Times(0);

  handle1.Reset();
  EXPECT_FALSE(handle1.IsValid());
}

}  // namespace content
