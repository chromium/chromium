// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/media/media_license_manager.h"

#include <string_view>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/browser/media/media_license_quota_client.h"
#include "content/public/browser/storage_partition.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom-forward.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

const media::CdmType kCdmType{1234, 5678};

const char kExampleOrigin[] = "https://example.com";

}  // namespace

class MediaLicenseManagerTest : public testing::Test {
 public:
  MediaLicenseManagerTest() : in_memory_(false) {}
  explicit MediaLicenseManagerTest(bool in_memory) : in_memory_(in_memory) {}

  void SetUp() override {
    ASSERT_TRUE(profile_path_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        in_memory_, in_memory_ ? base::FilePath() : profile_path_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*special storage policy=*/nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        static_cast<storage::MockQuotaManager*>(quota_manager_.get()),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    manager_ = std::make_unique<MediaLicenseManager>(
        in_memory_,
        /*special storage policy=*/nullptr, quota_manager_proxy_);
  }

  void TearDown() override {
    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(profile_path_.Delete());
  }

  // Hard-coded to the default bucket, since this API should never be used in
  // non-default buckets anyways.
  storage::QuotaErrorOr<storage::BucketLocator> GetOrCreateBucket(
      const blink::StorageKey& storage_key) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  mojo::AssociatedRemote<media::mojom::CdmFile> OpenCdmFile(
      const mojo::Remote<media::mojom::CdmStorage>& storage,
      const std::string& file_name) {
    mojo::AssociatedRemote<media::mojom::CdmFile> cdm_file;

    base::test::TestFuture<media::mojom::CdmStorage::Status,
                           mojo::PendingAssociatedRemote<media::mojom::CdmFile>>
        open_future;
    storage->Open(file_name, open_future.GetCallback());

    auto result = open_future.Take();
    EXPECT_EQ(std::get<0>(result), media::mojom::CdmStorage::Status::kSuccess);
    cdm_file.Bind(std::move(std::get<1>(result)));
    return cdm_file;
  }

  void Write(const mojo::AssociatedRemote<media::mojom::CdmFile>& cdm_file,
             const std::string& data) {
    base::test::TestFuture<media::mojom::CdmFile::Status> write_future;
    cdm_file->Write(
        std::vector<uint8_t>(data.data(), data.data() + data.size()),
        write_future.GetCallback());
    EXPECT_EQ(write_future.Get(), media::mojom::CdmFile::Status::kSuccess);
  }

  // Reads the previously opened `cdm_file` and check that its contents match
  // `expected_data`.
  void ExpectFileContents(
      const mojo::AssociatedRemote<media::mojom::CdmFile>& cdm_file,
      std::string_view expected_data) {
    base::test::TestFuture<media::mojom::CdmFile::Status, std::vector<uint8_t>>
        future;
    cdm_file->Read(future.GetCallback<media::mojom::CdmFile::Status,
                                      const std::vector<uint8_t>&>());

    media::mojom::CdmFile::Status status = future.Get<0>();
    auto data = future.Get<1>();
    EXPECT_EQ(status, media::mojom::CdmFile::Status::kSuccess);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(data.data()),
                               data.size()),
              expected_data);
  }

  base::FilePath FindMediaLicenseDatabase() {
    base::FileEnumerator file_enumerator(profile_path_.GetPath(),
                                         /*recursive=*/true,
                                         base::FileEnumerator::FILES);

    base::FilePath file;
    while (!(file = file_enumerator.Next()).empty()) {
      if (file.BaseName().value() == storage::kMediaLicenseDatabaseFileName) {
        return file;
      }
    }
    return base::FilePath();
  }

 protected:
  const bool in_memory_;

  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // This must be above MediaLicenseManager, to ensure that no file is accessed
  // when the temporary directory is deleted.
  base::ScopedTempDir profile_path_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<MediaLicenseManager> manager_;
};

TEST_F(MediaLicenseManagerTest, DeleteBucketData) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  // Write some data.
  Write(cdm_file, kTestData);

  auto database_file = FindMediaLicenseDatabase();

  EXPECT_FALSE(database_file.empty());

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);

  // Confirm that the database was deleted, but the Media License directory was
  // not.
  EXPECT_FALSE(base::PathExists(database_file));
  EXPECT_TRUE(base::DirectoryExists(database_file.DirName()));

  // Confirm that the file is now empty.
  ExpectFileContents(cdm_file, "");
}

TEST_F(MediaLicenseManagerTest, DeleteBucketDataClosedStorage) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  Write(cdm_file, kTestData);

  auto database_file = FindMediaLicenseDatabase();
  EXPECT_FALSE(database_file.empty());

  // We should still be able to wipe data to a closed storage.
  cdm_file.reset();
  remote.reset();

  EXPECT_TRUE(base::PathExists(database_file));

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);

  // Confirm that the database was deleted, but the Media License
  // directory was not.
  EXPECT_FALSE(base::PathExists(database_file));
  EXPECT_TRUE(base::DirectoryExists(database_file.DirName()));
}

