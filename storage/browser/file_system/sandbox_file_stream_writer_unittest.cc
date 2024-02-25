// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_writer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_stream_writer_test.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
const char kURLOrigin[] = "http://remote/";
}  // namespace

class SandboxFileStreamWriterTest : public FileStreamWriterTest {
 public:
  SandboxFileStreamWriterTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()) {}
  ~SandboxFileStreamWriterTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        is_incognito(), dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    file_system_context_ =
        CreateFileSystemContext(quota_manager_proxy_.get(), dir_);

    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/std::nullopt, kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const FileSystemURL& root_url,
                          const std::string& name, base::File::Error result) {
          ASSERT_EQ(base::File::FILE_OK, result);
        }));

    SetQuota(1024 * 1024 * 100);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    file_system_context_ = nullptr;
    quota_manager_proxy_ = nullptr;
    quota_manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<FileSystemContext> file_system_context_;

  struct quota_usage_and_info {
    blink::mojom::QuotaStatusCode status;
    int64_t usage;
    int64_t quota;
  };

  virtual scoped_refptr<FileSystemContext> CreateFileSystemContext(
      QuotaManagerProxy* quota_manager_proxy,
      const base::ScopedTempDir& dir) {
    return CreateFileSystemContextForTesting(quota_manager_proxy,
                                             dir.GetPath());
  }

  virtual bool is_incognito() { return false; }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        kFileSystemTypeTemporary, base::FilePath().AppendASCII(file_name));
  }

  bool CreateFileWithContent(const std::string& name,
                             const std::string& data) override {
    return AsyncFileTestHelper::CreateFileWithData(file_system_context_.get(),
                                                   GetFileSystemURL(name),
                                                   data) == base::File::FILE_OK;
  }

  std::unique_ptr<FileStreamWriter> CreateWriter(const std::string& name,
                                                 int64_t offset) override {
    auto writer = std::make_unique<SandboxFileStreamWriter>(
        file_system_context_.get(), GetFileSystemURL(name), offset,
        *file_system_context_->GetUpdateObservers(kFileSystemTypeTemporary));
    return writer;
  }

  bool FilePathExists(const std::string& name) override {
    return AsyncFileTestHelper::FileExists(file_system_context_.get(),
                                           GetFileSystemURL(name),
                                           AsyncFileTestHelper::kDontCheckSize);
  }

  std::string GetFileContent(const std::string& name) override {
    base::File::Info info;
    const FileSystemURL url = GetFileSystemURL(name);

    EXPECT_EQ(base::File::FILE_OK, AsyncFileTestHelper::GetMetadata(
                                       file_system_context_.get(), url, &info));

    std::unique_ptr<FileStreamReader> reader(
        file_system_context_.get()->CreateFileStreamReader(url, 0, info.size,
                                                           base::Time()));

    auto data_or_error = ReadFromReader(*reader, /*bytes_to_read=*/info.size);
    EXPECT_TRUE(data_or_error.has_value());
    EXPECT_THAT(data_or_error.value(), testing::SizeIs(info.size));

    return data_or_error.value();
  }

  std::unique_ptr<SandboxFileStreamWriter> CreateSandboxWriter(
      const std::string& name,
      int64_t offset) {
    auto writer = std::make_unique<SandboxFileStreamWriter>(
        file_system_context_.get(), GetFileSystemURL(name), offset,
        *file_system_context_->GetUpdateObservers(kFileSystemTypeTemporary));
    return writer;
  }

  quota_usage_and_info GetUsageAndQuotaSync() {
    base::test::TestFuture<blink::mojom::QuotaStatusCode, int64_t, int64_t>
        future;
    quota_manager_->GetUsageAndQuota(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        blink::mojom::StorageType::kTemporary, future.GetCallback());

    quota_usage_and_info info;
    info.status = future.Get<0>();
    info.usage = future.Get<1>();
    info.quota = future.Get<2>();
    return info;
  }

  void SetQuota(int64_t quota) {
    quota_manager_->SetQuota(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        blink::mojom::StorageType::kTemporary, quota);
  }

  int64_t GetFreeQuota() {
    auto info = GetUsageAndQuotaSync();
    return info.quota - info.usage;
  }

  void SetFreeQuota(int64_t free_quota) {
    auto info = GetUsageAndQuotaSync();
    SetQuota(info.usage + free_quota);
  }

  void Test_Quota_DefaultBucketCreated() {
    // Call method on context to ensure that OpenFileSystem task has completed.
    EXPECT_TRUE(CreateFileWithContent("file_a", "foo"));

    QuotaManagerProxySync quota_manager_proxy_sync(quota_manager_proxy_.get());

    // Check default bucket exist.
    ASSERT_OK_AND_ASSIGN(
        BucketInfo result,
        quota_manager_proxy_sync.GetBucket(
            blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
            kDefaultBucketName, blink::mojom::StorageType::kTemporary));
    EXPECT_EQ(result.name, kDefaultBucketName);
    EXPECT_EQ(result.storage_key,
              blink::StorageKey::CreateFromStringForTesting(kURLOrigin));
    EXPECT_GT(result.id.value(), 0);
  }

  void Test_Quota_OK() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(7);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 3));
    EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("fooxxx", 6), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 4);
  }

  void Test_Quota_WritePastEnd() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(6);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 6));
    EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo\0\0\0xxx", 9), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 0);
  }

  void Test_Quota_NoSpace() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(0);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 3));
    EXPECT_EQ(net::ERR_FILE_NO_SPACE, WriteStringToWriter(writer.get(), "xxx"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo", 3), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 0);
  }

  void Test_Quota_NoSpace_PartialWrite() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(5);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 6));
    EXPECT_EQ(net::ERR_FILE_NO_SPACE, WriteStringToWriter(writer.get(), "xxx"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo\0\0\0xx", 8), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 0);
  }

  void Test_Quota_Negative() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(-1);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 3));
    EXPECT_EQ(net::ERR_FILE_NO_SPACE, WriteStringToWriter(writer.get(), "xxx"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo", 3), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), -1);
  }

  void Test_Quota_WritePastEndTwice_OK() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(9);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 6));
    EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
    EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "yyy"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo\0\0\0xxxyyy", 12), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 0);
  }

  void Test_Quota_WritePastEndTwice_NoSpace() {
    std::string name = "file_a";
    EXPECT_TRUE(CreateFileWithContent(name, "foo"));

    SetFreeQuota(7);
    std::unique_ptr<SandboxFileStreamWriter> writer(
        CreateSandboxWriter(name, 6));
    EXPECT_EQ(net::OK, WriteStringToWriter(writer.get(), "xxx"));
    EXPECT_EQ(net::ERR_FILE_NO_SPACE, WriteStringToWriter(writer.get(), "yyy"));
    writer.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FilePathExists(name));
    EXPECT_EQ(std::string("foo\0\0\0xxxy", 10), GetFileContent(name));
    EXPECT_EQ(GetFreeQuota(), 0);
  }
};

