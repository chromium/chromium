// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_context.h"
#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using testing::_;
using testing::StrictMock;

class MockSmartCardEmulationManager : public SmartCardEmulationManager {
 public:
  MOCK_METHOD(void,
              OnCreateContext,
              (device::mojom::SmartCardContextFactory::CreateContextCallback),
              (override));

  MOCK_METHOD(void,
              OnListReaders,
              (uint32_t, device::mojom::SmartCardContext::ListReadersCallback),
              (override));

  base::WeakPtr<MockSmartCardEmulationManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSmartCardEmulationManager> weak_ptr_factory_{this};
};

class EmulatedSmartCardContextFactoryTest : public testing::Test {
 public:
  EmulatedSmartCardContextFactoryTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        mock_manager_(
            std::make_unique<StrictMock<MockSmartCardEmulationManager>>()),
        factory_(
            std::make_unique<EmulatedSmartCardContextFactory>(*mock_manager_)) {
  }

  MockSmartCardEmulationManager* manager() { return mock_manager_.get(); }
  EmulatedSmartCardContextFactory* factory() { return factory_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockSmartCardEmulationManager> mock_manager_;
  std::unique_ptr<EmulatedSmartCardContextFactory> factory_;
};

TEST_F(EmulatedSmartCardContextFactoryTest, CreateContext_Success) {
  EXPECT_CALL(*manager(), OnCreateContext(_)).WillOnce([](auto callback) {
    mojo::PendingRemote<device::mojom::SmartCardContext> remote;
    std::ignore = remote.InitWithNewPipeAndPassReceiver();
    std::move(callback).Run(
        device::mojom::SmartCardCreateContextResult::NewContext(
            std::move(remote)));
  });

  base::test::TestFuture<device::mojom::SmartCardCreateContextResultPtr> future;
  factory()->CreateContext(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_context());
  EXPECT_TRUE(result->get_context().is_valid());
}

TEST_F(EmulatedSmartCardContextFactoryTest, CreateContext_Failure) {
  EXPECT_CALL(*manager(), OnCreateContext(_)).WillOnce([](auto callback) {
    std::move(callback).Run(
        device::mojom::SmartCardCreateContextResult::NewError(
            device::mojom::SmartCardError::kNoService));
  });

  base::test::TestFuture<device::mojom::SmartCardCreateContextResultPtr> future;
  factory()->CreateContext(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

class EmulatedSmartCardContextTest
    : public EmulatedSmartCardContextFactoryTest {
 public:
  void SetUp() override {
    context_ = std::make_unique<EmulatedSmartCardContext>(
        manager()->GetWeakPtr(), 123);
  }

 protected:
  std::unique_ptr<EmulatedSmartCardContext> context_;
};

TEST_F(EmulatedSmartCardContextTest, ListReaders_Success) {
  const uint32_t kContextId = 123;
  const std::vector<std::string> kExpectedReaders = {"Reader A", "Reader B"};

  EXPECT_CALL(*manager(), OnListReaders(kContextId, _))
      .WillOnce([&](uint32_t, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardListReadersResult::NewReaders(
                kExpectedReaders));
      });

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr> future;
  context_->ListReaders(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_readers());
  EXPECT_EQ(result->get_readers(), kExpectedReaders);
}

TEST_F(EmulatedSmartCardContextTest, ListReaders_Failure) {
  const uint32_t kContextId = 123;

  EXPECT_CALL(*manager(), OnListReaders(kContextId, _))
      .WillOnce([](uint32_t, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardListReadersResult::NewError(
                device::mojom::SmartCardError::kNoService));
      });

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr> future;
  context_->ListReaders(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

TEST_F(EmulatedSmartCardContextTest, ListReaders_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr> future;
  context_->ListReaders(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

}  // namespace
}  // namespace content
