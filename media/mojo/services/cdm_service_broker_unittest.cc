// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service_broker.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/cdm_factory.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class MockCdmServiceClient : public media::CdmService::Client {
 public:
  MockCdmServiceClient() = default;
  ~MockCdmServiceClient() override = default;

  // media::CdmService::Client implementation.
  MOCK_METHOD0(EnsureSandboxed, void());
  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) override {
    return nullptr;
  }
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(std::vector<media::CdmHostFilePath>*) override {}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};

class CdmServiceBrokerTest : public testing::Test {
 public:
  CdmServiceBrokerTest() = default;
  ~CdmServiceBrokerTest() override = default;

  void Initialize() {
    auto mock_cdm_service_client = std::make_unique<MockCdmServiceClient>();
    mock_cdm_service_client_ = mock_cdm_service_client.get();

    broker_ = std::make_unique<CdmServiceBroker>(
        std::move(mock_cdm_service_client),
        remote_.BindNewPipeAndPassReceiver());
  }

  MockCdmServiceClient* mock_cdm_service_client() {
    return mock_cdm_service_client_;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockCdmServiceClient, AcrossTasksDanglingUntriaged>
      mock_cdm_service_client_ = nullptr;
  mojo::Remote<mojom::CdmServiceBroker> remote_;
  std::unique_ptr<CdmServiceBroker> broker_;
};

}  // namespace

TEST_F(CdmServiceBrokerTest, GetService) {
  Initialize();

  // Even with a dummy path where the CDM cannot be loaded, EnsureSandboxed()
  // should still be called to ensure the process is sandboxed.
  EXPECT_CALL(*mock_cdm_service_client(), EnsureSandboxed());

  mojo::Remote<mojom::CdmService> service_remote_;

  base::FilePath cdm_path(FILE_PATH_LITERAL("dummy path"));
#if BUILDFLAG(IS_MAC)
  // Token provider will not be used since the path is a dummy path.
  remote_->GetService(cdm_path, mojo::NullRemote(),
                      service_remote_.BindNewPipeAndPassReceiver());
#else
  remote_->GetService(cdm_path, service_remote_.BindNewPipeAndPassReceiver());
#endif

  remote_.FlushForTesting();
}

}  // namespace media
