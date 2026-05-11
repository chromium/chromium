// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/lazy_unexportable_private_key_factory.h"

#import <memory>

#import "base/test/gmock_callback_support.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/enterprise/client_certificates/core/mock_private_key_factory.h"
#import "components/enterprise/client_certificates/core/private_key.h"
#import "ios/chrome/browser/enterprise/client_certificates/cert_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::RunOnceCallback;
using testing::_;

namespace client_certificates {

namespace {

std::unique_ptr<PrivateKeyFactory> CreateMockFactory() {
  auto mock_factory = std::make_unique<MockPrivateKeyFactory>();

  ON_CALL(*mock_factory, CreatePrivateKey(_))
      .WillByDefault(RunOnceCallback<0>(nullptr));
  ON_CALL(*mock_factory, LoadPrivateKey(_, _))
      .WillByDefault(RunOnceCallback<1>(nullptr));
  ON_CALL(*mock_factory, LoadPrivateKeyFromDict(_, _))
      .WillByDefault(RunOnceCallback<1>(nullptr));

  return mock_factory;
}

std::optional<std::string> GetEmptyAccessGroup() {
  return std::nullopt;
}

}  // namespace

class LazyUnexportablePrivateKeyFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    LazyUnexportablePrivateKeyFactory::SetFactoryCreatorForTesting(
        &CreateMockFactory);
  }

  void TearDown() override {
    LazyUnexportablePrivateKeyFactory::SetFactoryCreatorForTesting(nullptr);
    SetAccessGroupHookForTesting(nullptr);
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that operations are queued if the background initialization of the
// underlying factory hasn't finished yet.
TEST_F(LazyUnexportablePrivateKeyFactoryTest,
       QueuesOperationsBeforeInitialization) {
  LazyUnexportablePrivateKeyFactory factory("test_profile");

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  base::test::TestFuture<scoped_refptr<PrivateKey>> load_dict_future;

  factory.CreatePrivateKey(create_future.GetCallback());

  client_certificates_pb::PrivateKey serialized_key;
  factory.LoadPrivateKey(serialized_key, load_future.GetCallback());

  base::DictValue dict_key;
  factory.LoadPrivateKeyFromDict(dict_key, load_dict_future.GetCallback());

  // At this point, the background task hasn't completed, so operations are
  // queued.
  EXPECT_FALSE(create_future.IsReady());
  EXPECT_FALSE(load_future.IsReady());
  EXPECT_FALSE(load_dict_future.IsReady());

  // Wait specifically for these futures to resolve. This spins a RunLoop
  // and quits it exactly when the callbacks are invoked by the background
  // initialization draining the queue.
  EXPECT_TRUE(create_future.Wait());
  EXPECT_TRUE(load_future.Wait());
  EXPECT_TRUE(load_dict_future.Wait());
}

// Tests that operations are executed directly if the background initialization
// has already completed.
TEST_F(LazyUnexportablePrivateKeyFactoryTest,
       ExecutesImmediatelyAfterInitialization) {
  LazyUnexportablePrivateKeyFactory factory("test_profile");

  // Wait for background initialization to complete by waiting for a dummy
  // operation to finish.
  base::test::TestFuture<scoped_refptr<PrivateKey>> init_future;
  client_certificates_pb::PrivateKey dummy_key;
  factory.LoadPrivateKey(dummy_key, init_future.GetCallback());
  EXPECT_TRUE(init_future.Wait());

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;

  // The mock factory is configured to resolve the callback synchronously.
  // Because the factory is already initialized, it passes the call through
  // directly without queuing, so the future becomes ready instantly without
  // needing to wait.
  factory.CreatePrivateKey(create_future.GetCallback());

  EXPECT_TRUE(create_future.IsReady());
}

// Tests that the factory is created correctly when real factory is used and
// GetAccessGroup returns std::nullopt.
TEST_F(LazyUnexportablePrivateKeyFactoryTest, HandlesEmptyAccessGroup) {
  SetAccessGroupHookForTesting(&GetEmptyAccessGroup);
  // Remove the factory creator override so that the actual CreateBaseFactory
  // logic runs in this test.
  LazyUnexportablePrivateKeyFactory::SetFactoryCreatorForTesting(nullptr);
  LazyUnexportablePrivateKeyFactory factory("test_profile");

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory.CreatePrivateKey(create_future.GetCallback());

  // It shouldn't crash, and since the access group is empty, no sub-factory
  // is added for kUnexportableKey, so it should return nullptr.
  EXPECT_EQ(create_future.Get(), nullptr);
}

}  // namespace client_certificates
