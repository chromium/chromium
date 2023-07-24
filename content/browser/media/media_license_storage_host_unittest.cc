// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/media_license_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

using media::mojom::CdmFile;
using media::mojom::CdmStorage;

namespace content {

namespace {

const media::CdmType kTestCdmType{1234, 5678};

const char kTestOrigin[] = "http://www.test.com";

// Helper functions to manipulate RenderFrameHosts.

void SimulateNavigation(RenderFrameHost** rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, *rfh);
  navigation_simulator->Commit();
  *rfh = navigation_simulator->GetFinalRenderFrameHost();
}

}  // namespace

class CdmStorageTest : public RenderViewHostTestHarness {
 public:
  CdmStorageTest()
      : RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    rfh_ = web_contents()->GetPrimaryMainFrame();
    RenderFrameHostTester::For(rfh_)->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&rfh_, GURL(kTestOrigin));

    auto* media_license_manager =
        static_cast<StoragePartitionImpl*>(rfh_->GetStoragePartition())
            ->GetMediaLicenseManager();
    DCHECK(media_license_manager);
    media_license_manager->OpenCdmStorage(
        CdmStorageBindingContext(
            blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
            kTestCdmType),
        cdm_storage_.BindNewPipeAndPassReceiver());
  }

  // Open the file |name|. Returns true if the file returned is valid, false
  // otherwise. On success |cdm_file| is bound to the CdmFileImpl object.
  bool Open(const std::string& name,
            mojo::AssociatedRemote<CdmFile>& cdm_file) {
    DVLOG(3) << __func__;

    base::test::TestFuture<CdmStorage::Status,
                           mojo::PendingAssociatedRemote<CdmFile>>
        future;
    cdm_storage_->Open(name, future.GetCallback());

    CdmStorage::Status status = future.Get<0>();
    mojo::PendingAssociatedRemote<CdmFile> actual_file =
        std::move(std::get<1>(future.Take()));
    if (!actual_file) {
      DCHECK_NE(status, CdmStorage::Status::kSuccess);
      return false;
    }

    // Open() returns a mojo::PendingAssociatedRemote<CdmFile>, so bind it to
    // the mojo::AssociatedRemote<CdmFileAssociated> provided.
    mojo::AssociatedRemote<CdmFile> cdm_file_remote;
    cdm_file_remote.Bind(std::move(actual_file));
    cdm_file = std::move(cdm_file_remote);

    return status == CdmStorage::Status::kSuccess;
  }

  // Reads the contents of the previously opened |cdm_file|. If successful,
  // true is returned and |data| is updated with the contents of the file.
  // If unable to read the file, false is returned.
  bool Read(CdmFile* cdm_file, std::vector<uint8_t>& data) {
    DVLOG(3) << __func__;

    base::test::TestFuture<CdmFile::Status, std::vector<uint8_t>> future;
    cdm_file->Read(
        future.GetCallback<CdmFile::Status, const std::vector<uint8_t>&>());

    CdmFile::Status status = future.Get<0>();
    data = future.Get<1>();
    return status == CdmFile::Status::kSuccess;
  }

  // Writes |data| to the previously opened |cdm_file|, replacing the contents
  // of the file. Returns true if successful, false otherwise.
  bool Write(CdmFile* cdm_file, const std::vector<uint8_t>& data) {
    DVLOG(3) << __func__;

    base::test::TestFuture<CdmFile::Status> future;
    cdm_file->Write(data, future.GetCallback());

    CdmFile::Status status = future.Get();
    return status == CdmFile::Status::kSuccess;
  }

  void WriteFiles(const std::vector<CdmFileIdAndContents>& files) {
    // Write some data using the old backend.
    for (const auto& file : files) {
      mojo::AssociatedRemote<CdmFile> remote;
      EXPECT_TRUE(Open(file.file.name, remote));
      ASSERT_TRUE(remote.is_bound());
      EXPECT_TRUE(Write(remote.get(), file.data));
    }
  }

  void ReadFiles(const std::vector<CdmFileIdAndContents>& files) {
    for (const auto& file : files) {
      mojo::AssociatedRemote<CdmFile> remote;
      EXPECT_TRUE(Open(file.file.name, remote));
      ASSERT_TRUE(remote.is_bound());
      std::vector<uint8_t> data_read;
      EXPECT_TRUE(Read(remote.get(), data_read));
      EXPECT_EQ(file.data, data_read);
    }
  }

  void ExpectFilesEmpty(const std::vector<CdmFileIdAndContents>& files) {
    for (const auto& file : files) {
      mojo::AssociatedRemote<CdmFile> remote;
      EXPECT_TRUE(Open(file.file.name, remote));
      ASSERT_TRUE(remote.is_bound());
      std::vector<uint8_t> data_read;
      EXPECT_TRUE(Read(remote.get(), data_read));
      EXPECT_TRUE(data_read.empty());
    }
  }

  MediaLicenseManager* media_license_manager() const {
    auto* media_license_manager =
        static_cast<StoragePartitionImpl*>(rfh_->GetStoragePartition())
            ->GetMediaLicenseManager();
    DCHECK(media_license_manager);
    return media_license_manager;
  }

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION RenderFrameHost* rfh_ = nullptr;
  mojo::Remote<CdmStorage> cdm_storage_;
};