TEST_F(SandboxFileStreamWriterTest, Test_Quota_DefaultBucketCreated) {
  Test_Quota_DefaultBucketCreated();
}

TEST_F(SandboxFileStreamWriterTest, Quota_OK) {
  Test_Quota_OK();
}

TEST_F(SandboxFileStreamWriterTest, Quota_WritePastEnd) {
  Test_Quota_WritePastEnd();
}

TEST_F(SandboxFileStreamWriterTest, Quota_NoSpace) {
  Test_Quota_NoSpace();
}

TEST_F(SandboxFileStreamWriterTest, Quota_NoSpace_PartialWrite) {
  Test_Quota_NoSpace_PartialWrite();
}

TEST_F(SandboxFileStreamWriterTest, Quota_Negative) {
  Test_Quota_Negative();
}

TEST_F(SandboxFileStreamWriterTest, Quota_WritePastEndTwice_OK) {
  Test_Quota_WritePastEndTwice_OK();
}

TEST_F(SandboxFileStreamWriterTest, Quota_WritePastEndTwice_NoSpace) {
  Test_Quota_WritePastEndTwice_NoSpace();
}

INSTANTIATE_TYPED_TEST_SUITE_P(Sandbox,
                               FileStreamWriterTypedTest,
                               SandboxFileStreamWriterTest);

class SandboxFileStreamWriterIncognitoTest
    : public SandboxFileStreamWriterTest {
 public:
  SandboxFileStreamWriterIncognitoTest() = default;

 protected:
  scoped_refptr<FileSystemContext> CreateFileSystemContext(
      QuotaManagerProxy* quota_manager_proxy,
      const base::ScopedTempDir& dir) override {
    return CreateIncognitoFileSystemContextForTesting(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), quota_manager_proxy,
        dir.GetPath());
  }

  bool is_incognito() override { return true; }
};

TEST_F(SandboxFileStreamWriterIncognitoTest, Test_Quota_DefaultBucketCreated) {
  Test_Quota_DefaultBucketCreated();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_OK) {
  Test_Quota_OK();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_WritePastEnd) {
  Test_Quota_WritePastEnd();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_NoSpace) {
  Test_Quota_NoSpace();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_NoSpace_PartialWrite) {
  Test_Quota_NoSpace_PartialWrite();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_Negative) {
  Test_Quota_Negative();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_WritePastEndTwice_OK) {
  Test_Quota_WritePastEndTwice_OK();
}

TEST_F(SandboxFileStreamWriterIncognitoTest, Quota_WritePastEndTwice_NoSpace) {
  Test_Quota_WritePastEndTwice_NoSpace();
}

INSTANTIATE_TYPED_TEST_SUITE_P(SandboxIncognito,
                               FileStreamWriterTypedTest,
                               SandboxFileStreamWriterIncognitoTest);

}  // namespace storage
