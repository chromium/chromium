// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/test_message_loop.h"
#include "media/base/mock_filters.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#include "media/mojo/services/mojo_cdm_proxy.h"
#include "media/mojo/services/mojo_cdm_proxy_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {

MATCHER_P(StatusEq, status, "") {
  return (arg == cdm::CdmProxyClient::Status::kOk &&
          status == media::CdmProxy::Status::kOk) ||
         (arg == cdm::CdmProxyClient::Status::kFail &&
          status == media::CdmProxy::Status::kFail);
}

constexpr uint32_t kCryptoSessionId = 1010;

class MockCdmProxyContext : public CdmProxyContext {};

class MockCdmProxy : public media::CdmProxy, public media::CdmContext {
 public:
  MockCdmProxy() {}
  ~MockCdmProxy() override = default;

  // media::CdmProxy implementation.

  base::WeakPtr<CdmContext> GetCdmContext() override {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD2(Initialize, void(Client* client, InitializeCB init_cb));

  MOCK_METHOD5(Process,
               void(Function function,
                    uint32_t crypto_session_id,
                    const std::vector<uint8_t>& input_data,
                    uint32_t expected_output_data_size,
                    ProcessCB process_cb));

  MOCK_METHOD2(CreateMediaCryptoSession,
               void(const std::vector<uint8_t>& input_data,
                    CreateMediaCryptoSessionCB create_media_crypto_session_cb));

  MOCK_METHOD5(SetKey,
               void(uint32_t crypto_session_id,
                    const std::vector<uint8_t>& key_id,
                    KeyType key_type,
                    const std::vector<uint8_t>& key_blob,
                    SetKeyCB set_key_cb));
  MOCK_METHOD3(RemoveKey,
               void(uint32_t crypto_session_id,
                    const std::vector<uint8_t>& key_id,
                    RemoveKeyCB remove_key_cb));

  // media::CdmContext implementation.
  CdmProxyContext* GetCdmProxyContext() override {
    return &mock_cdm_proxy_context_;
  }

 private:
  MockCdmProxyContext mock_cdm_proxy_context_;
  base::WeakPtrFactory<MockCdmProxy> weak_factory_{this};
};

class MockCdmProxyClient : public cdm::CdmProxyClient {
 public:
  MockCdmProxyClient() = default;
  ~MockCdmProxyClient() override = default;

  MOCK_METHOD3(OnInitialized,
               void(Status status,
                    Protocol protocol,
                    uint32_t crypto_session_id));
  MOCK_METHOD3(OnProcessed,
               void(Status status,
                    const uint8_t* output_data,
                    uint32_t output_data_size));
  MOCK_METHOD3(OnMediaCryptoSessionCreated,
               void(Status status,
                    uint32_t crypto_session_id,
                    uint64_t output_data));
  MOCK_METHOD1(OnKeySet, void(Status status));
  MOCK_METHOD1(OnKeyRemoved, void(Status status));
  MOCK_METHOD0(NotifyHardwareReset, void());
};

}  // namespace

class MojoCdmProxyTest : public ::testing::Test {
 public:
  using Status = CdmProxy::Status;

