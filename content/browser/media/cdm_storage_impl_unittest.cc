// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/test/bind.h"
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
#include "third_party/widevine/cdm/buildflags.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

using media::mojom::CdmFile;
using media::mojom::CdmStorage;

namespace content {
using CdmFileId = MediaLicenseManager::CdmFileId;
using CdmFileIdAndContents = MediaLicenseManager::CdmFileIdAndContents;

namespace {

const media::CdmType kTestCdmType{base::Token{1234, 5678}, "test_file_system"};
const media::CdmType kDifferentCdmType{base::Token{8765, 4321},
                                       "different_plugin"};
const media::CdmType kUnrecognizedCdmType{base::Token{1111, 2222}, "imposter"};

const char kTestOrigin[] = "http://www.test.com";

const std::vector<MediaLicenseManager::CdmFileIdAndContents> kDefaultFiles{
    {{"file1", kTestCdmType}, {'r', 'a', 'n', 'd'}},
    {{"file2", kTestCdmType}, {'r', 'a', 'n', 'd', 'o'}},
    {{"file3", kTestCdmType}, {'r', 'a', 'n', 'd', 'o', 'm'}},
};

// Helper functions to manipulate RenderFrameHosts.

void SimulateNavigation(RenderFrameHost** rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, *rfh);
  navigation_simulator->Commit();
  *rfh = navigation_simulator->GetFinalRenderFrameHost();
}

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

  void ResetAndBindToOldBackend(const blink::StorageKey& storage_key,
                                const media::CdmType& cdm_type) {
    cdm_storage_.reset();

    SimulateNavigation(&rfh_, storage_key.origin().GetURL());
    CdmStorageImpl::Create(rfh_, cdm_type,
                           cdm_storage_.BindNewPipeAndPassReceiver());
  }

  void ResetAndBindToNewBackend(const blink::StorageKey& storage_key,
                                const media::CdmType& cdm_type) {
    cdm_storage_.reset();

    SimulateNavigation(&rfh_, storage_key.origin().GetURL());
    media_license_manager()->OpenCdmStorage(
        MediaLicenseManager::BindingContext(storage_key, cdm_type),
        cdm_storage_.BindNewPipeAndPassReceiver());
  }

  MediaLicenseManager* media_license_manager() const {
    auto* media_license_manager =
        static_cast<StoragePartitionImpl*>(rfh_->GetStoragePartition())
            ->GetMediaLicenseManager();
    DCHECK(media_license_manager);
    return media_license_manager;
  }

  RenderFrameHost* rfh_ = nullptr;
  mojo::Remote<CdmStorage> cdm_storage_;
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
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameEmpty) {
  const char kFileName[] = "";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameStartWithUnderscore) {
  const char kFileName[] = "_invalid";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, InvalidFileNameTooLong) {
  // Limit is 256 characters, so try a file name with 257.
  const std::string kFileName(257, 'a');
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_FALSE(Open(kFileName, cdm_file));
  ASSERT_FALSE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, OpenFile) {
  const char kFileName[] = "test_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());
}

TEST_P(CdmStorageTest, OpenFileLocked) {
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

TEST_P(CdmStorageTest, MultipleFiles) {
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

TEST_P(CdmStorageTest, WriteThenReadFile) {
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

TEST_P(CdmStorageTest, ReadThenWriteEmptyFile) {
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

TEST_P(CdmStorageTest, ParallelRead) {
  const char kFileName[] = "duplicate_read_file_name";
  mojo::AssociatedRemote<CdmFile> cdm_file;
  EXPECT_TRUE(Open(kFileName, cdm_file));
  ASSERT_TRUE(cdm_file.is_bound());

  // Attempts to reads the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that 1 read succeeds and the
  // other fails.
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
  ASSERT_TRUE(cdm_file.is_bound());

  // Attempts to write the contents of the previously opened |cdm_file| twice.
  // We don't really care about the data, just that 1 write succeeds and the
  // other fails.
  base::test::TestFuture<CdmFile::Status> future1;
  base::test::TestFuture<CdmFile::Status> future2;

  cdm_file->Write({1, 2, 3}, future1.GetCallback());
  cdm_file->Write({4, 5, 6}, future2.GetCallback());

  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());

  CdmFile::Status status1 = future1.Get();
  CdmFile::Status status2 = future2.Get();

  // One call should succeed, one should fail.
  EXPECT_TRUE((status1 == CdmFile::Status::kSuccess &&
               status2 == CdmFile::Status::kFailure) ||
              (status1 == CdmFile::Status::kFailure &&
               status2 == CdmFile::Status::kSuccess))
      << "status 1: " << status1 << ", status2: " << status2;
}

TEST_P(CdmStorageTest, MigrateDataNone) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);

  // If there's no data to migrate this should run gracefully and without error.
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();
}

TEST_P(CdmStorageTest, MigrateDataBasic) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  // Read data using the new backend.
  ReadFiles(kDefaultFiles);
}

TEST_P(CdmStorageTest, MigrateDataAll) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  // Open a new backend using a different CDM type. The original data should
  // still have been migrated.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kDifferentCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  // Can't read files from another CDM type.
  ExpectFilesEmpty(kDefaultFiles);

  // Files from the original CDM type should exist without another migration.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  ReadFiles(kDefaultFiles);
}

