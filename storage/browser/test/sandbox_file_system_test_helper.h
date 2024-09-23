// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_
#define STORAGE_BROWSER_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_

#include <stdint.h>

#include <string>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemOperationRunner;
class ObfuscatedFileUtilDelegate;
class QuotaManagerProxy;
}  // namespace storage

namespace storage {

// Filesystem test helper class that encapsulates test environment for
// a given {StorageKey, (optional) BucketLocator, type} pair.  This helper only
// works for sandboxed file systems (Temporary or Persistent).
class SandboxFileSystemTestHelper {
 public:
  SandboxFileSystemTestHelper(const blink::StorageKey& storage_key,
                              FileSystemType type);
  SandboxFileSystemTestHelper();
  ~SandboxFileSystemTestHelper();

  void SetUp(const base::FilePath& base_dir);
  // If you want to use more than one SandboxFileSystemTestHelper in
  // a single base directory, they have to share a context, so that they don't
  // have multiple databases fighting over the lock to the origin directory
  // [deep down inside ObfuscatedFileUtil].
  void SetUp(scoped_refptr<FileSystemContext> file_system_context);
  void SetUp(scoped_refptr<FileSystemContext> file_system_context,
             const BucketLocator& bucket_locator);
  void SetUp(const base::FilePath& base_dir,
             scoped_refptr<QuotaManagerProxy> quota_manager_proxy);
  void TearDown();

  base::FilePath GetRootPath();
  base::FilePath GetLocalPath(const base::FilePath& path);
  base::FilePath GetLocalPathFromASCII(const std::string& path);

  // Returns empty path if filesystem type is neither temporary nor persistent.
  base::FileErrorOr<base::FilePath> GetUsageCachePath() const;

  FileSystemURL CreateURL(const base::FilePath& path) const;
  FileSystemURL CreateURLFromUTF8(const std::string& utf8) const {
    return CreateURL(base::FilePath::FromUTF8Unsafe(utf8));
  }

  // This returns cached usage size returned by QuotaUtil.
  int64_t GetCachedUsage() const;

  // This doesn't work with OFSFU.
  int64_t ComputeCurrentStorageKeyUsage();

  int64_t ComputeCurrentDirectoryDatabaseUsage();

  FileSystemOperationRunner* operation_runner();
  std::unique_ptr<FileSystemOperationContext> NewOperationContext();

  void AddFileChangeObserver(FileChangeObserver* observer);
  void AddFileUpdateObserver(FileUpdateObserver* observer);

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  const blink::StorageKey& storage_key() const {
    return bucket_locator_.storage_key;
  }

  FileSystemType type() const { return type_; }
  blink::mojom::StorageType storage_type() const {
    return FileSystemTypeToQuotaStorageType(type_);
  }
  FileSystemFileUtil* file_util() const { return file_util_; }
  FileSystemUsageCache* usage_cache();

  ObfuscatedFileUtilDelegate* file_util_delegate();

 private:
  void SetUpFileSystem();

  scoped_refptr<FileSystemContext> file_system_context_;
  BucketLocator bucket_locator_;

  const FileSystemType type_;
  raw_ptr<FileSystemFileUtil, DanglingUntriaged> file_util_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_
