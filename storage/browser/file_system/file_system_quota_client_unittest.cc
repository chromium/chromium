// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_client.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {
namespace {

const char kDummyURL1[] = "http://www.dummy.org";
const char kDummyURL2[] = "http://www.example.com";
const char kDummyURL3[] = "http://www.bleh";

// Declared to shorten the variable names.
const StorageType kTemporary = StorageType::kTemporary;
const StorageType kPersistent = StorageType::kPersistent;

}  // namespace

class FileSystemQuotaClientTest : public testing::Test {
 public:
  FileSystemQuotaClientTest() = default;
  ~FileSystemQuotaClientTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, data_dir_.GetPath());
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64_t size;
    const char* origin_url;
    FileSystemType type;
  };

 protected:
  storage::FileSystemContext* GetFileSystemContext() {
    return file_system_context_.get();
  }

  void GetStorageKeyUsageAsync(storage::mojom::QuotaClient& quota_client,
                               const std::string& origin_url,
                               StorageType type) {
    quota_client.GetStorageKeyUsage(
        StorageKey::CreateFromStringForTesting(origin_url), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetUsage,
                       weak_factory_.GetWeakPtr()));
  }

  int64_t GetStorageKeyUsage(storage::mojom::QuotaClient& quota_client,
                             const std::string& origin_url,
                             StorageType type) {
    GetStorageKeyUsageAsync(quota_client, origin_url, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::vector<StorageKey>& GetStorageKeysForType(
      storage::mojom::QuotaClient& quota_client,
      StorageType type) {
    storage_keys_.clear();
    quota_client.GetStorageKeysForType(
        type, base::BindOnce(&FileSystemQuotaClientTest::OnGetStorageKeys,
                             weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return storage_keys_;
  }

  const std::vector<StorageKey>& GetStorageKeysForHost(
      storage::mojom::QuotaClient& quota_client,
      StorageType type,
      const std::string& host) {
    storage_keys_.clear();
    quota_client.GetStorageKeysForHost(
        type, host,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetStorageKeys,
                       weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return storage_keys_;
  }

  void RunAdditionalStorageKeyUsageTask(
      storage::mojom::QuotaClient& quota_client,
      const std::string& origin_url,
      StorageType type) {
    quota_client.GetStorageKeyUsage(
        StorageKey::CreateFromStringForTesting(origin_url), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetAdditionalUsage,
                       weak_factory_.GetWeakPtr()));
  }

  bool CreateFileSystemDirectory(const base::FilePath& file_path,
                                 const std::string& origin_url,
                                 FileSystemType type) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type,
        file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateDirectory(file_system_context_.get(), url);
    return result == base::File::FILE_OK;
  }

  bool CreateFileSystemFile(const base::FilePath& file_path,
                            int64_t file_size,
                            const std::string& origin_url,
                            FileSystemType type) {
    if (file_path.empty())
      return false;

    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(origin_url), type,
        file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::File::FILE_OK)
      return false;

    result = AsyncFileTestHelper::TruncateFile(file_system_context_.get(), url,
                                               file_size);
    return result == base::File::FILE_OK;
  }

  void InitializeOriginFiles(storage::mojom::QuotaClient& quota_client,
                             const std::vector<TestFile>& files) {
    for (const TestFile& file : files) {
      base::FilePath path = base::FilePath().AppendASCII(file.name);
      if (file.isDirectory) {
        ASSERT_TRUE(
            CreateFileSystemDirectory(path, file.origin_url, file.type));
        if (path.empty()) {
          // Create the usage cache.
          // HACK--we always create the root [an empty path] first.  If we
          // create it later, this will fail due to a quota mismatch.  If we
          // call this before we create the root, it succeeds, but hasn't
          // actually created the cache.
          ASSERT_EQ(0, GetStorageKeyUsage(
                           quota_client, file.origin_url,
                           FileSystemTypeToQuotaStorageType(file.type)));
        }
      } else {
        ASSERT_TRUE(
            CreateFileSystemFile(path, file.size, file.origin_url, file.type));
      }
    }
  }

  // This is a bit fragile--it depends on the test data always creating a
  // directory before adding a file or directory to it, so that we can just
  // count the basename of each addition.  A recursive creation of a path, which
  // created more than one directory in a single shot, would break this.
  int64_t ComputeFilePathsCostForOriginAndType(
      const base::span<const TestFile> files,
      const std::string& origin_url,
      FileSystemType type) {
    int64_t file_paths_cost = 0;
    for (const TestFile& file : files) {
      if (file.type == type && GURL(file.origin_url) == GURL(origin_url)) {
        base::FilePath path = base::FilePath().AppendASCII(file.name);
        if (!path.empty()) {
          file_paths_cost += ObfuscatedFileUtil::ComputeFilePathCost(path);
        }
      }
    }
    return file_paths_cost;
  }

  void DeleteStorageKeyData(FileSystemQuotaClient* quota_client,
                            const std::string& origin,
                            StorageType type) {
    deletion_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    quota_client->DeleteStorageKeyData(
        StorageKey::CreateFromStringForTesting(origin), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnDeleteStorageKey,
                       weak_factory_.GetWeakPtr()));
  }

  int64_t usage() const { return usage_; }
  blink::mojom::QuotaStatusCode status() { return deletion_status_; }
  int additional_callback_count() const { return additional_callback_count_; }
  void set_additional_callback_count(int count) {
    additional_callback_count_ = count;
  }

 private:
  void OnGetUsage(int64_t usage) { usage_ = usage; }

  void OnGetStorageKeys(const std::vector<StorageKey>& storage_keys) {
    storage_keys_ = storage_keys;
  }

  void OnGetAdditionalUsage(int64_t usage_unused) {
    ++additional_callback_count_;
  }

  void OnDeleteStorageKey(blink::mojom::QuotaStatusCode status) {
    deletion_status_ = status;
  }

  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> file_system_context_;
  int64_t usage_ = 0;
  int additional_callback_count_ = 0;
  std::vector<StorageKey> storage_keys_;
  blink::mojom::QuotaStatusCode deletion_status_ =
      blink::mojom::QuotaStatusCode::kUnknown;
  base::WeakPtrFactory<FileSystemQuotaClientTest> weak_factory_{this};
};

