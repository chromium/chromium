// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/lock_observer.h"
#include "content/public/common/content_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

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

constexpr int kTestRenderProcessId = 4;
constexpr int kTestRenderFrameId = 2;

class IndexedDBExecutionContextConnectionTrackerTest : public testing::Test {
 protected:
  IndexedDBExecutionContextConnectionTrackerTest() = default;

  void SetUp() {
    testing::Test::SetUp();
    original_client_ = SetBrowserClientForTesting(&test_browser_client_);
  }

  void TearDown() {
    SetBrowserClientForTesting(original_client_);
    testing::Test::TearDown();
  }

  testing::StrictMock<MockObserver> observer_;

 private:
  TestBrowserClient test_browser_client_{&observer_};
  ContentBrowserClient* original_client_ = nullptr;
};

}  // namespace

TEST_F(IndexedDBExecutionContextConnectionTrackerTest, SingleHandle) {
  IndexedDBExecutionContextConnectionTracker tracker(kTestRenderProcessId,
                                                     kTestRenderFrameId);

  {
    // Expect a notification that the execution context started holding
    // connections when the first handle is created.
    EXPECT_CALL(observer_, OnFrameStartsHoldingIndexedDBConnections(
                               kTestRenderProcessId, kTestRenderFrameId));
    IndexedDBExecutionContextConnectionTracker::Handle handle =
        tracker.CreateHandle();
    testing::Mock::VerifyAndClear(&observer_);

    // Expect a notification that the execution context stopped holding
    // connections when the handle goes out of scope.
    EXPECT_CALL(observer_, OnFrameStopsHoldingIndexedDBConnections(
                               kTestRenderProcessId, kTestRenderFrameId));
  }
  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(IndexedDBExecutionContextConnectionTrackerTest, TwoHandles) {
  IndexedDBExecutionContextConnectionTracker tracker(kTestRenderProcessId,
                                                     kTestRenderFrameId);

  {
    // Expect a notification that the execution context started holding
    // connections when the first handle is created.
    EXPECT_CALL(observer_, OnFrameStartsHoldingIndexedDBConnections(
                               kTestRenderProcessId, kTestRenderFrameId));
    IndexedDBExecutionContextConnectionTracker::Handle handle =
        tracker.CreateHandle();
    testing::Mock::VerifyAndClear(&observer_);

    {
      // Don't expect notifications when a second handle is created and
      // destroyed.
      IndexedDBExecutionContextConnectionTracker::Handle other_handle =
          tracker.CreateHandle();
    }

    // Expect a notification that the execution context stopped holding
    // connections when the handle goes out of scope.
    EXPECT_CALL(observer_, OnFrameStopsHoldingIndexedDBConnections(
                               kTestRenderProcessId, kTestRenderFrameId));
  }
  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(IndexedDBExecutionContextConnectionTrackerTest, MoveHandle) {
  IndexedDBExecutionContextConnectionTracker tracker(kTestRenderProcessId,
                                                     kTestRenderFrameId);
  std::unique_ptr<IndexedDBExecutionContextConnectionTracker::Handle>
      moved_handle;

  {
    // Expect a notification that the execution context started holding
    // connections when the first handle is created.
    EXPECT_CALL(observer_, OnFrameStartsHoldingIndexedDBConnections(
                               kTestRenderProcessId, kTestRenderFrameId));
    IndexedDBExecutionContextConnectionTracker::Handle handle =
        tracker.CreateHandle();
    testing::Mock::VerifyAndClear(&observer_);

    // Move |handle| to |moved_handle|.
    moved_handle =
        std::make_unique<IndexedDBExecutionContextConnectionTracker::Handle>(
            std::move(handle));

    // Don't expect a notification when |handle| goes out of scope, because it
    // was moved to |moved_handle|.
  }

  // Expect a notification that the execution context stopped holding
  // connections when |moved_handle| is destroyed.
  EXPECT_CALL(observer_, OnFrameStopsHoldingIndexedDBConnections(
                             kTestRenderProcessId, kTestRenderFrameId));
  moved_handle.reset();
  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(IndexedDBExecutionContextConnectionTrackerTest, WorkerHandle) {
  // Passing MSG_ROUTING_NONE as |render_frame_id| to indicate that this is a
  // worker execution context.
  IndexedDBExecutionContextConnectionTracker tracker(kTestRenderProcessId,
                                                     MSG_ROUTING_NONE);

  {
    // Don't expect observer notifications when a handle is created and goes out
    // of scope.
    IndexedDBExecutionContextConnectionTracker::Handle handle =
        tracker.CreateHandle();
  }
}

}  // namespace content
