// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_file_io.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/cdm/api/content_decryption_module.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Unused;
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

class MockCdmFile : public mojom::CdmFile {
 public:
  MockCdmFile() = default;
  ~MockCdmFile() override = default;

  MOCK_METHOD1(Read, void(ReadCallback));
  MOCK_METHOD2(Write, void(const std::vector<uint8_t>&, WriteCallback));
};

class MockCdmStorage : public mojom::CdmStorage {
 public:
  MockCdmStorage(mojo::PendingReceiver<mojom::CdmStorage> receiver,
                 MockCdmFile* cdm_file)
      : receiver_(this, std::move(receiver)), client_receiver_(cdm_file) {}
  ~MockCdmStorage() override = default;

  // MojoCdmFileIO calls CdmStorage::Open() to open the file. Receivers always
  // succeed.
  void Open(const std::string& file_name, OpenCallback callback) override {
    mojo::PendingAssociatedRemote<mojom::CdmFile> client_remote;
    client_receiver_.Bind(client_remote.InitWithNewEndpointAndPassReceiver());
    std::move(callback).Run(mojom::CdmStorage::Status::kSuccess,
                            std::move(client_remote));

    base::RunLoop().RunUntilIdle();
  }

 private:
  mojo::Receiver<mojom::CdmStorage> receiver_;
  mojo::AssociatedReceiver<mojom::CdmFile> client_receiver_;
};

}  // namespace

// Note that the current browser_test ECKEncryptedMediaTest.FileIOTest
// does test reading and writing files with real data.

class MojoCdmFileIOTest : public testing::Test, public MojoCdmFileIO::Delegate {
 protected:
  MojoCdmFileIOTest() = default;
  ~MojoCdmFileIOTest() override = default;

  // testing::Test implementation.
  void SetUp() override {
    client_ = std::make_unique<MockFileIOClient>();

    mojo::Remote<mojom::CdmStorage> cdm_storage_remote;
    cdm_storage_ = std::make_unique<MockCdmStorage>(
        cdm_storage_remote.BindNewPipeAndPassReceiver(), &cdm_file_);

    file_io_ = std::make_unique<MojoCdmFileIO>(this, client_.get(),
                                               std::move(cdm_storage_remote));
  }

  // MojoCdmFileIO::Delegate implementation.
  void CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) override {
    DCHECK_EQ(file_io_.get(), cdm_file_io);
    file_io_.reset();
  }

  void ReportFileReadSize(int file_size_bytes) override {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MojoCdmFileIO> file_io_;
  std::unique_ptr<MockFileIOClient> client_;
  std::unique_ptr<MockCdmStorage> cdm_storage_;
  MockCdmFile cdm_file_;
};

TEST_F(MojoCdmFileIOTest, OpenFile) {
  const std::string kFileName = "openfile";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, OpenFileTwice) {
  const std::string kFileName = "openfile";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());

  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kError));
  file_io_->Open(kFileName.data(), kFileName.length());

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, OpenFileAfterOpen) {
  const std::string kFileName = "openfile";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());

  // Run now so that the file is opened.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kError));
  file_io_->Open(kFileName.data(), kFileName.length());

  // Run a second time so Open() tries after the file is already open.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, OpenDifferentFiles) {
  const std::string kFileName1 = "openfile1";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName1.data(), kFileName1.length());

  const std::string kFileName2 = "openfile2";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kError));
  file_io_->Open(kFileName2.data(), kFileName2.length());

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, Read) {
  const std::string kFileName = "readfile";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  // Successful reads always return a 3-byte buffer.
  EXPECT_CALL(cdm_file_, Read(_))
      .WillOnce([](mojom::CdmFile::ReadCallback callback) {
        std::move(callback).Run(mojom::CdmFile::Status::kSuccess, {1, 2, 3});
      });
  EXPECT_CALL(*client_.get(), OnReadComplete(Status::kSuccess, _, 3));
  file_io_->Read();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, ReadBeforeOpen) {
  // File not open, so reading should fail.
  EXPECT_CALL(*client_.get(), OnReadComplete(Status::kError, _, _));
  file_io_->Read();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, TwoReads) {
  const std::string kFileName = "readfile";
  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(cdm_file_, Read(_))
      .WillOnce([](mojom::CdmFile::ReadCallback callback) {
        std::move(callback).Run(mojom::CdmFile::Status::kSuccess, {1, 2, 3, 4});
      });
  EXPECT_CALL(*client_.get(), OnReadComplete(Status::kSuccess, _, 4));
  EXPECT_CALL(*client_.get(), OnReadComplete(Status::kInUse, _, 0));
  file_io_->Read();
  file_io_->Read();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, Write) {
  const std::string kFileName = "writefile";
  std::vector<uint8_t> data{1, 2, 3, 4, 5};

  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  // Writing always succeeds.
  EXPECT_CALL(cdm_file_, Write(_, _))
      .WillOnce([](Unused, mojom::CdmFile::WriteCallback callback) {
        std::move(callback).Run(mojom::CdmFile::Status::kSuccess);
      });
  EXPECT_CALL(*client_.get(), OnWriteComplete(Status::kSuccess));
  file_io_->Write(data.data(), data.size());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, WriteBeforeOpen) {
  std::vector<uint8_t> data{1, 2, 3, 4, 5};

  // File not open, so writing should fail.
  EXPECT_CALL(*client_.get(), OnWriteComplete(Status::kError));
  file_io_->Write(data.data(), data.size());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoCdmFileIOTest, TwoWrites) {
  const std::string kFileName = "writefile";
  std::vector<uint8_t> data{1, 2, 3, 4, 5};

  EXPECT_CALL(*client_.get(), OnOpenComplete(Status::kSuccess));
  file_io_->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(cdm_file_, Write(_, _))
      .WillOnce([](Unused, mojom::CdmFile::WriteCallback callback) {
        std::move(callback).Run(mojom::CdmFile::Status::kSuccess);
      });
  EXPECT_CALL(*client_.get(), OnWriteComplete(Status::kSuccess));
  EXPECT_CALL(*client_.get(), OnWriteComplete(Status::kInUse));
  file_io_->Write(data.data(), data.size());
  file_io_->Write(data.data(), data.size());
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
