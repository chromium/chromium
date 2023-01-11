// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

const char kTestData[] = "Hello World";
const char kEntryKey[] = "FooEntry";
const int kCacheEntryIndex = 1;

// A CacheStorageBlobToDiskCache that can delay reading from blobs.
class TestCacheStorageBlobToDiskCache : public CacheStorageBlobToDiskCache {
 public:
  explicit TestCacheStorageBlobToDiskCache(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
      : CacheStorageBlobToDiskCache(std::move(quota_manager_proxy),
                                    blink::StorageKey()) {}

  TestCacheStorageBlobToDiskCache(const TestCacheStorageBlobToDiskCache&) =
      delete;
  TestCacheStorageBlobToDiskCache& operator=(
      const TestCacheStorageBlobToDiskCache&) = delete;

  ~TestCacheStorageBlobToDiskCache() override = default;

  void ContinueReadFromBlob() { CacheStorageBlobToDiskCache::ReadFromBlob(); }

  void set_delay_blob_reads(bool delay) { delay_blob_reads_ = delay; }

  void DidWriteDataToEntry(int expected_bytes, int rv) {
    CacheStorageBlobToDiskCache::DidWriteDataToEntry(expected_bytes, rv);
  }

  const blink::StorageKey& storage_key() {
    return CacheStorageBlobToDiskCache::storage_key();
  }

 protected:
  void ReadFromBlob() override {
    if (delay_blob_reads_)
      return;
    ContinueReadFromBlob();
  }

 private:
  bool delay_blob_reads_ = false;
};

class CacheStorageBlobToDiskCacheTest : public testing::Test {
 protected:
  CacheStorageBlobToDiskCacheTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(std::make_unique<TestBrowserContext>()),
        data_(kTestData) {}

  void SetUp() override {
    InitBlobStorage();
    InitBlob();
    InitCache();
    InitQuotaManager();

    cache_storage_blob_to_disk_cache_ =
        std::make_unique<TestCacheStorageBlobToDiskCache>(
            quota_manager_proxy());
  }

  void TearDown() override {
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
  }

  void InitBlobStorage() {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(browser_context_.get());
    base::RunLoop().RunUntilIdle();

    blob_storage_context_ = blob_storage_context->context();
  }

  void InitBlob() {
    auto blob_data =
        std::make_unique<storage::BlobDataBuilder>("blob-id:myblob");
    blob_data->AppendData(data_);
    blob_handle_ = blob_storage_context_->AddFinishedBlob(std::move(blob_data));
  }

  void InitCache() {
    disk_cache::BackendResult backend_result = CreateCacheBackend(
        net::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT,
        /*file_operations=*/nullptr, base::FilePath(),
        (CacheStorageBlobToDiskCache::kBufferSize * 100) /* max bytes */,
        disk_cache::ResetHandling::kNeverReset, nullptr /* net log */,
        base::DoNothing());
    // The memory cache runs synchronously.
    EXPECT_EQ(net::OK, backend_result.net_error);
    cache_backend_ = std::move(backend_result.backend);
    EXPECT_TRUE(cache_backend_);

    disk_cache::EntryResult result =
        cache_backend_->CreateEntry(kEntryKey, net::HIGHEST, base::DoNothing());
    EXPECT_EQ(net::OK, result.net_error());
    disk_cache_entry_.reset(result.ReleaseEntry());
  }

  void InitQuotaManager() {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath().AppendASCII("filesystem");
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        false /* is_incognito */, base_dir,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager(), base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  std::string ReadCacheContent() {
    int bytes_to_read = disk_cache_entry_->GetDataSize(kCacheEntryIndex);
    scoped_refptr<net::IOBufferWithSize> buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(bytes_to_read);

    int rv = disk_cache_entry_->ReadData(kCacheEntryIndex, 0 /* offset */,
                                         buffer.get(), buffer->size(),
                                         base::DoNothing());
    EXPECT_EQ(bytes_to_read, rv);
    return std::string(buffer->data(), bytes_to_read);
  }

  bool Stream() {
    mojo::PendingRemote<blink::mojom::Blob> blob_remote;
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*blob_handle_),
        blob_remote.InitWithNewPipeAndPassReceiver());

    cache_storage_blob_to_disk_cache_->StreamBlobToCache(
        std::move(disk_cache_entry_), kCacheEntryIndex, std::move(blob_remote),
        blob_handle_->size(),
        base::BindOnce(&CacheStorageBlobToDiskCacheTest::StreamCallback,
                       base::Unretained(this)));

    base::RunLoop().RunUntilIdle();

    return callback_called_ && callback_success_;
  }

  void StreamCallback(ScopedWritableEntry entry_ptr, bool success) {
    disk_cache_entry_ = std::move(entry_ptr);
    callback_success_ = success;
    callback_called_ = true;
  }

  storage::MockQuotaManager* quota_manager() {
    return static_cast<storage::MockQuotaManager*>(quota_manager_.get());
  }

  storage::MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<storage::MockQuotaManagerProxy*>(
        quota_manager_proxy_.get());
  }

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  raw_ptr<storage::BlobStorageContext> blob_storage_context_;
  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  std::unique_ptr<disk_cache::Backend> cache_backend_;
  ScopedWritableEntry disk_cache_entry_;
  std::unique_ptr<TestCacheStorageBlobToDiskCache>
      cache_storage_blob_to_disk_cache_;
  std::string data_;

  bool callback_success_ = false;
  bool callback_called_ = false;

 private:
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::ScopedTempDir base_;
};

TEST_F(CacheStorageBlobToDiskCacheTest, Stream) {
  EXPECT_TRUE(Stream());
  EXPECT_STREQ(data_.c_str(), ReadCacheContent().c_str());
}

TEST_F(CacheStorageBlobToDiskCacheTest, StreamLarge) {
  blob_handle_.reset();
  base::RunLoop().RunUntilIdle();

  data_ = std::string(CacheStorageBlobToDiskCache::kBufferSize * 4, '.');
  InitBlob();

  EXPECT_TRUE(Stream());
  EXPECT_STREQ(data_.c_str(), ReadCacheContent().c_str());
}

TEST_F(CacheStorageBlobToDiskCacheTest, TestDelayMidStream) {
  cache_storage_blob_to_disk_cache_->set_delay_blob_reads(true);

  // The operation should stall while reading from the blob.
  EXPECT_FALSE(Stream());
  EXPECT_FALSE(callback_called_);

  // Verify that continuing results in a completed operation.
  cache_storage_blob_to_disk_cache_->set_delay_blob_reads(false);
  cache_storage_blob_to_disk_cache_->ContinueReadFromBlob();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(callback_success_);
}

TEST_F(CacheStorageBlobToDiskCacheTest, DeleteMidStream) {
  cache_storage_blob_to_disk_cache_->set_delay_blob_reads(true);

  // The operation should stall while reading from the blob.
  EXPECT_FALSE(Stream());

  // Deleting the BlobToDiskCache mid-stream shouldn't cause a crash.
  cache_storage_blob_to_disk_cache_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called_);
}

TEST_F(CacheStorageBlobToDiskCacheTest, NotifyQuotaAboutWriteErrors) {
  cache_storage_blob_to_disk_cache_->DidWriteDataToEntry(5, 2);
  auto write_error_tracker = quota_manager()->write_error_tracker();
  EXPECT_EQ(1U, write_error_tracker.size());
  auto write_error_log = write_error_tracker.find(
      cache_storage_blob_to_disk_cache_->storage_key());
  EXPECT_NE(write_error_tracker.end(), write_error_log);
  EXPECT_EQ(1, write_error_log->second);
}
}  // namespace

}  // namespace content