TEST_F(FileSystemQuotaClientTest, NoFileSystemTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());

  EXPECT_EQ(0, GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
}

TEST_F(FileSystemQuotaClientTest, NoFileTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());

  InitializeOriginFiles(quota_client,
                        {{true, "", 0, kDummyURL1, kFileSystemTypeTemporary}});

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(0, GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, OneFileTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 4921, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + file_paths_cost,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, TwoFilesTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 10310, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 41, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + file_paths_cost,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, EmptyFilesTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "baz", 0, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(file_paths_cost,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, SubDirectoryTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dirtest/foo", 11921, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 4814, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + file_paths_cost,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiTypeTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dirtest/foo", 133, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 14, kDummyURL1, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL1, kFileSystemTypePersistent},
      {true, "dirtest", 0, kDummyURL1, kFileSystemTypePersistent},
      {false, "dirtest/foo", 193, kDummyURL1, kFileSystemTypePersistent},
      {false, "bar", 9, kDummyURL1, kFileSystemTypePersistent},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypePersistent);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(133 + 14 + file_paths_cost_temporary,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
    EXPECT_EQ(193 + 9 + file_paths_cost_persistent,
              GetStorageKeyUsage(quota_client, kDummyURL1, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiDomainTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {true, "dir1", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "dir1/foo", 1331, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 134, kDummyURL1, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL1, kFileSystemTypePersistent},
      {true, "dir2", 0, kDummyURL1, kFileSystemTypePersistent},
      {false, "dir2/foo", 1903, kDummyURL1, kFileSystemTypePersistent},
      {false, "bar", 19, kDummyURL1, kFileSystemTypePersistent},
      {true, "", 0, kDummyURL2, kFileSystemTypeTemporary},
      {true, "dom", 0, kDummyURL2, kFileSystemTypeTemporary},
      {false, "dom/fan", 1319, kDummyURL2, kFileSystemTypeTemporary},
      {false, "bar", 113, kDummyURL2, kFileSystemTypeTemporary},
      {true, "", 0, kDummyURL2, kFileSystemTypePersistent},
      {true, "dom", 0, kDummyURL2, kFileSystemTypePersistent},
      {false, "dom/fan", 2013, kDummyURL2, kFileSystemTypePersistent},
      {false, "baz", 18, kDummyURL2, kFileSystemTypePersistent},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary1 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent1 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL1,
                                           kFileSystemTypePersistent);
  const int64_t file_paths_cost_temporary2 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL2,
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent2 =
      ComputeFilePathsCostForOriginAndType(kFiles, kDummyURL2,
                                           kFileSystemTypePersistent);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(1331 + 134 + file_paths_cost_temporary1,
              GetStorageKeyUsage(quota_client, kDummyURL1, kTemporary));
    EXPECT_EQ(1903 + 19 + file_paths_cost_persistent1,
              GetStorageKeyUsage(quota_client, kDummyURL1, kPersistent));
    EXPECT_EQ(1319 + 113 + file_paths_cost_temporary2,
              GetStorageKeyUsage(quota_client, kDummyURL2, kTemporary));
    EXPECT_EQ(2013 + 18 + file_paths_cost_persistent2,
              GetStorageKeyUsage(quota_client, kDummyURL2, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, GetUsage_MultipleTasks) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
      {false, "foo", 11, kDummyURL1, kFileSystemTypeTemporary},
      {false, "bar", 22, kDummyURL1, kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, kDummyURL1, kFileSystemTypeTemporary);

  // Dispatching three GetUsage tasks.
  set_additional_callback_count(0);
  GetStorageKeyUsageAsync(quota_client, kDummyURL1, kTemporary);
  RunAdditionalStorageKeyUsageTask(quota_client, kDummyURL1, kTemporary);
  RunAdditionalStorageKeyUsageTask(quota_client, kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());

  // Once more, in a different order.
  set_additional_callback_count(0);
  RunAdditionalStorageKeyUsageTask(quota_client, kDummyURL1, kTemporary);
  GetStorageKeyUsageAsync(quota_client, kDummyURL1, kTemporary);
  RunAdditionalStorageKeyUsageTask(quota_client, kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(FileSystemQuotaClientTest, GetStorageKeysForType) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  InitializeOriginFiles(
      quota_client, {
                        {true, "", 0, kDummyURL1, kFileSystemTypeTemporary},
                        {true, "", 0, kDummyURL2, kFileSystemTypeTemporary},
                        {true, "", 0, kDummyURL3, kFileSystemTypePersistent},
                    });

  EXPECT_THAT(GetStorageKeysForType(quota_client, kTemporary),
              testing::UnorderedElementsAre(
                  StorageKey::CreateFromStringForTesting(kDummyURL1),
                  StorageKey::CreateFromStringForTesting(kDummyURL2)));
}

TEST_F(FileSystemQuotaClientTest, GetStorageKeysForHost) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const char* kURL1 = "http://foo.com/";
  const char* kURL2 = "https://foo.com/";
  const char* kURL3 = "http://foo.com:1/";
  const char* kURL4 = "http://foo2.com/";
  const char* kURL5 = "http://foo.com:2/";
  InitializeOriginFiles(quota_client,
                        {
                            {true, "", 0, kURL1, kFileSystemTypeTemporary},
                            {true, "", 0, kURL2, kFileSystemTypeTemporary},
                            {true, "", 0, kURL3, kFileSystemTypeTemporary},
                            {true, "", 0, kURL4, kFileSystemTypeTemporary},
                            {true, "", 0, kURL5, kFileSystemTypePersistent},
                        });

  EXPECT_THAT(GetStorageKeysForHost(quota_client, kTemporary, "foo.com"),
              testing::UnorderedElementsAre(
                  StorageKey::CreateFromStringForTesting(kURL1),
                  StorageKey::CreateFromStringForTesting(kURL2),
                  StorageKey::CreateFromStringForTesting(kURL3)));
}

TEST_F(FileSystemQuotaClientTest, DeleteOriginTest) {
  FileSystemQuotaClient quota_client(GetFileSystemContext());
  const std::vector<TestFile> kFiles = {
      {true, "", 0, "http://foo.com/", kFileSystemTypeTemporary},
      {false, "a", 1, "http://foo.com/", kFileSystemTypeTemporary},
      {true, "", 0, "https://foo.com/", kFileSystemTypeTemporary},
      {false, "b", 2, "https://foo.com/", kFileSystemTypeTemporary},
      {true, "", 0, "http://foo.com/", kFileSystemTypePersistent},
      {false, "c", 4, "http://foo.com/", kFileSystemTypePersistent},
      {true, "", 0, "http://bar.com/", kFileSystemTypeTemporary},
      {false, "d", 8, "http://bar.com/", kFileSystemTypeTemporary},
      {true, "", 0, "http://bar.com/", kFileSystemTypePersistent},
      {false, "e", 16, "http://bar.com/", kFileSystemTypePersistent},
      {true, "", 0, "https://bar.com/", kFileSystemTypePersistent},
      {false, "f", 32, "https://bar.com/", kFileSystemTypePersistent},
      {true, "", 0, "https://bar.com/", kFileSystemTypeTemporary},
      {false, "g", 64, "https://bar.com/", kFileSystemTypeTemporary},
  };
  InitializeOriginFiles(quota_client, kFiles);
  const int64_t file_paths_cost_temporary_foo_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://foo.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent_foo =
      ComputeFilePathsCostForOriginAndType(kFiles, "http://foo.com/",
                                           kFileSystemTypePersistent);
  const int64_t file_paths_cost_temporary_bar =
      ComputeFilePathsCostForOriginAndType(kFiles, "http://bar.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_temporary_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://bar.com/",
                                           kFileSystemTypeTemporary);
  const int64_t file_paths_cost_persistent_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, "https://bar.com/",
                                           kFileSystemTypePersistent);

  DeleteStorageKeyData(&quota_client, "http://foo.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  DeleteStorageKeyData(&quota_client, "http://bar.com/", kPersistent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  DeleteStorageKeyData(&quota_client, "http://buz.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  EXPECT_EQ(0, GetStorageKeyUsage(quota_client, "http://foo.com/", kTemporary));
  EXPECT_EQ(0,
            GetStorageKeyUsage(quota_client, "http://bar.com/", kPersistent));
  EXPECT_EQ(0, GetStorageKeyUsage(quota_client, "http://buz.com/", kTemporary));

  EXPECT_EQ(2 + file_paths_cost_temporary_foo_https,
            GetStorageKeyUsage(quota_client, "https://foo.com/", kTemporary));
  EXPECT_EQ(4 + file_paths_cost_persistent_foo,
            GetStorageKeyUsage(quota_client, "http://foo.com/", kPersistent));
  EXPECT_EQ(8 + file_paths_cost_temporary_bar,
            GetStorageKeyUsage(quota_client, "http://bar.com/", kTemporary));
  EXPECT_EQ(32 + file_paths_cost_persistent_bar_https,
            GetStorageKeyUsage(quota_client, "https://bar.com/", kPersistent));
  EXPECT_EQ(64 + file_paths_cost_temporary_bar_https,
            GetStorageKeyUsage(quota_client, "https://bar.com/", kTemporary));
}

}  // namespace storage
