// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_manager.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::mojom::CdmFile;
using media::mojom::CdmStorage;

namespace content {

namespace {

const media::CdmType kCdmType{1234, 5678};

const char kFileName[] = "file.txt";
const char kFileNameTwo[] = "file2.txt";

const std::vector<uint8_t> kPopulatedFileValue = {1, 2, 3};
const std::vector<uint8_t> kPopulatedFileValueTwo = {1, 2, 3, 4};

std::vector<uint8_t> kEmptyFileValue;

}  // namespace

// TODO(crbug.com/40272342): Add a DeleteFile test once there is a way to check
// that the db is actually deleted.

class CdmStorageManagerTest : public testing::Test {
 public:
  CdmStorageManagerTest() = default;
  ~CdmStorageManagerTest() override = default;

 protected:
  void SetUpManager(const base::FilePath file_path) {
    cdm_storage_manager_ = std::make_unique<CdmStorageManager>(file_path);
  }

  void SetUpManagerTwo(const base::FilePath file_path) {
    cdm_storage_manager_two_ = std::make_unique<CdmStorageManager>(file_path);
  }

  mojo::AssociatedRemote<media::mojom::CdmFile> OpenCdmFile(
      const mojo::Remote<media::mojom::CdmStorage>& storage,
      const std::string& file_name) {
    base::test::TestFuture<media::mojom::CdmStorage::Status,
                           mojo::PendingAssociatedRemote<media::mojom::CdmFile>>
        open_future;
    storage->Open(file_name, open_future.GetCallback());

    auto [status, pending_remote] = open_future.Take();
    EXPECT_EQ(status, media::mojom::CdmStorage::Status::kSuccess);

    mojo::AssociatedRemote<media::mojom::CdmFile> cdm_file;
    cdm_file.Bind(std::move(pending_remote));
    return cdm_file;
  }

  void Write(const mojo::AssociatedRemote<media::mojom::CdmFile>& cdm_file,
             const std::vector<uint8_t> data) {
    base::test::TestFuture<media::mojom::CdmFile::Status> write_future;
    cdm_file->Write(data, write_future.GetCallback());
    EXPECT_EQ(write_future.Get(), media::mojom::CdmFile::Status::kSuccess);
  }

  // Reads the previously opened `cdm_file` and check that its contents match
  // `expected_data`.
  void ExpectFileContents(
      const mojo::AssociatedRemote<media::mojom::CdmFile>& cdm_file,
      const std::vector<uint8_t> expected_data) {
    base::test::TestFuture<media::mojom::CdmFile::Status, std::vector<uint8_t>>
        future;
    cdm_file->Read(future.GetCallback<media::mojom::CdmFile::Status,
                                      const std::vector<uint8_t>&>());

    auto [status, data] = future.Take();
    EXPECT_EQ(status, media::mojom::CdmFile::Status::kSuccess);
    EXPECT_EQ(data, expected_data);
  }

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/");
  const blink::StorageKey kTestStorageKeyTwo =
      blink::StorageKey::CreateFromStringForTesting("https://exampletwo.com/");

  std::unique_ptr<CdmStorageManager> cdm_storage_manager_;
  std::unique_ptr<CdmStorageManager> cdm_storage_manager_two_;

  mojo::Remote<CdmStorage> cdm_storage_;
  mojo::Remote<CdmStorage> cdm_storage_two_;

  base::test::TaskEnvironment task_environment_;
};

class CdmStorageManagerSingularTest : public CdmStorageManagerTest,
                                      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    bool is_incognito = GetParam();
    if (is_incognito) {
      SetUpManager(base::FilePath());
    } else {
      SetUpManager(base::CreateUniqueTempDirectoryScopedToTest().Append(
          kCdmStorageDatabaseFileName));
    }
  }

  void TearDown() override {
    cdm_storage_manager_->DeleteData(base::NullCallback(), blink::StorageKey(),
                                     base::Time::Min(), base::Time::Max(),
                                     base::DoNothing());

    // To prevent a memory leak, reset the manager. This may post
    // destruction of other objects, so RunUntilIdle().
    cdm_storage_manager_.reset();
    task_environment_.RunUntilIdle();
  }
};

