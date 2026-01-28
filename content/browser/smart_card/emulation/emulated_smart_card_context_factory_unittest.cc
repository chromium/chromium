// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_connection.h"
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

  MOCK_METHOD(void,
              OnGetStatusChange,
              (uint32_t,
               base::TimeDelta,
               std::vector<device::mojom::SmartCardReaderStateInPtr>,
               device::mojom::SmartCardContext::GetStatusChangeCallback),
              (override));

  MOCK_METHOD(void,
              OnCancel,
              (uint32_t, device::mojom::SmartCardContext::CancelCallback),
              (override));

  MOCK_METHOD(void,
              OnConnect,
              (uint32_t,
               const std::string&,
               device::mojom::SmartCardShareMode,
               device::mojom::SmartCardProtocolsPtr,
               mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher>,
               device::mojom::SmartCardContext::ConnectCallback),
              (override));

  MOCK_METHOD(void,
              OnDisconnect,
              (uint32_t,
               device::mojom::SmartCardDisposition,
               device::mojom::SmartCardConnection::DisconnectCallback),
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

TEST_F(EmulatedSmartCardContextTest, GetStatusChange_Success) {
  const uint32_t kContextId = 123;
  base::TimeDelta timeout = base::Seconds(5);

  std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;
  auto state = device::mojom::SmartCardReaderStateIn::New();
  state->reader = "Reader A";
  reader_states.push_back(std::move(state));

  EXPECT_CALL(*manager(), OnGetStatusChange(kContextId, timeout, _, _))
      .WillOnce([&](uint32_t, base::TimeDelta, auto states, auto callback) {
        EXPECT_EQ(states.size(), 1u);
        EXPECT_EQ(states[0]->reader, "Reader A");

        std::vector<device::mojom::SmartCardReaderStateOutPtr> results;
        std::move(callback).Run(
            device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                std::move(results)));
      });

  base::test::TestFuture<device::mojom::SmartCardStatusChangeResultPtr> future;
  context_->GetStatusChange(timeout, std::move(reader_states),
                            future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_reader_states());
}

TEST_F(EmulatedSmartCardContextTest, GetStatusChange_Failure) {
  const uint32_t kContextId = 123;

  std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;
  reader_states.push_back(device::mojom::SmartCardReaderStateIn::New());

  EXPECT_CALL(*manager(), OnGetStatusChange(kContextId, _, _, _))
      .WillOnce([](uint32_t, base::TimeDelta, auto, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardStatusChangeResult::NewError(
                device::mojom::SmartCardError::kTimeout));
      });

  base::test::TestFuture<device::mojom::SmartCardStatusChangeResultPtr> future;
  context_->GetStatusChange(/*timeout=*/base::Seconds(1),
                            std::move(reader_states), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kTimeout);
}

TEST_F(EmulatedSmartCardContextTest, GetStatusChange_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardStatusChangeResultPtr> future;
  context_->GetStatusChange(base::Seconds(1), {}, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

TEST_F(EmulatedSmartCardContextTest, Cancel_Success) {
  const uint32_t kContextId = 123;

  EXPECT_CALL(*manager(), OnCancel(kContextId, _))
      .WillOnce([](uint32_t, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
            device::mojom::SmartCardSuccess::kOk));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  context_->Cancel(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
}

TEST_F(EmulatedSmartCardContextTest, Cancel_Failure) {
  const uint32_t kContextId = 123;

  EXPECT_CALL(*manager(), OnCancel(kContextId, _))
      .WillOnce([](uint32_t, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewError(
            device::mojom::SmartCardError::kInvalidHandle));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  context_->Cancel(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kInvalidHandle);
}

TEST_F(EmulatedSmartCardContextTest, Cancel_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  context_->Cancel(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

TEST_F(EmulatedSmartCardContextTest, Connect_Success) {
  const uint32_t kContextId = 123;
  std::string reader_name = "Reader A";
  auto share_mode = device::mojom::SmartCardShareMode::kShared;
  auto protocols = device::mojom::SmartCardProtocols::New(true, true, false);

  mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher_remote;
  std::ignore = watcher_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<device::mojom::SmartCardConnection> connection_remote;
  mojo::PendingReceiver<device::mojom::SmartCardConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();

  auto success_result = device::mojom::SmartCardConnectResult::NewSuccess(
      device::mojom::SmartCardConnectSuccess::New(
          std::move(connection_remote), device::mojom::SmartCardProtocol::kT1));

  EXPECT_CALL(*manager(),
              OnConnect(kContextId, reader_name, share_mode, _, _, _))
      .WillOnce(base::test::RunOnceCallback<5>(std::move(success_result)));

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr> future;
  context_->Connect(reader_name, share_mode, std::move(protocols),
                    std::move(watcher_remote), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
  EXPECT_EQ(result->get_success()->active_protocol,
            device::mojom::SmartCardProtocol::kT1);

  EXPECT_TRUE(result->get_success()->connection.is_valid());
}

TEST_F(EmulatedSmartCardContextTest, Connect_Failure) {
  const uint32_t kContextId = 123;

  EXPECT_CALL(*manager(), OnConnect(kContextId, _, _, _, _, _))
      .WillOnce([](uint32_t, auto, auto, auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardConnectResult::NewError(
            device::mojom::SmartCardError::kNoSmartcard));
      });

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr> future;
  context_->Connect("Reader A", device::mojom::SmartCardShareMode::kShared,
                    device::mojom::SmartCardProtocols::New(true, false, false),
                    mojo::NullRemote(), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoSmartcard);
}

TEST_F(EmulatedSmartCardContextTest, Connect_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr> future;
  context_->Connect("Reader A", device::mojom::SmartCardShareMode::kShared,
                    device::mojom::SmartCardProtocols::New(true, false, false),
                    mojo::NullRemote(), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNoService);
}

class EmulatedSmartCardConnectionTest
    : public EmulatedSmartCardContextFactoryTest {
 public:
  void SetUp() override {
    connection_ = std::make_unique<EmulatedSmartCardConnection>(
        manager()->GetWeakPtr(), 555);
  }

 protected:
  std::unique_ptr<EmulatedSmartCardConnection> connection_;
};

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_Success) {
  const uint32_t kConnectionId = 555;
  const auto kDisposition = device::mojom::SmartCardDisposition::kEject;

  EXPECT_CALL(*manager(), OnDisconnect(kConnectionId, kDisposition, _))
      .WillOnce([](uint32_t, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
            device::mojom::SmartCardSuccess::kOk));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->Disconnect(kDisposition, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
}

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_Failure) {
  const uint32_t kConnectionId = 555;

  EXPECT_CALL(*manager(), OnDisconnect(kConnectionId, _, _))
      .WillOnce([](uint32_t, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewError(
            device::mojom::SmartCardError::kInvalidHandle));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->Disconnect(device::mojom::SmartCardDisposition::kLeave,
                          future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kInvalidHandle);
}

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->Disconnect(device::mojom::SmartCardDisposition::kLeave,
                          future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
}

}  // namespace
}  // namespace content
