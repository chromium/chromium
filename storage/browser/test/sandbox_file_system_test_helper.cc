// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/sandbox_file_system_test_helper.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

SandboxFileSystemTestHelper::SandboxFileSystemTestHelper(
    const blink::StorageKey& storage_key,
    FileSystemType type)
    : storage_key_(storage_key), type_(type), file_util_(nullptr) {}

SandboxFileSystemTestHelper::SandboxFileSystemTestHelper()
    : storage_key_(
          blink::StorageKey::CreateFromStringForTesting("http://foo.com")),
      type_(kFileSystemTypeTemporary),
      file_util_(nullptr) {}

SandboxFileSystemTestHelper::~SandboxFileSystemTestHelper() = default;

void SandboxFileSystemTestHelper::SetUp(const base::FilePath& base_dir) {
  SetUp(base_dir, nullptr);
}

void SandboxFileSystemTestHelper::SetUp(
    scoped_refptr<FileSystemContext> file_system_context) {
  file_system_context_ = std::move(file_system_context);

  SetUpFileSystem();
}

void SandboxFileSystemTestHelper::SetUp(
    const base::FilePath& base_dir,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy) {
  file_system_context_ = CreateFileSystemContextForTesting(
      std::move(quota_manager_proxy), base_dir);

  SetUpFileSystem();
}

void SandboxFileSystemTestHelper::TearDown() {
  file_system_context_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

base::FilePath SandboxFileSystemTestHelper::GetStorageKeyRootPath() {
  return file_system_context_->sandbox_delegate()
      ->GetBaseDirectoryForStorageKeyAndType(storage_key(), type_, false);
}

base::FilePath SandboxFileSystemTestHelper::GetLocalPath(
    const base::FilePath& path) {
  DCHECK(file_util_);
  base::FilePath local_path;
  std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
  file_util_->GetLocalFilePath(context.get(), CreateURL(path), &local_path);
  return local_path;
}

base::FilePath SandboxFileSystemTestHelper::GetLocalPathFromASCII(
    const std::string& path) {
  return GetLocalPath(base::FilePath().AppendASCII(path));
}

base::FilePath SandboxFileSystemTestHelper::GetUsageCachePath() const {
  return file_system_context_->sandbox_delegate()
      ->GetUsageCachePathForStorageKeyAndType(storage_key(), type_);
}

FileSystemURL SandboxFileSystemTestHelper::CreateURL(
    const base::FilePath& path) const {
  return file_system_context_->CreateCrackedFileSystemURL(storage_key(), type_,
                                                          path);
}

int64_t SandboxFileSystemTestHelper::GetCachedStorageKeyUsage() const {
  return file_system_context_->GetQuotaUtil(type_)
      ->GetStorageKeyUsageOnFileTaskRunner(file_system_context_.get(),
                                           storage_key(), type_);
}

int64_t SandboxFileSystemTestHelper::ComputeCurrentStorageKeyUsage() {
  usage_cache()->CloseCacheFiles();

  int64_t size =
      file_util_delegate()->ComputeDirectorySize(GetStorageKeyRootPath());
  if (file_util_delegate()->PathExists(GetUsageCachePath()))
    size -= FileSystemUsageCache::kUsageFileSize;

  return size;
}

int64_t SandboxFileSystemTestHelper::ComputeCurrentDirectoryDatabaseUsage() {
  return file_util_delegate()->ComputeDirectorySize(
      GetStorageKeyRootPath().AppendASCII("Paths"));
}

FileSystemOperationRunner* SandboxFileSystemTestHelper::operation_runner() {
  return file_system_context_->operation_runner();
}

std::unique_ptr<FileSystemOperationContext>
SandboxFileSystemTestHelper::NewOperationContext() {
  DCHECK(file_system_context_.get());
  auto context =
      std::make_unique<FileSystemOperationContext>(file_system_context_.get());
  context->set_update_observers(
      *file_system_context_->GetUpdateObservers(type_));
  return context;
}

void SandboxFileSystemTestHelper::AddFileChangeObserver(
    FileChangeObserver* observer) {
  file_system_context_->sandbox_delegate()->AddFileChangeObserver(
      type_, observer, nullptr);
}

void SandboxFileSystemTestHelper::AddFileUpdateObserver(
    FileUpdateObserver* observer) {
  file_system_context_->sandbox_delegate()->AddFileUpdateObserver(
      type_, observer, nullptr);
}

FileSystemUsageCache* SandboxFileSystemTestHelper::usage_cache() {
  return file_system_context()->sandbox_delegate()->usage_cache();
}

ObfuscatedFileUtilDelegate* SandboxFileSystemTestHelper::file_util_delegate() {
  return file_system_context_->sandbox_delegate()
      ->obfuscated_file_util()
      ->delegate();
}

void SandboxFileSystemTestHelper::SetUpFileSystem() {
  DCHECK(file_system_context_.get());
  DCHECK(file_system_context_->sandbox_backend()->CanHandleType(type_));

  file_util_ = file_system_context_->sandbox_delegate()->sync_file_util();
  DCHECK(file_util_);

  // Prepare the origin's root directory.
  file_system_context_->sandbox_delegate()
      ->GetBaseDirectoryForStorageKeyAndType(storage_key(), type_,
                                             true /* create */);

  base::FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    usage_cache()->UpdateUsage(usage_cache_path, 0);
}

}  // namespace storage