class CdmStorageManagerMultipleTest
    : public CdmStorageManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    if (std::get<0>(GetParam())) {
      SetUpManager(base::FilePath());
    } else {
      SetUpManager(base::CreateUniqueTempDirectoryScopedToTest().Append(
          kCdmStorageDatabaseFileName));
    }

    if (std::get<1>(GetParam())) {
      SetUpManagerTwo(base::FilePath());
    } else {
      SetUpManagerTwo(base::CreateUniqueTempDirectoryScopedToTest().Append(
          kCdmStorageDatabaseFileName));
    }
  }

  void TearDown() override {
    cdm_storage_manager_->DeleteData(base::NullCallback(), blink::StorageKey(),
                                     base::Time::Min(), base::Time::Max(),
                                     base::DoNothing());
    cdm_storage_manager_two_->DeleteData(base::NullCallback(),
                                         blink::StorageKey(), base::Time::Min(),
                                         base::Time::Max(), base::DoNothing());

    // To prevent a memory leak, reset the manager. This may post
    // destruction of other objects, so RunUntilIdle().
    cdm_storage_manager_.reset();
    cdm_storage_manager_two_.reset();
    task_environment_.RunUntilIdle();
  }
};

// This test case tests deletion of a file.
TEST_P(CdmStorageManagerSingularTest, DeleteFile) {
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(cdm_storage_, kFileName);
  ASSERT_TRUE(cdm_file.is_bound());

  Write(cdm_file, kPopulatedFileValue);

  ExpectFileContents(cdm_file, kPopulatedFileValue);

  cdm_storage_manager_->DeleteFile(kTestStorageKey, kCdmType, kFileName,
                                   base::DoNothing());

  ExpectFileContents(cdm_file, kEmptyFileValue);
}

// This test case tests deletion of a non existent file to make sure it doesn't
// affect any other files in the database.
TEST_P(CdmStorageManagerSingularTest, DeleteNonExistentFile) {
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(cdm_storage_, kFileName);
  ASSERT_TRUE(cdm_file.is_bound());

  Write(cdm_file, kPopulatedFileValue);

  ExpectFileContents(cdm_file, kPopulatedFileValue);

  cdm_storage_manager_->DeleteFile(kTestStorageKey, kCdmType, kFileNameTwo,
                                   base::DoNothing());

  ExpectFileContents(cdm_file, kPopulatedFileValue);
}

// This test cases tests that we accurately delete data for a storage key
// without touching other data in the database.
TEST_P(CdmStorageManagerSingularTest, DeleteDataForStorageKey) {
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_one = OpenCdmFile(cdm_storage_, kFileName);
  Write(cdm_file_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_one.is_bound());

  auto cdm_file_two_for_remote_one = OpenCdmFile(cdm_storage_, kFileNameTwo);
  Write(cdm_file_two_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_two_for_remote_one.is_bound());

  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKeyTwo, kCdmType),
      cdm_storage_two_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_two = OpenCdmFile(cdm_storage_two_, kFileNameTwo);
  Write(cdm_file_for_remote_two, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_two.is_bound());

  cdm_storage_manager_->DeleteData(base::NullCallback(), kTestStorageKey,
                                   base::Time::Min(), base::Time::Max(),
                                   base::DoNothing());

  ExpectFileContents(cdm_file_for_remote_one, kEmptyFileValue);
  ExpectFileContents(cdm_file_two_for_remote_one, kEmptyFileValue);

  ExpectFileContents(cdm_file_for_remote_two, kPopulatedFileValue);
}

TEST_P(CdmStorageManagerSingularTest, DeleteFileNoDatabase) {
  cdm_storage_manager_->DeleteFile(kTestStorageKey, kCdmType, kFileNameTwo,
                                   base::DoNothing());
}

