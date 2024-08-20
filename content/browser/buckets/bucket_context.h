// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_

#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace blink {
enum class PermissionType;
}

namespace storage {
struct BucketInfo;
}

namespace content {

// An interface that represents an execution context from which a bucket can be
// created and used. This may correlate to a RenderFrame or a worker.
class CONTENT_EXPORT BucketContext {
 public:
  virtual ~BucketContext() = default;

  // Returns the StorageKey for the context.
  virtual blink::StorageKey GetBucketStorageKey() = 0;

  // Checks the permission status for the given type.
  virtual blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) = 0;

  // Used to access CacheStorage for the bucket.
  virtual void BindCacheStorageForBucket(
      const storage::BucketInfo& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) = 0;

  // Returns an OPFS file system associated with `bucket`. An empty
  // `directory_path_components` means that the root directory will be returned.
  virtual void GetSandboxedFileSystemForBucket(
      const storage::BucketInfo& bucket,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) = 0;

  // Returns the `BucketClientInfo` for this context.
  virtual storage::BucketClientInfo GetBucketClientInfo() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
