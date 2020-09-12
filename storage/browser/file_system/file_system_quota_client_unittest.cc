// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_client.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::StorageType;

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
    file_system_context_ =
        CreateFileSystemContextForTesting(nullptr, data_dir_.GetPath());
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64_t size;
    const char* origin_url;
    StorageType type;
  };

 protected:
  scoped_refptr<FileSystemQuotaClient> NewQuotaClient() {
    return base::MakeRefCounted<FileSystemQuotaClient>(
        file_system_context_.get());
  }

  void GetOriginUsageAsync(FileSystemQuotaClient* quota_client,
                           const std::string& origin_url,
                           StorageType type) {
    quota_client->GetOriginUsage(
        url::Origin::Create(GURL(origin_url)), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetUsage,
                       weak_factory_.GetWeakPtr()));
  }

  int64_t GetOriginUsage(FileSystemQuotaClient* quota_client,
                         const std::string& origin_url,
                         StorageType type) {
    GetOriginUsageAsync(quota_client, origin_url, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::vector<url::Origin>& GetOriginsForType(
      FileSystemQuotaClient* quota_client,
      StorageType type) {
    origins_.clear();
    quota_client->GetOriginsForType(
        type, base::BindOnce(&FileSystemQuotaClientTest::OnGetOrigins,
                             weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::vector<url::Origin>& GetOriginsForHost(
      FileSystemQuotaClient* quota_client,
      StorageType type,
      const std::string& host) {
    origins_.clear();
    quota_client->GetOriginsForHost(
        type, host,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetOrigins,
                       weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  void RunAdditionalOriginUsageTask(FileSystemQuotaClient* quota_client,
                                    const std::string& origin_url,
                                    StorageType type) {
    quota_client->GetOriginUsage(
        url::Origin::Create(GURL(origin_url)), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnGetAdditionalUsage,
                       weak_factory_.GetWeakPtr()));
  }

  bool CreateFileSystemDirectory(const base::FilePath& file_path,
                                 const std::string& origin_url,
                                 StorageType storage_type) {
    FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        url::Origin::Create(GURL(origin_url)), type, file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateDirectory(file_system_context_.get(), url);
    return result == base::File::FILE_OK;
  }

  bool CreateFileSystemFile(const base::FilePath& file_path,
                            int64_t file_size,
                            const std::string& origin_url,
                            StorageType storage_type) {
    if (file_path.empty())
      return false;

    FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        url::Origin::Create(GURL(origin_url)), type, file_path);

    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::File::FILE_OK)
      return false;

    result = AsyncFileTestHelper::TruncateFile(file_system_context_.get(), url,
                                               file_size);
    return result == base::File::FILE_OK;
  }

  void InitializeOriginFiles(FileSystemQuotaClient* quota_client,
                             const TestFile* files,
                             int num_files) {
    for (int i = 0; i < num_files; i++) {
      base::FilePath path = base::FilePath().AppendASCII(files[i].name);
      if (files[i].isDirectory) {
        ASSERT_TRUE(CreateFileSystemDirectory(path, files[i].origin_url,
                                              files[i].type));
        if (path.empty()) {
          // Create the usage cache.
          // HACK--we always create the root [an empty path] first.  If we
          // create it later, this will fail due to a quota mismatch.  If we
          // call this before we create the root, it succeeds, but hasn't
          // actually created the cache.
          ASSERT_EQ(0, GetOriginUsage(quota_client, files[i].origin_url,
                                      files[i].type));
        }
      } else {
        ASSERT_TRUE(CreateFileSystemFile(path, files[i].size,
                                         files[i].origin_url, files[i].type));
      }
    }
  }

  // This is a bit fragile--it depends on the test data always creating a
  // directory before adding a file or directory to it, so that we can just
  // count the basename of each addition.  A recursive creation of a path, which
  // created more than one directory in a single shot, would break this.
  int64_t ComputeFilePathsCostForOriginAndType(const TestFile* files,
                                               int num_files,
                                               const std::string& origin_url,
                                               StorageType type) {
    int64_t file_paths_cost = 0;
    for (int i = 0; i < num_files; i++) {
      if (files[i].type == type &&
          GURL(files[i].origin_url) == GURL(origin_url)) {
        base::FilePath path = base::FilePath().AppendASCII(files[i].name);
        if (!path.empty()) {
          file_paths_cost += ObfuscatedFileUtil::ComputeFilePathCost(path);
        }
      }
    }
    return file_paths_cost;
  }

  void DeleteOriginData(FileSystemQuotaClient* quota_client,
                        const std::string& origin,
                        StorageType type) {
    deletion_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    quota_client->DeleteOriginData(
        url::Origin::Create(GURL(origin)), type,
        base::BindOnce(&FileSystemQuotaClientTest::OnDeleteOrigin,
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

  void OnGetOrigins(const std::vector<url::Origin>& origins) {
    origins_ = origins;
  }

  void OnGetAdditionalUsage(int64_t usage_unused) {
    ++additional_callback_count_;
  }

  void OnDeleteOrigin(blink::mojom::QuotaStatusCode status) {
    deletion_status_ = status;
  }

  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> file_system_context_;
  int64_t usage_ = 0;
  int additional_callback_count_ = 0;
  std::vector<url::Origin> origins_;
  blink::mojom::QuotaStatusCode deletion_status_ =
      blink::mojom::QuotaStatusCode::kUnknown;
  base::WeakPtrFactory<FileSystemQuotaClientTest> weak_factory_{this};
};

TEST_F(FileSystemQuotaClientTest, NoFileSystemTest) {
  auto quota_client = NewQuotaClient();

  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
}

TEST_F(FileSystemQuotaClientTest, NoFileTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, OneFileTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {false, "foo", 4921, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, base::size(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + file_paths_cost,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, TwoFilesTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {false, "foo", 10310, kDummyURL1, kTemporary},
      {false, "bar", 41, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, base::size(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + file_paths_cost,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, EmptyFilesTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {false, "foo", 0, kDummyURL1, kTemporary},
      {false, "bar", 0, kDummyURL1, kTemporary},
      {false, "baz", 0, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, base::size(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(file_paths_cost,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, SubDirectoryTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {true, "dirtest", 0, kDummyURL1, kTemporary},
      {false, "dirtest/foo", 11921, kDummyURL1, kTemporary},
      {false, "bar", 4814, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, base::size(kFiles), kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + file_paths_cost,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiTypeTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {true, "dirtest", 0, kDummyURL1, kTemporary},
      {false, "dirtest/foo", 133, kDummyURL1, kTemporary},
      {false, "bar", 14, kDummyURL1, kTemporary},
      {true, "", 0, kDummyURL1, kPersistent},
      {true, "dirtest", 0, kDummyURL1, kPersistent},
      {false, "dirtest/foo", 193, kDummyURL1, kPersistent},
      {false, "bar", 9, kDummyURL1, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost_temporary =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL1, kTemporary);
  const int64_t file_paths_cost_persistent =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL1, kTemporary);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(133 + 14 + file_paths_cost_temporary,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(193 + 9 + file_paths_cost_persistent,
              GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiDomainTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {true, "dir1", 0, kDummyURL1, kTemporary},
      {false, "dir1/foo", 1331, kDummyURL1, kTemporary},
      {false, "bar", 134, kDummyURL1, kTemporary},
      {true, "", 0, kDummyURL1, kPersistent},
      {true, "dir2", 0, kDummyURL1, kPersistent},
      {false, "dir2/foo", 1903, kDummyURL1, kPersistent},
      {false, "bar", 19, kDummyURL1, kPersistent},
      {true, "", 0, kDummyURL2, kTemporary},
      {true, "dom", 0, kDummyURL2, kTemporary},
      {false, "dom/fan", 1319, kDummyURL2, kTemporary},
      {false, "bar", 113, kDummyURL2, kTemporary},
      {true, "", 0, kDummyURL2, kPersistent},
      {true, "dom", 0, kDummyURL2, kPersistent},
      {false, "dom/fan", 2013, kDummyURL2, kPersistent},
      {false, "baz", 18, kDummyURL2, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost_temporary1 =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL1, kTemporary);
  const int64_t file_paths_cost_persistent1 =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL1, kPersistent);
  const int64_t file_paths_cost_temporary2 =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL2, kTemporary);
  const int64_t file_paths_cost_persistent2 =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           kDummyURL2, kPersistent);

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(1331 + 134 + file_paths_cost_temporary1,
              GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(1903 + 19 + file_paths_cost_persistent1,
              GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
    EXPECT_EQ(1319 + 113 + file_paths_cost_temporary2,
              GetOriginUsage(quota_client.get(), kDummyURL2, kTemporary));
    EXPECT_EQ(2013 + 18 + file_paths_cost_persistent2,
              GetOriginUsage(quota_client.get(), kDummyURL2, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, GetUsage_MultipleTasks) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {false, "foo", 11, kDummyURL1, kTemporary},
      {false, "bar", 22, kDummyURL1, kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost = ComputeFilePathsCostForOriginAndType(
      kFiles, base::size(kFiles), kDummyURL1, kTemporary);

  // Dispatching three GetUsage tasks.
  set_additional_callback_count(0);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());

  // Once more, in a different order.
  set_additional_callback_count(0);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(11 + 22 + file_paths_cost, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForType) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, kDummyURL1, kTemporary},
      {true, "", 0, kDummyURL2, kTemporary},
      {true, "", 0, kDummyURL3, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));

  std::vector<url::Origin> origins =
      GetOriginsForType(quota_client.get(), kTemporary);
  EXPECT_EQ(2U, origins.size());
  EXPECT_THAT(origins,
              testing::Contains(url::Origin::Create(GURL(kDummyURL1))));
  EXPECT_THAT(origins,
              testing::Contains(url::Origin::Create(GURL(kDummyURL2))));
  EXPECT_THAT(
      origins,
      testing::Not(testing::Contains(url::Origin::Create(GURL(kDummyURL3)))));
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForHost) {
  auto quota_client = NewQuotaClient();
  const char* kURL1 = "http://foo.com/";
  const char* kURL2 = "https://foo.com/";
  const char* kURL3 = "http://foo.com:1/";
  const char* kURL4 = "http://foo2.com/";
  const char* kURL5 = "http://foo.com:2/";
  const TestFile kFiles[] = {
      {true, "", 0, kURL1, kTemporary},  {true, "", 0, kURL2, kTemporary},
      {true, "", 0, kURL3, kTemporary},  {true, "", 0, kURL4, kTemporary},
      {true, "", 0, kURL5, kPersistent},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));

  std::vector<url::Origin> origins =
      GetOriginsForHost(quota_client.get(), kTemporary, "foo.com");
  EXPECT_EQ(3U, origins.size());
  EXPECT_THAT(origins, testing::Contains(url::Origin::Create(GURL(kURL1))));
  EXPECT_THAT(origins, testing::Contains(url::Origin::Create(GURL(kURL2))));
  EXPECT_THAT(origins, testing::Contains(url::Origin::Create(GURL(kURL3))));
  EXPECT_THAT(origins, testing::Not(testing::Contains(url::Origin::Create(
                           GURL(kURL4)))));  // Different host.
  EXPECT_THAT(origins, testing::Not(testing::Contains(url::Origin::Create(
                           GURL(kURL5)))));  // Different type.
}

TEST_F(FileSystemQuotaClientTest, DeleteOriginTest) {
  auto quota_client = NewQuotaClient();
  const TestFile kFiles[] = {
      {true, "", 0, "http://foo.com/", kTemporary},
      {false, "a", 1, "http://foo.com/", kTemporary},
      {true, "", 0, "https://foo.com/", kTemporary},
      {false, "b", 2, "https://foo.com/", kTemporary},
      {true, "", 0, "http://foo.com/", kPersistent},
      {false, "c", 4, "http://foo.com/", kPersistent},
      {true, "", 0, "http://bar.com/", kTemporary},
      {false, "d", 8, "http://bar.com/", kTemporary},
      {true, "", 0, "http://bar.com/", kPersistent},
      {false, "e", 16, "http://bar.com/", kPersistent},
      {true, "", 0, "https://bar.com/", kPersistent},
      {false, "f", 32, "https://bar.com/", kPersistent},
      {true, "", 0, "https://bar.com/", kTemporary},
      {false, "g", 64, "https://bar.com/", kTemporary},
  };
  InitializeOriginFiles(quota_client.get(), kFiles, base::size(kFiles));
  const int64_t file_paths_cost_temporary_foo_https =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           "https://foo.com/", kTemporary);
  const int64_t file_paths_cost_persistent_foo =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           "http://foo.com/", kPersistent);
  const int64_t file_paths_cost_temporary_bar =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           "http://bar.com/", kTemporary);
  const int64_t file_paths_cost_temporary_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           "https://bar.com/", kTemporary);
  const int64_t file_paths_cost_persistent_bar_https =
      ComputeFilePathsCostForOriginAndType(kFiles, base::size(kFiles),
                                           "https://bar.com/", kPersistent);

  DeleteOriginData(quota_client.get(), "http://foo.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  DeleteOriginData(quota_client.get(), "http://bar.com/", kPersistent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  DeleteOriginData(quota_client.get(), "http://buz.com/", kTemporary);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status());

  EXPECT_EQ(0,
            GetOriginUsage(quota_client.get(), "http://foo.com/", kTemporary));
  EXPECT_EQ(0,
            GetOriginUsage(quota_client.get(), "http://bar.com/", kPersistent));
  EXPECT_EQ(0,
            GetOriginUsage(quota_client.get(), "http://buz.com/", kTemporary));

  EXPECT_EQ(2 + file_paths_cost_temporary_foo_https,
            GetOriginUsage(quota_client.get(), "https://foo.com/", kTemporary));
  EXPECT_EQ(4 + file_paths_cost_persistent_foo,
            GetOriginUsage(quota_client.get(), "http://foo.com/", kPersistent));
  EXPECT_EQ(8 + file_paths_cost_temporary_bar,
            GetOriginUsage(quota_client.get(), "http://bar.com/", kTemporary));
  EXPECT_EQ(
      32 + file_paths_cost_persistent_bar_https,
      GetOriginUsage(quota_client.get(), "https://bar.com/", kPersistent));
  EXPECT_EQ(64 + file_paths_cost_temporary_bar_https,
            GetOriginUsage(quota_client.get(), "https://bar.com/", kTemporary));
}

}  // namespace storage