// This test case tests when there is a file written to the CdmStorageDatabase
// from two different profiles with the same file name, cdm type, and storage
// key. We want to make sure that although they share the same qualities,
// deleting the file from one of the profiles does not affect the other profile.
TEST_P(CdmStorageManagerMultipleTest, DeleteFile) {
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());
  auto cdm_file = OpenCdmFile(cdm_storage_, kFileName);
  ASSERT_TRUE(cdm_file.is_bound());

  Write(cdm_file, kPopulatedFileValue);

  ExpectFileContents(cdm_file, kPopulatedFileValue);

  cdm_storage_manager_two_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_two_.BindNewPipeAndPassReceiver());
  auto cdm_file_two = OpenCdmFile(cdm_storage_two_, kFileName);
  ASSERT_TRUE(cdm_file_two.is_bound());

  Write(cdm_file_two, kPopulatedFileValue);

  ExpectFileContents(cdm_file_two, kPopulatedFileValue);

  cdm_storage_manager_->DeleteFile(kTestStorageKey, kCdmType, kFileName,
                                   base::DoNothing());

  ExpectFileContents(cdm_file, kEmptyFileValue);
  ExpectFileContents(cdm_file_two, kPopulatedFileValue);
}

// This tests deletion of multiple files under the same storage key. Since the
// files are written from multiple profiles with the same storage key, we verify
// that deletion from one profile does not affect the files written to the same
// storage key from another profile.
TEST_P(CdmStorageManagerMultipleTest, DeleteDataForStorageKey) {
  auto time_test_started = base::Time::Now();

  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_one = OpenCdmFile(cdm_storage_, kFileName);
  Write(cdm_file_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_one.is_bound());

  auto cdm_file_two_for_remote_one = OpenCdmFile(cdm_storage_, kFileNameTwo);
  Write(cdm_file_two_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_two_for_remote_one.is_bound());

  cdm_storage_manager_two_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_two_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_two = OpenCdmFile(cdm_storage_two_, kFileNameTwo);
  Write(cdm_file_for_remote_two, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_two.is_bound());

  cdm_storage_manager_->DeleteData(base::NullCallback(), kTestStorageKey,
                                   base::Time::Min(), time_test_started,
                                   base::DoNothing());

  ExpectFileContents(cdm_file_for_remote_one, kPopulatedFileValue);
  ExpectFileContents(cdm_file_two_for_remote_one, kPopulatedFileValue);

  ExpectFileContents(cdm_file_for_remote_two, kPopulatedFileValue);

  cdm_storage_manager_->DeleteData(base::NullCallback(), kTestStorageKey,
                                   base::Time::Min(), base::Time::Max(),
                                   base::DoNothing());

  ExpectFileContents(cdm_file_for_remote_one, kEmptyFileValue);
  ExpectFileContents(cdm_file_two_for_remote_one, kEmptyFileValue);

  ExpectFileContents(cdm_file_for_remote_two, kPopulatedFileValue);
}

TEST_P(CdmStorageManagerMultipleTest, DeleteDataForStorageKeyTimeSpecified) {
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_one = OpenCdmFile(cdm_storage_, kFileName);
  Write(cdm_file_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_one.is_bound());

  auto cdm_file_two_for_remote_one = OpenCdmFile(cdm_storage_, kFileNameTwo);
  Write(cdm_file_two_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_two_for_remote_one.is_bound());

  cdm_storage_manager_two_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_two_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_two = OpenCdmFile(cdm_storage_two_, kFileNameTwo);
  Write(cdm_file_for_remote_two, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_two.is_bound());

  cdm_storage_manager_->DeleteData(base::NullCallback(), kTestStorageKey,
                                   base::Time::Min(), base::Time::Max(),
                                   base::DoNothing());
  ExpectFileContents(cdm_file_for_remote_one, kEmptyFileValue);
  ExpectFileContents(cdm_file_two_for_remote_one, kEmptyFileValue);

  ExpectFileContents(cdm_file_for_remote_two, kPopulatedFileValue);
}

