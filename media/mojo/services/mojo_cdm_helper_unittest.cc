// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using Status = cdm::FileIOClient::Status;

namespace media {

namespace {

class MockFileIOClient : public cdm::FileIOClient {
 public:
  MockFileIOClient() = default;
  ~MockFileIOClient() override = default;

  MOCK_METHOD1(OnOpenComplete, void(Status));
  MOCK_METHOD3(OnReadComplete, void(Status, const uint8_t*, uint32_t));
  MOCK_METHOD1(OnWriteComplete, void(Status));
};

class TestCdmFile : public mojom::CdmFile {
 public:
  TestCdmFile() = default;
  ~TestCdmFile() override = default;

  // Reading always succeeds with a 3-byte buffer.
  void Read(ReadCallback callback) override {
    std::move(callback).Run(Status::kSuccess, {1, 2, 3});
  }

  // Writing always succeeds.
  void Write(const std::vector<uint8_t>& data,
             WriteCallback callback) override {
    std::move(callback).Run(Status::kSuccess);
  }
};

class MockCdmStorage : public mojom::CdmStorage {
 public:
  MockCdmStorage() = default;
  ~MockCdmStorage() override = default;

  void Open(const std::string& file_name, OpenCallback callback) override {
    std::move(callback).Run(mojom::CdmStorage::Status::kSuccess,
                            client_receiver_.BindNewEndpointAndPassRemote());
  }

 private:
  TestCdmFile cdm_file_;
  mojo::AssociatedReceiver<mojom::CdmFile> client_receiver_{&cdm_file_};
};

class TestFrameInterfaceFactory : public mojom::FrameInterfaceFactory {
 public:
  void CreateProvisionFetcher(
      mojo::PendingReceiver<mojom::ProvisionFetcher>) override {}
  void CreateCdmStorage(
      mojo::PendingReceiver<mojom::CdmStorage> receiver) override {
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockCdmStorage>(),
                                std::move(receiver));
  }
#if BUILDFLAG(IS_WIN)
  void RegisterMuteStateObserver(
      mojo::PendingRemote<mojom::MuteStateObserver> observer) override {}
  void CreateDCOMPSurfaceRegistry(
      mojo::PendingReceiver<mojom::DCOMPSurfaceRegistry> receiver) override {}
#endif  // BUILDFLAG(IS_WIN)
  void GetCdmOrigin(GetCdmOriginCallback callback) override {}
  void BindEmbedderReceiver(mojo::GenericPendingReceiver) override {}
};

}  // namespace

class MojoCdmHelperTest : public testing::Test {
 protected:
  MojoCdmHelperTest() : helper_(&frame_interfaces_) {}
  ~MojoCdmHelperTest() override = default;

  base::test::TaskEnvironment task_environment_;
  TestFrameInterfaceFactory frame_interfaces_;
  MockFileIOClient file_io_client_;
  MojoCdmHelper helper_;
};

TEST_F(MojoCdmHelperTest, CreateCdmFileIO_OpenClose) {
  cdm::FileIO* file_io = helper_.CreateCdmFileIO(&file_io_client_);
  const std::string kFileName = "openfile";
  EXPECT_CALL(file_io_client_, OnOpenComplete(Status::kSuccess));
  file_io->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  // Close the file as required by cdm::FileIO API.
  file_io->Close();
  base::RunLoop().RunUntilIdle();
}

// Simulate the case where the CDM didn't call Close(). In this case we still
// should not leak the cdm::FileIO object. LeakSanitizer bots should be able to
// catch such issues.
TEST_F(MojoCdmHelperTest, CreateCdmFileIO_OpenWithoutClose) {
  cdm::FileIO* file_io = helper_.CreateCdmFileIO(&file_io_client_);
  const std::string kFileName = "openfile";
  EXPECT_CALL(file_io_client_, OnOpenComplete(Status::kSuccess));
  file_io->Open(kFileName.data(), kFileName.length());
  // file_io->Close() is NOT called.
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/40544338): Add more test cases.

}  // namespace media
