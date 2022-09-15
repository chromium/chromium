// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class FileSystemContext;
struct BucketLocator;

// All of the public methods of this class are called by the quota manager
// (except for the constructor/destructor).
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemQuotaClient
    : public mojom::QuotaClient {
 public:
  explicit FileSystemQuotaClient(FileSystemContext* file_system_context);
  ~FileSystemQuotaClient() override;

  FileSystemQuotaClient(const FileSystemQuotaClient&) = delete;
  FileSystemQuotaClient& operator=(const FileSystemQuotaClient&) = delete;

  // mojom::QuotaClient methods.
  void GetBucketUsage(const BucketLocator& bucket,
                      GetBucketUsageCallback callback) override;
  void GetStorageKeysForType(blink::mojom::StorageType type,
                             GetStorageKeysForTypeCallback callback) override;
  void DeleteBucketData(const BucketLocator& bucket,
                        DeleteBucketDataCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override;

 private:
  base::SequencedTaskRunner* file_task_runner() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer usage is safe because `file_system_context_` owns this.
  //
  // The FileSystemQuotaClient implementation mints scoped_refptrs from this
  // raw pointer in order to ensure that the FileSystemContext remains alive
  // while tasks are posted to the FileSystemContext's file sequence.
  //
  // So, it would be tempting to use scoped_refptr<FileSystemContext> here.
  // However, using scoped_refptr here creates a cycle, because
  // `file_system_context_` owns this. We could break the cycle in
  // FileSystemContext::Shutdown(), but then we would have to ensure that
  // Shutdown() is called by all FileSystemContext users.
  const raw_ptr<FileSystemContext> file_system_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_CLIENT_H_
