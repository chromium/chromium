// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/sync_point_manager.h"

#include <stdint.h>

#include <memory>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/test/with_feature_override.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SyncPointManagerTest : public base::test::WithFeatureOverride,
                             public testing::Test {
 public:
  SyncPointManagerTest()
      : base::test::WithFeatureOverride(features::kSyncPointGraphValidation) {}
  ~SyncPointManagerTest() override = default;

 protected:
  void SetUp() override {
    sync_point_manager_ = std::make_unique<SyncPointManager>();

    CHECK_EQ(GetParam(), sync_point_manager_->graph_validation_enabled());
  }

  // Simple static function which can be used to test callbacks.
  static void SetIntegerFunction(int* test, int value) { *test = value; }

  std::unique_ptr<SyncPointManager> sync_point_manager_;
};

// Tests SyncPointManager behavior when using SyncPointOrderData for validation.
class SyncPointManagerOrderValidationTest : public SyncPointManagerTest {
 public:
  SyncPointManagerOrderValidationTest() = default;
  ~SyncPointManagerOrderValidationTest() override = default;

 protected:
  void SetUp() override {
    SyncPointManagerTest::SetUp();
    CHECK(!sync_point_manager_->graph_validation_enabled());
  }
};

struct SyncPointStream {
  scoped_refptr<SyncPointOrderData> order_data;
  scoped_refptr<SyncPointClientState> client_state;
  base::queue<uint32_t> order_numbers;

  SyncPointStream(SyncPointManager* sync_point_manager,
                  CommandBufferNamespace namespace_id,
                  CommandBufferId command_buffer_id)
      : order_data(sync_point_manager->CreateSyncPointOrderData()),
        client_state(sync_point_manager->CreateSyncPointClientState(
            namespace_id,
            command_buffer_id,
            order_data->sequence_id())) {}

  ~SyncPointStream() {
    if (order_data)
      order_data->Destroy();
    if (client_state)
      client_state->Destroy();
  }

  void AllocateOrderNum() {
    order_numbers.push(order_data->GenerateUnprocessedOrderNumber());
  }

  void BeginProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->BeginProcessingOrderNumber(order_numbers.front());
  }

  void EndProcessing() {
    ASSERT_FALSE(order_numbers.empty());
    order_data->FinishProcessingOrderNumber(order_numbers.front());
    order_numbers.pop();
  }
};

TEST_P(SyncPointManagerTest, BasicSyncPointOrderDataTest) {
  scoped_refptr<SyncPointOrderData> order_data =
      sync_point_manager_->CreateSyncPointOrderData();

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(0u, order_data->unprocessed_order_num());

  uint32_t order_num = order_data->GenerateUnprocessedOrderNumber();
  EXPECT_EQ(1u, order_num);

  EXPECT_EQ(0u, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(0u, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->PauseProcessingOrderNumber(order_num);
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());

  order_data->BeginProcessingOrderNumber(order_num);
  EXPECT_TRUE(order_data->IsProcessingOrderNumber());

  order_data->FinishProcessingOrderNumber(order_num);
  EXPECT_EQ(order_num, order_data->current_order_num());
  EXPECT_EQ(order_num, order_data->processed_order_num());
  EXPECT_EQ(order_num, order_data->unprocessed_order_num());
  EXPECT_FALSE(order_data->IsProcessingOrderNumber());

  order_data->Destroy();
}

TEST_P(SyncPointManagerTest, BasicFenceSyncRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kBufferId = CommandBufferId::FromUnsafeValue(0x123);

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kBufferId, release_count);

  // Can't wait for sync token before client is registered.
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  SyncPointStream stream(sync_point_manager_.get(), kNamespaceId, kBufferId);

  stream.AllocateOrderNum();

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  stream.order_data->BeginProcessingOrderNumber(1);
  stream.client_state->ReleaseFenceSync(release_count);
  stream.order_data->FinishProcessingOrderNumber(1);

  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerTest, OutOfOrderSyncTokenRelease) {
  sync_point_manager_->set_suppress_fatal_log_for_testing();

  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kBufferId = CommandBufferId::FromUnsafeValue(0x123);

  uint64_t release_count_1 = 2;
  SyncToken sync_token_1(kNamespaceId, kBufferId, release_count_1);
  uint64_t release_count_2 = 1;
  SyncToken sync_token_2(kNamespaceId, kBufferId, release_count_2);

  SyncPointStream stream(sync_point_manager_.get(), kNamespaceId, kBufferId);
  stream.AllocateOrderNum();
  stream.AllocateOrderNum();

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));

  // Releasing the first sync token also releases the second because the first
  // token's release count is larger.
  stream.order_data->BeginProcessingOrderNumber(1);
  stream.client_state->ReleaseFenceSync(release_count_1);
  stream.order_data->FinishProcessingOrderNumber(1);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));

  // Releasing the second token should be a no-op.
  stream.order_data->BeginProcessingOrderNumber(2);
  stream.client_state->ReleaseFenceSync(release_count_2);
  stream.order_data->FinishProcessingOrderNumber(2);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));
}

