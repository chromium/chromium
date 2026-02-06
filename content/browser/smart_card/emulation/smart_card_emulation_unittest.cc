// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_connection.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_context.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"
#include "content/browser/smart_card/emulation/emulated_smart_card_transaction.h"
#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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

  MOCK_METHOD(void,
              OnControl,
              (uint32_t,
               uint32_t,
               const std::vector<uint8_t>&,
               device::mojom::SmartCardConnection::ControlCallback),
              (override));

  MOCK_METHOD(void,
              OnGetAttrib,
              (uint32_t,
               uint32_t,
               device::mojom::SmartCardConnection::GetAttribCallback),
              (override));

  MOCK_METHOD(void,
              OnTransmit,
              (uint32_t,
               device::mojom::SmartCardProtocol,
               const std::vector<uint8_t>&,
               device::mojom::SmartCardConnection::TransmitCallback),
              (override));

  MOCK_METHOD(void,
              OnStatus,
              (uint32_t, device::mojom::SmartCardConnection::StatusCallback),
              (override));

  MOCK_METHOD(void,
              OnSetAttrib,
              (uint32_t,
               uint32_t,
               const std::vector<uint8_t>&,
               device::mojom::SmartCardConnection::SetAttribCallback),
              (override));

  MOCK_METHOD(void,
              OnBeginTransaction,
              (uint32_t,
               device::mojom::SmartCardConnection::BeginTransactionCallback),
              (override));

  MOCK_METHOD(void,
              OnEndTransaction,
              (uint32_t,
               device::mojom::SmartCardDisposition,
               device::mojom::SmartCardTransaction::EndTransactionCallback),
              (override));

  MOCK_METHOD(void, OnReleaseContext, (uint32_t), (override));

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
        manager()->GetWeakPtr(), context_id);
  }

  void TearDown() override {
    context_.reset();
    EmulatedSmartCardContextFactoryTest::TearDown();
  }

  void ExpectContextRelease() {
    EXPECT_CALL(*manager(), OnReleaseContext(context_id)).Times(1);
  }

 protected:
  std::unique_ptr<EmulatedSmartCardContext> context_;
  const int context_id = 123;
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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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
  ExpectContextRelease();

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

class MockSmartCardConnectionWatcher
    : public device::mojom::SmartCardConnectionWatcher {
 public:
  MOCK_METHOD(void, NotifyConnectionUsed, (), (override));
};

class EmulatedSmartCardConnectionTest
    : public EmulatedSmartCardContextFactoryTest {
 public:
  void SetUp() override {
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher>
        watcher_remote;
    watcher_receiver_.Bind(watcher_remote.InitWithNewPipeAndPassReceiver());

    EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());

    connection_ = std::make_unique<EmulatedSmartCardConnection>(
        manager()->GetWeakPtr(), kConnectionId, std::move(watcher_remote));
    watcher_receiver_.FlushForTesting();
  }

 protected:
  testing::StrictMock<MockSmartCardConnectionWatcher> mock_watcher_;
  mojo::Receiver<device::mojom::SmartCardConnectionWatcher> watcher_receiver_{
      &mock_watcher_};
  std::unique_ptr<EmulatedSmartCardConnection> connection_;
  const uint32_t kConnectionId = 555;
};

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_Success) {
  const auto kDisposition = device::mojom::SmartCardDisposition::kEject;

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());

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

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());

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

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Disconnect_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->Disconnect(device::mojom::SmartCardDisposition::kLeave,
                          future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
}