TEST_F(MediaLicenseManagerTest, DeleteBucketDataOpenConnection) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  Write(cdm_file, kTestData);

  auto database_file = FindMediaLicenseDatabase();
  EXPECT_FALSE(database_file.empty());

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);

  // Confirm that the database was deleted, but the Media License directory was
  // not.
  EXPECT_FALSE(base::PathExists(database_file));
  EXPECT_TRUE(base::DirectoryExists(database_file.DirName()));

  // Confirm that the file is now empty.
  ExpectFileContents(cdm_file, "");

  // Write some more data. This should succeed.
  Write(cdm_file, kTestData);

  EXPECT_TRUE(base::PathExists(database_file));
  EXPECT_TRUE(base::PathExists(database_file.DirName()));
}

TEST_F(MediaLicenseManagerTest, BucketCreationFailed) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Disable the quota database, causing GetOrCreateBucket() to fail.
  quota_manager_->SetDisableDatabase(/*disable=*/true);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  // Opening a CDM file should fail.
  base::test::TestFuture<media::mojom::CdmStorage::Status,
                         mojo::PendingAssociatedRemote<media::mojom::CdmFile>>
      open_future;
  remote->Open("test_file", open_future.GetCallback());

  auto result = open_future.Take();
  EXPECT_EQ(std::get<0>(result), media::mojom::CdmStorage::Status::kFailure);
  EXPECT_FALSE(std::get<1>(result).is_valid());
}

class MediaLicenseManagerIncognitoTest : public MediaLicenseManagerTest {
 public:
  MediaLicenseManagerIncognitoTest()
      : MediaLicenseManagerTest(/*in_memory=*/true) {}
};

TEST_F(MediaLicenseManagerIncognitoTest, DeleteBucketData) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  // Write some data.
  Write(cdm_file, kTestData);

  // We should be able to read the written file.
  ExpectFileContents(cdm_file, kTestData);

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);

  // Confirm that the file is now empty.
  ExpectFileContents(cdm_file, "");
}

TEST_F(MediaLicenseManagerIncognitoTest, DeleteBucketDataClosedStorage) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  Write(cdm_file, kTestData);

  // There should be no db_file since its in memory.
  EXPECT_TRUE(FindMediaLicenseDatabase().empty());

  // We should still be able to wipe data to a closed storage.
  cdm_file.reset();
  remote.reset();

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);
}

TEST_F(MediaLicenseManagerIncognitoTest, DeleteBucketDataOpenConnection) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(remote, "test_file");

  Write(cdm_file, kTestData);

  ExpectFileContents(cdm_file, kTestData);

  // There should be no db file since its in memory.
  EXPECT_TRUE(FindMediaLicenseDatabase().empty());

  // Delete data for this storage key.
  base::test::TestFuture<blink::mojom::QuotaStatusCode> delete_future;
  manager_->DeleteBucketData(bucket, delete_future.GetCallback());
  EXPECT_EQ(delete_future.Get(), blink::mojom::QuotaStatusCode::kOk);

  // Confirm that the file is now empty.
  ExpectFileContents(cdm_file, "");

  // Write some more data. This should succeed.
  Write(cdm_file, kTestData);

  // Check that no file was created.
  EXPECT_TRUE(FindMediaLicenseDatabase().empty());
}

TEST_F(MediaLicenseManagerIncognitoTest, BucketCreationFailed) {
  const std::string kTestData("Test Data");
  mojo::Remote<media::mojom::CdmStorage> remote;
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(kExampleOrigin);
  ASSERT_OK_AND_ASSIGN(storage::BucketLocator bucket,
                       GetOrCreateBucket(storage_key));
  CdmStorageBindingContext binding_context(storage_key, kCdmType);

  // Disable the quota database, causing GetOrCreateBucket() to fail.
  quota_manager_->SetDisableDatabase(/*disable=*/true);

  // Open CDM storage for a storage key.
  manager_->OpenCdmStorage(binding_context,
                           remote.BindNewPipeAndPassReceiver());
  // Opening a CDM file should fail.
  base::test::TestFuture<media::mojom::CdmStorage::Status,
                         mojo::PendingAssociatedRemote<media::mojom::CdmFile>>
      open_future;
  remote->Open("test_file", open_future.GetCallback());

  auto result = open_future.Take();
  EXPECT_EQ(std::get<0>(result), media::mojom::CdmStorage::Status::kFailure);
  EXPECT_FALSE(std::get<1>(result).is_valid());
}

}  // namespace content