TEST_F(CdmStorageTest, InvalidFileName) {
  // Anything other than ASCII letter, digits, and -._ will fail. Add a
  // Unicode character to the name.
  const char kFileName[] = "openfile\u1234";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameEmpty) {
  const char kFileName[] = "";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameStartWithUnderscore) {
  const char kFileName[] = "_invalid";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameTooLong) {
  // Limit is 256 characters, so try a file name with 257.
  const std::string kFileName(257, 'a');
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, OpenFile) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());
}

TEST_F(CdmStorageTest, OpenFileLocked) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file1;
  EXPECT_TRUE(Open(kFileName, cdm_file1));
  ASSERT_TRUE(cdm_file1.is_bound());

  // Second attempt on the same file should fail as the file is locked.
  mojo::AssociatedRemote<CdmFile> cdm_file2;
  EXPECT_FALSE(Open(kFileName, cdm_file2));
  ASSERT_FALSE(cdm_file2.is_bound());

  // Now close the first file and try again. It should be free now.
  cdm_file1.reset();

  mojo::AssociatedRemote<CdmFile> cdm_file3;
  EXPECT_TRUE(Open(kFileName, cdm_file3));
  ASSERT_TRUE(cdm_file3.is_bound());
}

TEST_F(CdmStorageTest, MultipleFiles) {
  const char kFileName1[] = "file1";
  mojo::AssociatedRemote<CdmFile> cdm_file1;
  EXPECT_TRUE(Open(kFileName1, cdm_file1));
  ASSERT_TRUE(cdm_file1.is_bound());

  const char kFileName2[] = "file2";
  mojo::AssociatedRemote<CdmFile> cdm_file2;
  EXPECT_TRUE(Open(kFileName2, cdm_file2));
  ASSERT_TRUE(cdm_file2.is_bound());

  const char kFileName3[] = "file3";
  mojo::AssociatedRemote<CdmFile> cdm_file3;
  EXPECT_TRUE(Open(kFileName3, cdm_file3));
  ASSERT_TRUE(cdm_file3.is_bound());
}

TEST_F(CdmStorageTest, WriteThenReadFile) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());

  // Write several bytes and read them back.
  std::vector<uint8_t> kTestData = {'r', 'a', 'n', 'd', 'o', 'm'};
  EXPECT_TRUE(Write(cdm_file.get(), kTestData));

  std::vector<uint8_t> data_read;
  EXPECT_TRUE(Read(cdm_file.get(), data_read));
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(CdmStorageTest, ReadThenWriteEmptyFile) {
  const char kFileName[] = "empty_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());

  // New file should be empty.
  std::vector<uint8_t> data_read;
  EXPECT_TRUE(Read(cdm_file.get(), data_read));
  EXPECT_EQ(0u, data_read.size());

  // Write nothing.
  EXPECT_TRUE(Write(cdm_file.get(), std::vector<uint8_t>()));

  // Should still be empty.
  EXPECT_TRUE(Read(cdm_file.get(), data_read));
  EXPECT_EQ(0u, data_read.size());
}

TEST_F(CdmStorageTest, ParallelRead) {
  const char kFileName[] = "duplicate_read_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());

  // Attempts to reads the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that the first read succeeds.
  base::test::TestFuture<CdmFile::Status, std::vector<uint8_t>> future1;
  base::test::TestFuture<CdmFile::Status, std::vector<uint8_t>> future2;

  cdm_file->Read(
      future1.GetCallback<CdmFile::Status, const std::vector<uint8_t>&>());
  cdm_file->Read(
      future2.GetCallback<CdmFile::Status, const std::vector<uint8_t>&>());

  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());

  CdmFile::Status status1 = future1.Get<0>();
  CdmFile::Status status2 = future2.Get<0>();

  // The first call should succeed. The second call might fail, if its blocked
  // by our locking system, or might succeed, if the reads are processed faster
  // than expected.
  EXPECT_TRUE(status1 == CdmFile::Status::kSuccess &&
              (status2 == CdmFile::Status::kFailure ||
               status2 == CdmFile::Status::kSuccess))
      << "status 1: " << status1 << ", status2: " << status2;
}

TEST_F(CdmStorageTest, ParallelWrite) {
  const char kFileName[] = "duplicate_write_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());

  // Attempts to write the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that the first write succeeds.
  base::test::TestFuture<CdmFile::Status> future1;
  base::test::TestFuture<CdmFile::Status> future2;

  cdm_file->Write({1, 2, 3}, future1.GetCallback());
  cdm_file->Write({4, 5, 6}, future2.GetCallback());

  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());

  CdmFile::Status status1 = future1.Get();
  CdmFile::Status status2 = future2.Get();

  // The first call should succeed. The second call might fail, if its blocked
  // by our locking system, or might succeed, if the writes are processed
  // faster than expected.
  EXPECT_TRUE(status1 == CdmFile::Status::kSuccess &&
              (status2 == CdmFile::Status::kSuccess ||
               status2 == CdmFile::Status::kFailure))
      << "status 1: " << status1 << ", status2: " << status2;
}

}  // namespace content