TEST_F(EmulatedSmartCardConnectionTest, Control_Success) {
  const uint32_t kControlCode = 42;
  const std::vector<uint8_t> kInData = {1, 2, 3};
  const std::vector<uint8_t> kOutData = {4, 5, 6};

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());

  EXPECT_CALL(*manager(), OnControl(kConnectionId, kControlCode, kInData, _))
      .WillOnce(
          [&](uint32_t, uint32_t, const std::vector<uint8_t>&, auto callback) {
            std::move(callback).Run(
                device::mojom::SmartCardDataResult::NewData(kOutData));
          });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Control(kControlCode, kInData, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data(), kOutData);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Control_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnControl(kConnectionId, _, _, _))
      .WillOnce([](auto, auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
            device::mojom::SmartCardError::kInvalidParameter));
      });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Control(123, {}, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kInvalidParameter);
  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Control_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Control(0xCAFE, {1, 2}, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

TEST_F(EmulatedSmartCardConnectionTest, GetAttrib_Success) {
  const uint32_t kAttrId = 1001;
  const std::vector<uint8_t> kAttrValue = {'F', 'a', 'k', 'e'};

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnGetAttrib(kConnectionId, kAttrId, _))
      .WillOnce([&](uint32_t, uint32_t, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardDataResult::NewData(kAttrValue));
      });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->GetAttrib(kAttrId, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data(), kAttrValue);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, GetAttrib_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnGetAttrib(kConnectionId, _, _))
      .WillOnce([](auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
            device::mojom::SmartCardError::kUnknownReader));
      });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->GetAttrib(999, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kUnknownReader);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, GetAttrib_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->GetAttrib(1234, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

TEST_F(EmulatedSmartCardConnectionTest, Transmit_Success) {
  const auto kProtocol = device::mojom::SmartCardProtocol::kT1;
  const std::vector<uint8_t> kApdu = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> kResponse = {0x90, 0x00};

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnTransmit(kConnectionId, kProtocol, kApdu, _))
      .WillOnce([&](uint32_t, auto, auto, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardDataResult::NewData(kResponse));
      });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Transmit(kProtocol, kApdu, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data(), kResponse);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Transmit_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnTransmit(kConnectionId, _, _, _))
      .WillOnce([](auto, auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
            device::mojom::SmartCardError::kProtoMismatch));
      });

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Transmit(device::mojom::SmartCardProtocol::kT0, {},
                        future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kProtoMismatch);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Transmit_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> future;
  connection_->Transmit(device::mojom::SmartCardProtocol::kT1, {1, 2, 3},
                        future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

TEST_F(EmulatedSmartCardConnectionTest, Status_Success) {
  const std::vector<uint8_t> kAtr = {0x3B, 0x00};

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnStatus(kConnectionId, _))
      .WillOnce([&](uint32_t, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardStatusResult::NewStatus(
            device::mojom::SmartCardStatus::New(
                "Reader A", device::mojom::SmartCardConnectionState::kSpecific,
                device::mojom::SmartCardProtocol::kT1, kAtr)));
      });

  base::test::TestFuture<device::mojom::SmartCardStatusResultPtr> future;
  connection_->Status(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_status());
  EXPECT_EQ(result->get_status()->reader_name, "Reader A");
  EXPECT_EQ(result->get_status()->answer_to_reset, kAtr);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Status_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnStatus(kConnectionId, _))
      .WillOnce([](uint32_t, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardStatusResult::NewError(
            device::mojom::SmartCardError::kReaderUnavailable));
      });

  base::test::TestFuture<device::mojom::SmartCardStatusResultPtr> future;
  connection_->Status(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kReaderUnavailable);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, Status_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardStatusResultPtr> future;
  connection_->Status(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

TEST_F(EmulatedSmartCardConnectionTest, SetAttrib_Success) {
  const uint32_t kAttrId = 1234;
  const std::vector<uint8_t> kData = {0x01, 0x02};

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnSetAttrib(kConnectionId, kAttrId, kData, _))
      .WillOnce([&](uint32_t, uint32_t, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
            device::mojom::SmartCardSuccess::kOk));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->SetAttrib(kAttrId, kData, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, SetAttrib_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnSetAttrib(kConnectionId, _, _, _))
      .WillOnce([](auto, auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewError(
            device::mojom::SmartCardError::kUnsupportedCard));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->SetAttrib(1234, {}, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kUnsupportedCard);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, SetAttrib_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  connection_->SetAttrib(1234, {0x01}, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

TEST_F(EmulatedSmartCardConnectionTest, BeginTransaction_Success) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnBeginTransaction(kConnectionId, _))
      .WillOnce([](uint32_t, auto callback) {
        mojo::PendingAssociatedRemote<device::mojom::SmartCardTransaction>
            remote;
        std::ignore = remote.InitWithNewEndpointAndPassReceiver();

        std::move(callback).Run(
            device::mojom::SmartCardTransactionResult::NewTransaction(
                std::move(remote)));
      });

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr> future;
  connection_->BeginTransaction(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_transaction());

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, BeginTransaction_Failure) {
  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed());
  EXPECT_CALL(*manager(), OnBeginTransaction(kConnectionId, _))
      .WillOnce([](uint32_t, auto callback) {
        std::move(callback).Run(
            device::mojom::SmartCardTransactionResult::NewError(
                device::mojom::SmartCardError::kSharingViolation));
      });

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr> future;
  connection_->BeginTransaction(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kSharingViolation);

  watcher_receiver_.FlushForTesting();
}

TEST_F(EmulatedSmartCardConnectionTest, BeginTransaction_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  EXPECT_CALL(mock_watcher_, NotifyConnectionUsed()).Times(0);

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr> future;
  connection_->BeginTransaction(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

class EmulatedSmartCardTransactionTest
    : public EmulatedSmartCardContextFactoryTest {
 public:
  void SetUp() override {
    transaction_ = std::make_unique<EmulatedSmartCardTransaction>(
        manager()->GetWeakPtr(), kConnectionId);
  }

 protected:
  std::unique_ptr<EmulatedSmartCardTransaction> transaction_;
  const uint32_t kConnectionId = 555;
};

TEST_F(EmulatedSmartCardTransactionTest, EndTransaction_Success) {
  const auto kDisposition = device::mojom::SmartCardDisposition::kLeave;

  EXPECT_CALL(*manager(), OnEndTransaction(kConnectionId, kDisposition, _))
      .WillOnce([](uint32_t, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
            device::mojom::SmartCardSuccess::kOk));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  transaction_->EndTransaction(kDisposition, future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->is_success());
}

TEST_F(EmulatedSmartCardTransactionTest, EndTransaction_Failure) {
  EXPECT_CALL(*manager(), OnEndTransaction(kConnectionId, _, _))
      .WillOnce([](auto, auto, auto callback) {
        std::move(callback).Run(device::mojom::SmartCardResult::NewError(
            device::mojom::SmartCardError::kNotTransacted));
      });

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  transaction_->EndTransaction(device::mojom::SmartCardDisposition::kReset,
                               future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), device::mojom::SmartCardError::kNotTransacted);
}

TEST_F(EmulatedSmartCardTransactionTest, EndTransaction_NoService) {
  // Destroy the factory and manager to simulate DevTools closing.
  factory_.reset();
  mock_manager_.reset();

  base::test::TestFuture<device::mojom::SmartCardResultPtr> future;
  transaction_->EndTransaction(device::mojom::SmartCardDisposition::kLeave,
                               future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(),
            device::mojom::SmartCardError::kServiceStopped);
}

}  // namespace
}  // namespace content