TEST_P(CdmStorageTest, MigrateDataPluginDataDeleted) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  // Read data using the new backend.
  ReadFiles(kDefaultFiles);

  ResetAndBindToOldBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);

  // Data should have been removed from the old backend.
  ExpectFilesEmpty(kDefaultFiles);
}

TEST_P(CdmStorageTest, MigrateDataMultipleCdmTypes) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  // Write data to another CDM type from the same origin.
  const std::vector<MediaLicenseManager::CdmFileIdAndContents> kDifferentFiles{
      {{"other0", kDifferentCdmType}, {'e', 'x', 'a', 'm'}},
      {{"other1", kDifferentCdmType}, {'e', 'x', 'a', 'm', 'p'}},
      {{"other2", kDifferentCdmType}, {'e', 'x', 'a', 'm', 'p', 'l'}},
      {{"other3", kDifferentCdmType}, {'e', 'x', 'a', 'm', 'p', 'l', 'e'}},
  };
  ResetAndBindToOldBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kDifferentCdmType);
  WriteFiles(kDifferentFiles);

  // Ensure files from the first CDM type can be read by the new backend.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  ReadFiles(kDefaultFiles);

  // Open storage for the other CDM type. All media licenses should have been
  // migrated.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kDifferentCdmType);

  ReadFiles(kDifferentFiles);
}

TEST_P(CdmStorageTest, MigrateDataUnrecognizedCdmType) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  // Write data an unrecognized CDM type from the same origin.
  const std::vector<CdmFileIdAndContents> kDifferentFiles{
      {{"other1", kUnrecognizedCdmType}, {'i', 'g', 'n', 'o'}},
      {{"other2", kUnrecognizedCdmType}, {'i', 'g', 'n', 'o', 'r'}},
      {{"other3", kUnrecognizedCdmType}, {'i', 'g', 'n', 'o', 'r', 'e'}},
      {{"other4", kUnrecognizedCdmType}, {'i', 'g', 'n', 'o', 'r', 'e', 'd'}},
  };
  ResetAndBindToOldBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kUnrecognizedCdmType);
  WriteFiles(kDifferentFiles);

  // Read data from the original CDM type using the new backend.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();
  ReadFiles(kDefaultFiles);

  // Open storage for the other CDM type. Media licenses for the unrecognized
  // CDM type should NOT have been migrated.
  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kUnrecognizedCdmType);
  ExpectFilesEmpty(kDifferentFiles);

  // Despite not being migrated, the data should still have been removed from
  // the old backend.
  ResetAndBindToOldBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  ExpectFilesEmpty(kDefaultFiles);
}

TEST_P(CdmStorageTest, MigrateDataMultipleOrigins) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  // Write some data using the old backend.
  WriteFiles(kDefaultFiles);

  cdm_storage_.reset();

  const blink::StorageKey kDifferentStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://www.example.com");
  ResetAndBindToOldBackend(kDifferentStorageKey, kTestCdmType);

  const std::vector<CdmFileIdAndContents> kDifferentFiles{
      {{"dif_file1", kTestCdmType}, {'e', 'x', 'a', 'm'}},
      {{"dif_file2", kTestCdmType}, {'e', 'x', 'a', 'm', 'p'}},
      {{"dif_file3", kTestCdmType}, {'e', 'x', 'a', 'm', 'p', 'l'}},
      {{"dif_file4", kTestCdmType}, {'e', 'x', 'a', 'm', 'p', 'l', 'e'}},
  };
  WriteFiles(kDifferentFiles);

  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin), kTestCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  ReadFiles(kDefaultFiles);

  cdm_storage_.reset();

  // Open storage for the other origin. All media licenses should have been
  // migrated.
  ResetAndBindToNewBackend(kDifferentStorageKey, kTestCdmType);

  ReadFiles(kDifferentFiles);
}

#if BUILDFLAG(ENABLE_WIDEVINE)
TEST_P(CdmStorageTest, MigrateDataWidevine) {
  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // No data to migrate if the flag is enabled.
    return;
  }

  ResetAndBindToOldBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kWidevineCdmType);

  const std::vector<CdmFileIdAndContents> kWidevineFiles{
      {{"wide_file1", kWidevineCdmType}, {'e', 'x', 'a', 'm'}},
      {{"wide_file2", kWidevineCdmType}, {'e', 'x', 'a', 'm', 'p'}},
      {{"wide_file3", kWidevineCdmType}, {'e', 'x', 'a', 'm', 'p', 'l'}},
      {{"wide_file4", kWidevineCdmType}, {'e', 'x', 'a', 'm', 'p', 'l', 'e'}},
  };

  // Write some Widevine data using the old backend.
  WriteFiles(kWidevineFiles);

  ResetAndBindToNewBackend(
      blink::StorageKey::CreateFromStringForTesting(kTestOrigin),
      kWidevineCdmType);
  base::RunLoop loop;
  media_license_manager()->MigrateMediaLicensesForTesting(loop.QuitClosure());
  loop.Run();

  // Read data using the new backend.
  ReadFiles(kWidevineFiles);
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

}  // namespace content