TEST_P(SyncPointManagerTest, MultipleClientsPerOrderData) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kCmdBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream stream1(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId1);
  SyncPointStream stream2(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId2);

  uint64_t release_count = 1;
  SyncToken sync_token1(kNamespaceId, kCmdBufferId1, release_count);
  stream1.AllocateOrderNum();

  SyncToken sync_token2(kNamespaceId, kCmdBufferId2, release_count);
  stream2.AllocateOrderNum();

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token1));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token2));

  stream1.order_data->BeginProcessingOrderNumber(1);
  stream1.client_state->ReleaseFenceSync(release_count);
  stream1.order_data->FinishProcessingOrderNumber(1);

  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token1));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token2));
}

TEST_P(SyncPointManagerTest, BasicFenceSyncWaitRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  release_stream.BeginProcessing();
  release_stream.client_state->ReleaseFenceSync(release_count);
  EXPECT_EQ(123, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerTest, WaitWithOutOfOrderSyncTokenRelease) {
  sync_point_manager_->set_suppress_fatal_log_for_testing();

  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  int test_num_1 = 10;
  int test_num_2 = 10;
  int test_num_3 = 10;
  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum();
  uint64_t release_count_1 = 2;
  SyncToken sync_token_1(kNamespaceId, kReleaseCmdBufferId, release_count_1);
  release_stream.AllocateOrderNum();
  uint64_t release_count_2 = 1;
  SyncToken sync_token_2(kNamespaceId, kReleaseCmdBufferId, release_count_2);
  release_stream.AllocateOrderNum();
  uint64_t release_count_3 = 3;
  SyncToken sync_token_3(kNamespaceId, kReleaseCmdBufferId, release_count_3);

  wait_stream.AllocateOrderNum();
  wait_stream.BeginProcessing();
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token_1, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                   &test_num_1, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num_1);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  wait_stream.EndProcessing();

  wait_stream.AllocateOrderNum();
  wait_stream.BeginProcessing();
  valid_wait = wait_stream.client_state->Wait(
      sync_token_2, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                   &test_num_2, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num_2);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));
  wait_stream.EndProcessing();

  wait_stream.AllocateOrderNum();
  wait_stream.BeginProcessing();
  valid_wait = wait_stream.client_state->Wait(
      sync_token_3, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                   &test_num_3, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num_3);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_3));
  wait_stream.EndProcessing();

  // Releasing the first sync token should release the second one. Then,
  // releasing the second one should be a no-op.
  release_stream.BeginProcessing();
  release_stream.client_state->ReleaseFenceSync(release_count_1);
  EXPECT_EQ(123, test_num_1);
  EXPECT_EQ(123, test_num_2);
  EXPECT_EQ(10, test_num_3);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_3));
  release_stream.EndProcessing();

  release_stream.BeginProcessing();
  release_stream.client_state->ReleaseFenceSync(release_count_2);
  EXPECT_EQ(123, test_num_1);
  EXPECT_EQ(123, test_num_2);
  EXPECT_EQ(10, test_num_3);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_1));
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token_2));
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token_3));
  release_stream.EndProcessing();
}