  MojoCdmProxyTest() {
    // Client side setup.
    mojo::PendingRemote<mojom::CdmProxy> cdm_proxy_remote;
    auto receiver = cdm_proxy_remote.InitWithNewPipeAndPassReceiver();
    mojo_cdm_proxy_.reset(
        new MojoCdmProxy(std::move(cdm_proxy_remote), &client_));
    cdm_proxy_ = mojo_cdm_proxy_.get();

    // Service side setup.
    std::unique_ptr<MockCdmProxy> mock_cdm_proxy(new MockCdmProxy());
    mock_cdm_proxy_ = mock_cdm_proxy.get();
    mojo_cdm_proxy_service_.reset(new MojoCdmProxyService(
        std::move(mock_cdm_proxy), &mojo_cdm_service_context_));
    receiver_.reset(new mojo::Receiver<mojom::CdmProxy>(
        mojo_cdm_proxy_service_.get(), std::move(receiver)));
    receiver_->set_disconnect_handler(base::BindOnce(
        &MojoCdmProxyTest::OnConnectionError, base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  ~MojoCdmProxyTest() override = default;

  void Initialize(Status expected_status = Status::kOk,
                  bool has_connection = true) {
    if (has_connection) {
      EXPECT_CALL(*mock_cdm_proxy_, Initialize(NotNull(), _))
          .WillOnce([&](auto, auto init_cb) {
            std::move(init_cb).Run(expected_status, CdmProxy::Protocol::kNone,
                                   kCryptoSessionId);
          });
      EXPECT_CALL(client_,
                  OnInitialized(StatusEq(expected_status),
                                cdm::CdmProxyClient::kNone, kCryptoSessionId))
          .WillOnce(SaveArg<2>(&crypto_session_id_));
    } else {
      // Client should always be called even without connection. But we only
      // care about status in this case.
      EXPECT_CALL(client_, OnInitialized(StatusEq(expected_status), _, _));
    }

    cdm_proxy_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void Process(Status expected_status = Status::kOk,
               bool has_connection = true) {
    const std::vector<uint8_t> kInputData = {1, 2};
    const uint32_t kExpectedOutputDataSize = 111;
    const std::vector<uint8_t> kOutputData = {3, 4, 5};

    if (has_connection) {
      EXPECT_CALL(
          *mock_cdm_proxy_,
          Process(CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange,
                  crypto_session_id_, kInputData, kExpectedOutputDataSize, _))
          .WillOnce([&](auto, auto, auto, auto, auto process_cb) {
            std::move(process_cb).Run(expected_status, kOutputData);
          });
      EXPECT_CALL(client_, OnProcessed(StatusEq(expected_status), NotNull(),
                                       kOutputData.size()));
    } else {
      // Client should always be called even without connection. But we only
      // care about status in this case.
      EXPECT_CALL(client_, OnProcessed(StatusEq(expected_status), _, _));
    }

    cdm_proxy_->Process(
        cdm::CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange,
        crypto_session_id_, kInputData.data(), kInputData.size(),
        kExpectedOutputDataSize);
    base::RunLoop().RunUntilIdle();
  }

  void CreateMediaCryptoSession(Status expected_status = Status::kOk,
                                bool has_connection = true) {
    const std::vector<uint8_t> kInputData = {6, 7};
    const uint32_t kMediaCryptoSessionId = 222;
    const uint64_t kOutputData = 333;

    if (has_connection) {
      EXPECT_CALL(*mock_cdm_proxy_, CreateMediaCryptoSession(kInputData, _))
          .WillOnce([&](auto, auto create_media_crypto_session_cb) {
            std::move(create_media_crypto_session_cb)
                .Run(expected_status, kMediaCryptoSessionId, kOutputData);
          });
      EXPECT_CALL(client_, OnMediaCryptoSessionCreated(
                               StatusEq(expected_status), kMediaCryptoSessionId,
                               kOutputData));
    } else {
      // Client should always be called even without connection. But we only
      // care about status in this case.
      EXPECT_CALL(client_,
                  OnMediaCryptoSessionCreated(StatusEq(expected_status), _, _));
    }

    cdm_proxy_->CreateMediaCryptoSession(kInputData.data(), kInputData.size());
    base::RunLoop().RunUntilIdle();
  }

  void SetKey() {
    const std::vector<uint8_t> key_id = {8, 9};
    const std::vector<uint8_t> key_blob = {10, 11, 12};
    EXPECT_CALL(*mock_cdm_proxy_,
                SetKey(crypto_session_id_, key_id, _, key_blob, _))
        .WillOnce([&](auto, auto, auto, auto, auto set_key_cb) {
          std::move(set_key_cb).Run(Status::kOk);
        });
    EXPECT_CALL(client_, OnKeySet(StatusEq(Status::kOk)));
    cdm_proxy_->SetKey(crypto_session_id_, key_id.data(), key_id.size(),
                       cdm::CdmProxy::KeyType::kDecryptOnly, key_blob.data(),
                       key_blob.size());
    base::RunLoop().RunUntilIdle();
  }

  void RemoveKey() {
    const std::vector<uint8_t> key_id = {13, 14};
    EXPECT_CALL(*mock_cdm_proxy_, RemoveKey(crypto_session_id_, key_id, _))
        .WillOnce([&](auto, auto, auto remove_key_cb) {
          std::move(remove_key_cb).Run(Status::kOk);
        });
    EXPECT_CALL(client_, OnKeyRemoved(StatusEq(Status::kOk)));
    cdm_proxy_->RemoveKey(crypto_session_id_, key_id.data(), key_id.size());
    base::RunLoop().RunUntilIdle();
  }

  // Simulate connecting the media component with the CdmContext. Can only be
  // called after the CdmProxy is successfully initialized (see Initialize()).
  void SetCdm() {
    int cdm_id = mojo_cdm_proxy_service_->GetCdmIdForTesting();
    cdm_context_ref_ = mojo_cdm_service_context_.GetCdmContextRef(cdm_id);
    cdm_context_ = cdm_context_ref_->GetCdmContext();
  }

  CdmProxyContext* GetCdmProxyContext() {
    return cdm_context_->GetCdmProxyContext();
  }

  void Destroy() {
    mojo_cdm_proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnConnectionError() { mojo_cdm_proxy_service_.reset(); }

  void ForceConnectionError() {
    receiver_->ResetWithReason(2, "Test closed connection.");
    mojo_cdm_proxy_service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::TestMessageLoop message_loop_;
  uint32_t crypto_session_id_ = 0;

  // Client side members.
  StrictMock<MockCdmProxyClient> client_;
  std::unique_ptr<MojoCdmProxy> mojo_cdm_proxy_;
  cdm::CdmProxy* cdm_proxy_ = nullptr;

  // Service side members.
  MojoCdmServiceContext mojo_cdm_service_context_;
  std::unique_ptr<MojoCdmProxyService> mojo_cdm_proxy_service_;
  std::unique_ptr<mojo::Receiver<mojom::CdmProxy>> receiver_;
  MockCdmProxy* mock_cdm_proxy_ = nullptr;

  // Media component side members.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;
  CdmContext* cdm_context_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoCdmProxyTest);
};

TEST_F(MojoCdmProxyTest, Initialize) {
  Initialize();
}

TEST_F(MojoCdmProxyTest, Initialize_Failure) {
  Initialize(Status::kFail);
}

TEST_F(MojoCdmProxyTest, Initialize_Twice) {
  Initialize();
  EXPECT_CHECK_DEATH(Initialize());
}

TEST_F(MojoCdmProxyTest, Process) {
  Initialize();
  Process();
}

TEST_F(MojoCdmProxyTest, Process_Failure) {
  Initialize();
  Process(Status::kFail);
}

TEST_F(MojoCdmProxyTest, CreateMediaCryptoSession) {
  Initialize();
  Process();
  CreateMediaCryptoSession();
}

TEST_F(MojoCdmProxyTest, CreateMediaCryptoSession_Failure) {
  Initialize();
  Process();
  CreateMediaCryptoSession(Status::kFail);
}

TEST_F(MojoCdmProxyTest, SetKey) {
  Initialize();
  Process();
  CreateMediaCryptoSession();
  SetKey();
}

TEST_F(MojoCdmProxyTest, RemoveKey) {
  Initialize();
  Process();
  CreateMediaCryptoSession();
  RemoveKey();
}

TEST_F(MojoCdmProxyTest, Destroy) {
  Initialize();
  Process();
  EXPECT_TRUE(mojo_cdm_proxy_service_);
  Destroy();
  EXPECT_FALSE(mojo_cdm_proxy_service_);
}

TEST_F(MojoCdmProxyTest, ConnectionError_BeforeInitialize) {
  ForceConnectionError();
  Initialize(Status::kFail, false);
}

TEST_F(MojoCdmProxyTest, ConnectionError_AfterInitialize) {
  Initialize();
  Process();
  CreateMediaCryptoSession();

  ForceConnectionError();

  // Calling Process() and CreateMediaCryptoSession() without connection. These
  // calls should fail but the client should still get notified (about the
  // failure).
  Process(Status::kFail, false);
  CreateMediaCryptoSession(Status::kFail, false);
}

TEST_F(MojoCdmProxyTest, GetCdmProxyContext) {
  Initialize();
  SetCdm();
  EXPECT_TRUE(GetCdmProxyContext());
}

TEST_F(MojoCdmProxyTest, GetCdmProxyContext_AfterDestroy) {
  Initialize();
  SetCdm();
  EXPECT_TRUE(GetCdmProxyContext());
  Destroy();
  EXPECT_FALSE(GetCdmProxyContext());
}

TEST_F(MojoCdmProxyTest, GetCdmProxyContext_AfterConnectionError) {
  Initialize();
  SetCdm();
  EXPECT_TRUE(GetCdmProxyContext());
  ForceConnectionError();
  EXPECT_FALSE(GetCdmProxyContext());
}

}  // namespace media