TEST_P(CdmStorageManagerMultipleTest, DeleteFileNoDatabase) {
  cdm_storage_manager_->DeleteFile(kTestStorageKey, kCdmType, kFileNameTwo,
                                   base::DoNothing());
  cdm_storage_manager_two_->DeleteFile(kTestStorageKey, kCdmType, kFileNameTwo,
                                       base::DoNothing());
}

TEST_P(CdmStorageManagerMultipleTest, GetSizeForFileNoDatabase) {
  base::test::TestFuture<uint64_t> get_size_future;
  cdm_storage_manager_->GetSizeForFile(kTestStorageKey, kCdmType, kFileNameTwo,
                                       get_size_future.GetCallback());
  EXPECT_EQ(get_size_future.Get(), 0u);

  base::test::TestFuture<uint64_t> get_size_future_two;
  cdm_storage_manager_two_->GetSizeForFile(kTestStorageKey, kCdmType,
                                           kFileNameTwo,
                                           get_size_future_two.GetCallback());
  EXPECT_EQ(get_size_future_two.Get(), 0u);
}

TEST_P(CdmStorageManagerMultipleTest, GetSizeForTests) {
  auto time_test_started = base::Time::Now();
  cdm_storage_manager_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_one = OpenCdmFile(cdm_storage_, kFileName);
  Write(cdm_file_for_remote_one, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_one.is_bound());

  auto cdm_file_two_for_remote_one = OpenCdmFile(cdm_storage_, kFileNameTwo);
  Write(cdm_file_two_for_remote_one, kPopulatedFileValueTwo);
  ASSERT_TRUE(cdm_file_two_for_remote_one.is_bound());

  cdm_storage_manager_two_->OpenCdmStorage(
      CdmStorageBindingContext(kTestStorageKey, kCdmType),
      cdm_storage_two_.BindNewPipeAndPassReceiver());

  auto cdm_file_for_remote_two = OpenCdmFile(cdm_storage_two_, kFileNameTwo);
  Write(cdm_file_for_remote_two, kPopulatedFileValue);
  ASSERT_TRUE(cdm_file_for_remote_two.is_bound());

  base::test::TestFuture<uint64_t> get_size_time_frame_future;
  cdm_storage_manager_->GetSizeForTimeFrame(
      base::Time::Min(), base::Time::Max(),
      get_size_time_frame_future.GetCallback());
  EXPECT_EQ(get_size_time_frame_future.Get(),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());

  base::test::TestFuture<uint64_t> get_size_time_frame_future_two;
  cdm_storage_manager_two_->GetSizeForTimeFrame(
      time_test_started, base::Time::Max(),
      get_size_time_frame_future_two.GetCallback());
  EXPECT_EQ(get_size_time_frame_future_two.Get(), kPopulatedFileValue.size());

  base::test::TestFuture<uint64_t> get_size_storage_key;
  cdm_storage_manager_->GetSizeForStorageKey(
      kTestStorageKey, time_test_started, base::Time::Max(),
      get_size_storage_key.GetCallback());
  EXPECT_EQ(get_size_storage_key.Get(),
            kPopulatedFileValue.size() + kPopulatedFileValueTwo.size());

  base::test::TestFuture<uint64_t> get_size_file;
  cdm_storage_manager_->GetSizeForFile(kTestStorageKey, kCdmType, kFileName,
                                       get_size_file.GetCallback());
  EXPECT_EQ(get_size_file.Get(), kPopulatedFileValue.size());
}

// `testing::Bool()` represents either incognito, or a non incognito profile, so
// that we test on both.
INSTANTIATE_TEST_SUITE_P(,
                         CdmStorageManagerSingularTest,
                         /*is_profile_incognito=*/testing::Bool());

// The pairs represent different combinations of the types of profiles that
// can exist on Chrome together.
INSTANTIATE_TEST_SUITE_P(
    ,
    CdmStorageManagerMultipleTest,
    testing::Combine(/*is_profile_one_incognito=*/testing::Bool(),
                     /*is_profile_two_incognito=*/testing::Bool()));

}  // namespace content
