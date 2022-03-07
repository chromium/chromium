// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "content/browser/media/media_license_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
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
#include "url/origin.h"

using media::mojom::CdmFile;
using media::mojom::CdmStorage;

namespace content {

namespace {

const media::CdmType kTestCdmType{base::Token{1234, 5678}, "test_file_system"};
const char kTestOrigin[] = "http://www.test.com";

// Helper functions to manipulate RenderFrameHosts.

void SimulateNavigation(RenderFrameHost** rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, *rfh);
  navigation_simulator->Commit();
  *rfh = navigation_simulator->GetFinalRenderFrameHost();
}

// Helper that wraps a base::RunLoop and only quits the RunLoop
// if the expected number of quit calls have happened.
class RunLoopWithExpectedCount {
 public:
  RunLoopWithExpectedCount() = default;

  RunLoopWithExpectedCount(const RunLoopWithExpectedCount&) = delete;
  RunLoopWithExpectedCount& operator=(const RunLoopWithExpectedCount&) = delete;

  ~RunLoopWithExpectedCount() { DCHECK_EQ(0, remaining_quit_calls_); }

  void Run(int expected_quit_calls) {
    DCHECK_GT(expected_quit_calls, 0);
    DCHECK_EQ(remaining_quit_calls_, 0);
    remaining_quit_calls_ = expected_quit_calls;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void Quit() {
    if (--remaining_quit_calls_ > 0)
      return;
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  int remaining_quit_calls_ = 0;
};

}  // namespace

class CdmStorageTest : public base::test::WithFeatureOverride,
                       public RenderViewHostTestHarness {
 public:
  CdmStorageTest()
      : base::test::WithFeatureOverride(features::kMediaLicenseBackend),
        RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    rfh_ = web_contents()->GetMainFrame();
    RenderFrameHostTester::For(rfh_)->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&rfh_, GURL(kTestOrigin));

    if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
      // Create the CdmStorageImpl object.
      auto* media_license_manager =
          static_cast<StoragePartitionImpl*>(rfh_->GetStoragePartition())
              ->GetMediaLicenseManager();
      DCHECK(media_license_manager);
      media_license_manager->OpenCdmStorage(
          MediaLicenseManager::BindingContext(
              blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
              kTestCdmType),
          cdm_storage_.BindNewPipeAndPassReceiver());
    } else {
      // Create the CdmStorageImpl object. |cdm_storage_| will own the resulting
      // object.
      CdmStorageImpl::Create(rfh_, kTestCdmType,
                             cdm_storage_.BindNewPipeAndPassReceiver());
    }
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

  // Attempts to reads the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that 1 read succeeds and the
  // other fails.
  void ReadTwice(CdmFile* cdm_file,
                 CdmFile::Status* status1,
                 CdmFile::Status* status2) {
    DVLOG(3) << __func__;
    std::vector<uint8_t> data1;
    std::vector<uint8_t> data2;

    cdm_file->Read(base::BindOnce(&CdmStorageTest::FileRead,
                                  base::Unretained(this), status1, &data1));
    cdm_file->Read(base::BindOnce(&CdmStorageTest::FileRead,
                                  base::Unretained(this), status2, &data2));
    RunAndWaitForResult(2);
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

  // Attempts to write the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that 1 read succeeds and the
  // other fails.
  void WriteTwice(CdmFile* cdm_file,
                  CdmFile::Status* status1,
                  CdmFile::Status* status2) {
    DVLOG(3) << __func__;

    cdm_file->Write({1, 2, 3}, base::BindOnce(&CdmStorageTest::FileWritten,
                                              base::Unretained(this), status1));
    cdm_file->Write({4, 5, 6}, base::BindOnce(&CdmStorageTest::FileWritten,
                                              base::Unretained(this), status2));
    RunAndWaitForResult(2);
  }

 private:
  void FileRead(CdmFile::Status* status,
                std::vector<uint8_t>* data,
                CdmFile::Status actual_status,
                const std::vector<uint8_t>& actual_data) {
    DVLOG(3) << __func__;
    *status = actual_status;
    *data = actual_data;
    run_loop_with_count_->Quit();
  }

  void FileWritten(CdmFile::Status* status, CdmFile::Status actual_status) {
    DVLOG(3) << __func__;
    *status = actual_status;
    run_loop_with_count_->Quit();
  }

  // Start running and allow the asynchronous IO operations to complete.
  void RunAndWaitForResult(int expected_quit_calls) {
    run_loop_with_count_ = std::make_unique<RunLoopWithExpectedCount>();
    run_loop_with_count_->Run(expected_quit_calls);
  }

  RenderFrameHost* rfh_ = nullptr;
  mojo::Remote<CdmStorage> cdm_storage_;
  std::unique_ptr<RunLoopWithExpectedCount> run_loop_with_count_;
};

// TODO(crbug.com/1231162): Make this a non-parameterized test suite once we no
// longer have to test against both backends.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(CdmStorageTest);

TEST_P(CdmStorageTest, InvalidFileName) {
  // Anything other than ASCII letter, digits, and -._ will fail. Add a
  // Unicode character to the name.
  const char kFileName[] = "openfile\u1234";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameEmpty) {
  const char kFileName[] = "";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameStartWithUnderscore) {
  const char kFileName[] = "_invalid";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameTooLong) {
  // Limit is 256 characters, so try a file name with 257.
  const std::string kFileName(257, 'a');
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  EXPECT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, OpenFile) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, OpenFileLocked) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file1;
  EXPECT_TRUE(Open(kFileName, cdm_file1));
  EXPECT_TRUE(cdm_file1.is_bound());

  // Second attempt on the same file should fail as the file is locked.
  mojo::AssociatedRemote<CdmFile> cdm_file2;
  EXPECT_FALSE(Open(kFileName, cdm_file2));
  EXPECT_FALSE(cdm_file2.is_bound());

  // Now close the first file and try again. It should be free now.
  cdm_file1.reset();

  mojo::AssociatedRemote<CdmFile> cdm_file3;
  EXPECT_TRUE(Open(kFileName, cdm_file3));
  EXPECT_TRUE(cdm_file3.is_bound());
}