TEST_P(SyncPointManagerTest, WaitOnSelfFails) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kWaitCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerOrderValidationTest, ReleaseAfterWaitOrderNumber) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum();
  release_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerTest, HigherOrderNumberRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Generate wait order number first.
  wait_stream.AllocateOrderNum();
  release_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  // Order number was higher but it was actually released.
  release_stream.BeginProcessing();
  release_stream.client_state->ReleaseFenceSync(release_count);
  release_stream.EndProcessing();

  // Release stream has already released so there's no need to wait.
  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerTest, DestroyedClientRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();

  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Destroying the client should release the wait.
  release_stream.client_state->Destroy();
  release_stream.client_state = nullptr;

  EXPECT_EQ(123, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerOrderValidationTest, NonExistentRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign release stream order [1] and wait stream order [2].
  // This test simply tests that a wait stream of order [2] waiting on
  // release stream of order [1] will still release the fence sync even
  // though nothing was released.
  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  wait_stream.BeginProcessing();
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // No release but finishing the order number should automatically release.
  release_stream.BeginProcessing();
  EXPECT_EQ(10, test_num);
  release_stream.EndProcessing();
  EXPECT_EQ(123, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerOrderValidationTest, NonExistentRelease2) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign Release stream order [1] and assign Wait stream orders [2, 3].
  // This test is similar to the NonExistentRelease case except
  // we place an extra order number in between the release and wait.
  // The wait stream [3] is waiting on release stream [1] even though
  // order [2] was also generated. Although order [2] only exists on the
  // wait stream so the release stream should only know about order [1].
  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
  // Have wait with order [3] to wait on release.
  wait_stream.BeginProcessing();
  EXPECT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  EXPECT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // Even though release stream order [1] did not have a release, it
  // should have changed test_num although the fence sync is still not released.
  release_stream.BeginProcessing();
  EXPECT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_EQ(123, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  test_num = 1;
  release_stream.AllocateOrderNum();
  release_stream.BeginProcessing();
  release_stream.client_state->ReleaseFenceSync(release_count);
  release_stream.EndProcessing();
  EXPECT_EQ(1, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerOrderValidationTest, NonExistentOrderNumRelease) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kReleaseCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kWaitCmdBufferId = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream release_stream(sync_point_manager_.get(), kNamespaceId,
                                 kReleaseCmdBufferId);
  SyncPointStream wait_stream(sync_point_manager_.get(), kNamespaceId,
                              kWaitCmdBufferId);

  // Assign Release stream orders [1, 4] and assign Wait stream orders [2, 3].
  // Here we are testing that wait order [3] will wait on a fence sync
  // in either order [1] or order [2]. Order [2] was not actually assigned
  // to the release stream so it is essentially non-existent to the release
  // stream's point of view. Once the release stream begins processing the next
  // order [3], it should realize order [2] didn't exist and release the fence.
  release_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();
  wait_stream.AllocateOrderNum();
  release_stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kReleaseCmdBufferId, release_count);

  // Have wait with order [3] to wait on release order [1] or [2].
  wait_stream.BeginProcessing();
  EXPECT_EQ(2u, wait_stream.order_data->current_order_num());
  wait_stream.EndProcessing();
  wait_stream.BeginProcessing();
  EXPECT_EQ(3u, wait_stream.order_data->current_order_num());
  int test_num = 10;
  bool valid_wait = wait_stream.client_state->Wait(
      sync_token, base::BindOnce(&SyncPointManagerTest::SetIntegerFunction,
                                 &test_num, 123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  // Release stream should know it should release fence sync by order [3], but
  // it has no unprocessed order numbers less than 3, so it runs the callback.
  release_stream.BeginProcessing();
  EXPECT_EQ(1u, release_stream.order_data->current_order_num());
  release_stream.EndProcessing();
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
  EXPECT_EQ(123, test_num);

  // Ensure that the wait callback does not get triggered again when it is
  // actually released.
  release_stream.BeginProcessing();
  test_num = 10;
  release_stream.client_state->ReleaseFenceSync(1);
  EXPECT_EQ(10, test_num);
  EXPECT_TRUE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerTest, WaitOnSameSequenceFails) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId = CommandBufferId::FromUnsafeValue(0x123);

  SyncPointStream stream(sync_point_manager_.get(), kNamespaceId, kCmdBufferId);

  // Dummy order number to avoid the wait_order_num <= processed_order_num + 1
  // check in SyncPointOrderData::ValidateReleaseOrderNum.
  sync_point_manager_->GenerateOrderNumber();

  // Order number for the wait.
  stream.AllocateOrderNum();

  uint64_t release_count = 1;
  SyncToken sync_token(kNamespaceId, kCmdBufferId, release_count);

  int test_num = 10;
  bool valid_wait = sync_point_manager_->Wait(
      sync_token, stream.order_data->sequence_id(),
      stream.order_data->unprocessed_order_num(),
      base::BindOnce(&SyncPointManagerTest::SetIntegerFunction, &test_num,
                     123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
  EXPECT_FALSE(sync_point_manager_->IsSyncTokenReleased(sync_token));
}

TEST_P(SyncPointManagerOrderValidationTest, HandleInvalidWaitOrderNumber) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kCmdBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream stream1(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId1);
  SyncPointStream stream2(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId2);

  stream1.AllocateOrderNum();  // stream 1, order num 1
  stream2.AllocateOrderNum();  // stream 2, order num 2
  stream2.AllocateOrderNum();  // stream 2, order num 3
  stream1.AllocateOrderNum();  // stream 1, order num 4

  // Run stream 1, order num 1.
  stream1.BeginProcessing();
  stream1.EndProcessing();

  // Stream 2 waits on stream 1 with order num 3. This wait is invalid because
  // there's no unprocessed order num less than 3 on stream 1.
  int test_num = 10;
  bool valid_wait = sync_point_manager_->Wait(
      SyncToken(kNamespaceId, kCmdBufferId1, 1),
      stream2.order_data->sequence_id(), 3,
      base::BindOnce(&SyncPointManagerTest::SetIntegerFunction, &test_num,
                     123));
  EXPECT_FALSE(valid_wait);
  EXPECT_EQ(10, test_num);
}

TEST_P(SyncPointManagerOrderValidationTest,
       RetireInvalidWaitAfterOrderNumberPasses) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kCmdBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream stream1(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId1);
  SyncPointStream stream2(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId2);

  stream1.AllocateOrderNum();  // stream 1, order num 1
  stream1.AllocateOrderNum();  // stream 1, order num 2
  stream2.AllocateOrderNum();  // stream 2, order num 3

  // Stream 2 waits on stream 1 with order num 3.
  int test_num = 10;
  bool valid_wait = sync_point_manager_->Wait(
      SyncToken(kNamespaceId, kCmdBufferId1, 1),
      stream2.order_data->sequence_id(), 3,
      base::BindOnce(&SyncPointManagerTest::SetIntegerFunction, &test_num,
                     123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num);

  stream1.AllocateOrderNum();  // stream 1, order num 4

  // Run stream 1, order num 1. The wait isn't retired.
  stream1.BeginProcessing();
  stream1.EndProcessing();
  EXPECT_EQ(10, test_num);

  // Run stream 1, order num 2. Wait is retired because we reach the last order
  // number that was unprocessed at the time the wait was enqueued.
  stream1.BeginProcessing();
  stream1.EndProcessing();
  EXPECT_EQ(123, test_num);
}

TEST_P(SyncPointManagerOrderValidationTest, HandleInvalidCyclicWaits) {
  CommandBufferNamespace kNamespaceId = gpu::CommandBufferNamespace::GPU_IO;
  CommandBufferId kCmdBufferId1 = CommandBufferId::FromUnsafeValue(0x123);
  CommandBufferId kCmdBufferId2 = CommandBufferId::FromUnsafeValue(0x234);

  SyncPointStream stream1(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId1);
  SyncPointStream stream2(sync_point_manager_.get(), kNamespaceId,
                          kCmdBufferId2);

  stream1.AllocateOrderNum();  // stream 1, order num 1
  stream2.AllocateOrderNum();  // stream 2, order num 2
  stream1.AllocateOrderNum();  // stream 1, order num 3
  stream2.AllocateOrderNum();  // stream 2, order num 4

  // Stream 2 waits on stream 1 with order num 2.
  int test_num1 = 10;
  bool valid_wait = sync_point_manager_->Wait(
      SyncToken(kNamespaceId, kCmdBufferId1, 1),
      stream2.order_data->sequence_id(), 2,
      base::BindOnce(&SyncPointManagerTest::SetIntegerFunction, &test_num1,
                     123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num1);

  // Stream 1 waits on stream 2 with order num 3.
  int test_num2 = 10;
  valid_wait = sync_point_manager_->Wait(
      SyncToken(kNamespaceId, kCmdBufferId2, 1),
      stream1.order_data->sequence_id(), 3,
      base::BindOnce(&SyncPointManagerTest::SetIntegerFunction, &test_num2,
                     123));
  EXPECT_TRUE(valid_wait);
  EXPECT_EQ(10, test_num2);

  // Run stream 1, order num 1.
  stream1.BeginProcessing();
  stream1.EndProcessing();

  // Since there's no unprocessed order num less than 2 on stream 1, the wait is
  // released.
  EXPECT_EQ(123, test_num1);
  EXPECT_EQ(10, test_num2);

  // Run stream 2, order num 2.
  stream2.BeginProcessing();
  stream2.EndProcessing();

  // Since there's no unprocessed order num less than 3 on stream 2, the wait is
  // released.
  EXPECT_EQ(123, test_num1);
  EXPECT_EQ(123, test_num2);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncPointManagerTest,
                         testing::Values(false, true));

// Only test the case of IsSyncPointGraphValidationEnabled() being false.
INSTANTIATE_TEST_SUITE_P(All,
                         SyncPointManagerOrderValidationTest,
                         testing::Values(false));

}  // namespace gpu
