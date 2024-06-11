// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service.h"

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/mock_filters.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media {

namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;

MATCHER_P(MatchesResult, success, "") {
  return arg->success == success;
}

// MockCdmFactory treats any non-empty key system as valid and the empty key
// system as invalid.
const char kInvalidKeySystem[] = "";

// Needed since MockCdmServiceClient needs to return unique_ptr of CdmFactory.
class CdmFactoryWrapper : public CdmFactory {
 public:
  explicit CdmFactoryWrapper(CdmFactory* cdm_factory)
      : cdm_factory_(cdm_factory) {}

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override {
    cdm_factory_->Create(cdm_config, session_message_cb, session_closed_cb,
                         session_keys_change_cb, session_expiration_update_cb,
                         std::move(cdm_created_cb));
  }

 private:
  const raw_ptr<CdmFactory> cdm_factory_;
};

class MockCdmServiceClient : public CdmService::Client {
 public:
  explicit MockCdmServiceClient(CdmFactory* cdm_factory)
      : cdm_factory_(cdm_factory) {}
  ~MockCdmServiceClient() override = default;

  // CdmService::Client implementation.
  MOCK_METHOD0(EnsureSandboxed, void());

  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) override {
    return std::make_unique<CdmFactoryWrapper>(cdm_factory_);
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(std::vector<CdmHostFilePath>*) override {}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

 private:
  const raw_ptr<CdmFactory> cdm_factory_;
};

class CdmServiceTest : public testing::Test {
 public:
  CdmServiceTest() = default;

  CdmServiceTest(const CdmServiceTest&) = delete;
  CdmServiceTest& operator=(const CdmServiceTest&) = delete;

  ~CdmServiceTest() override = default;

  MOCK_METHOD0(CdmServiceIdle, void());
  MOCK_METHOD0(CdmFactoryConnectionClosed, void());
  MOCK_METHOD0(CdmConnectionClosed, void());

  void Initialize() {
    EXPECT_CALL(*mock_cdm_, GetCdmContext())
        .WillRepeatedly(Return(&cdm_context_));

    auto mock_cdm_service_client =
        std::make_unique<MockCdmServiceClient>(&mock_cdm_factory_);
    mock_cdm_service_client_ = mock_cdm_service_client.get();

    service_ = std::make_unique<CdmService>(
        std::move(mock_cdm_service_client),
        cdm_service_remote_.BindNewPipeAndPassReceiver());
    cdm_service_remote_.set_idle_handler(
        base::TimeDelta(), base::BindRepeating(&CdmServiceTest::CdmServiceIdle,
                                               base::Unretained(this)));

    mojo::PendingRemote<mojom::FrameInterfaceFactory> interfaces;
    std::ignore = interfaces.InitWithNewPipeAndPassReceiver();

    ASSERT_FALSE(cdm_factory_remote_);
    cdm_service_remote_->CreateCdmFactory(
        cdm_factory_remote_.BindNewPipeAndPassReceiver(),
        std::move(interfaces));
    cdm_service_remote_.FlushForTesting();
    ASSERT_TRUE(cdm_factory_remote_);
    cdm_factory_remote_.set_disconnect_handler(base::BindOnce(
        &CdmServiceTest::CdmFactoryConnectionClosed, base::Unretained(this)));
  }

  void InitializeCdm(const std::string& key_system, bool expected_result) {
    cdm_factory_remote_->CreateCdm(
        {key_system, false, false, false},
        base::BindOnce(&CdmServiceTest::OnCdmCreated, base::Unretained(this),
                       expected_result));
    cdm_factory_remote_.FlushForTesting();
  }

  void DestroyService() { service_.reset(); }

  MockCdmServiceClient* mock_cdm_service_client() {
    return mock_cdm_service_client_;
  }

  CdmService* cdm_service() { return service_.get(); }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::CdmService> cdm_service_remote_;
  mojo::Remote<mojom::CdmFactory> cdm_factory_remote_;
  mojo::Remote<mojom::ContentDecryptionModule> cdm_remote_;

  // MojoCdmService will always create/use `mock_cdm_factory_` and `mock_cdm_`,
  // so it's easier to set expectations on them.
  scoped_refptr<MockCdm> mock_cdm_{new MockCdm()};
  MockCdmFactory mock_cdm_factory_{mock_cdm_};
  NiceMock<MockCdmContext> cdm_context_;

 private:
  void OnCdmCreated(bool expected_result,
                    mojo::PendingRemote<mojom::ContentDecryptionModule> remote,
                    mojom::CdmContextPtr cdm_context,
                    CreateCdmStatus status) {
    if (!expected_result) {
      EXPECT_FALSE(remote);
      EXPECT_FALSE(cdm_context);
      EXPECT_NE(status, CreateCdmStatus::kSuccess);
      return;
    }
    EXPECT_TRUE(remote);
    EXPECT_EQ(status, CreateCdmStatus::kSuccess);
    cdm_remote_.Bind(std::move(remote));
    cdm_remote_.set_disconnect_handler(base::BindOnce(
        &CdmServiceTest::CdmConnectionClosed, base::Unretained(this)));
  }
  std::unique_ptr<CdmService> service_;
  raw_ptr<MockCdmServiceClient, AcrossTasksDanglingUntriaged>
      mock_cdm_service_client_ = nullptr;
};

}  // namespace

TEST_F(CdmServiceTest, InitializeCdm_Success) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);
}

TEST_F(CdmServiceTest, InitializeCdm_InvalidKeySystem) {
  Initialize();
  InitializeCdm(kInvalidKeySystem, false);
}

TEST_F(CdmServiceTest, DestroyAndRecreateCdm) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);
  cdm_remote_.reset();
  InitializeCdm(kClearKeyKeySystem, true);
}

// CdmFactory disconnection will cause the service to idle.
TEST_F(CdmServiceTest, DestroyCdmFactory) {
  Initialize();
  auto* service = cdm_service();

  InitializeCdm(kClearKeyKeySystem, true);
  EXPECT_EQ(service->BoundCdmFactorySizeForTesting(), 1u);
  EXPECT_EQ(service->UnboundCdmFactorySizeForTesting(), 0u);

  cdm_factory_remote_.reset();
  EXPECT_CALL(*this, CdmServiceIdle());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service->BoundCdmFactorySizeForTesting(), 0u);
  EXPECT_EQ(service->UnboundCdmFactorySizeForTesting(), 1u);
}

// Destroy service will destroy the CdmFactory and all CDMs.
TEST_F(CdmServiceTest, DestroyCdmService_AfterCdmCreation) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);

  base::RunLoop run_loop;
  // Ideally we should not care about order, and should only quit the loop when
  // both connections are closed.
  EXPECT_CALL(*this, CdmFactoryConnectionClosed());
  EXPECT_CALL(*this, CdmConnectionClosed())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  DestroyService();
  run_loop.Run();
}

// Before the CDM is fully created, CdmService has been destroyed. We should
// fail gracefully instead of a crash. See crbug.com/1190319.
TEST_F(CdmServiceTest, DestroyCdmService_DuringCdmCreation) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, CdmFactoryConnectionClosed())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  mock_cdm_factory_.SetBeforeCreationCB(base::BindRepeating(
      &CdmServiceTest::DestroyService, base::Unretained(this)));
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);
  run_loop.Run();
}

}  // namespace media