TEST_P(CdmStorageTest, MultipleFiles) {
  const char kFileName1[] = "file1";
  mojo::AssociatedRemote<CdmFile> cdm_file1;
  EXPECT_TRUE(Open(kFileName1, cdm_file1));
  EXPECT_TRUE(cdm_file1.is_bound());

  const char kFileName2[] = "file2";
  mojo::AssociatedRemote<CdmFile> cdm_file2;
  EXPECT_TRUE(Open(kFileName2, cdm_file2));
  EXPECT_TRUE(cdm_file2.is_bound());

  const char kFileName3[] = "file3";
  mojo::AssociatedRemote<CdmFile> cdm_file3;
  EXPECT_TRUE(Open(kFileName3, cdm_file3));
  EXPECT_TRUE(cdm_file3.is_bound());
}

TEST_P(CdmStorageTest, WriteThenReadFile) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

  // Write several bytes and read them back.
  std::vector<uint8_t> kTestData = {'r', 'a', 'n', 'd', 'o', 'm'};
  EXPECT_TRUE(Write(cdm_file.get(), kTestData));

  std::vector<uint8_t> data_read;
  EXPECT_TRUE(Read(cdm_file.get(), data_read));
  EXPECT_EQ(kTestData, data_read);
}

TEST_P(CdmStorageTest, ReadThenWriteEmptyFile) {
  const char kFileName[] = "empty_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

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

TEST_P(CdmStorageTest, ParallelRead) {
  const char kFileName[] = "duplicate_read_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

  CdmFile::Status status1;
  CdmFile::Status status2;
  ReadTwice(cdm_file.get(), &status1, &status2);

  // One call should succeed, one should fail.
  EXPECT_TRUE((status1 == CdmFile::Status::kSuccess &&
               status2 == CdmFile::Status::kFailure) ||
              (status1 == CdmFile::Status::kFailure &&
               status2 == CdmFile::Status::kSuccess))
      << "status 1: " << status1 << ", status2: " << status2;
}

TEST_P(CdmStorageTest, ParallelWrite) {
  const char kFileName[] = "duplicate_write_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  EXPECT_TRUE(cdm_file.is_bound());

  CdmFile::Status status1;
  CdmFile::Status status2;
  WriteTwice(cdm_file.get(), &status1, &status2);

  // One call should succeed, one should fail.
  EXPECT_TRUE((status1 == CdmFile::Status::kSuccess &&
               status2 == CdmFile::Status::kFailure) ||
              (status1 == CdmFile::Status::kFailure &&
               status2 == CdmFile::Status::kSuccess))
      << "status 1: " << status1 << ", status2: " << status2;
}

}  // namespace content
