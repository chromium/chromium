// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_IO_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_IO_CONTEXT_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/common/content_export.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

// Represents the per-BrowserContext NativeIOContext data.
//
// This class is RefCountedDeleteOnSequence because it has members that must be
// accessed on the IO thread, and therefore must be destroyed on the IO thread.
// Conceptually, NativeIOContext instances are owned by StoragePartition.
class CONTENT_EXPORT NativeIOContext
    : public base::RefCountedDeleteOnSequence<NativeIOContext> {
 public:
  NativeIOContext();

  NativeIOContext(const NativeIOContext&) = delete;
  NativeIOContext& operator=(const NativeIOContext&) = delete;

  // Removes an storage key's data and closes any open files. Must be called on
  // the UI thread.
  virtual void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback) = 0;

  // Returns the usage in bytes for all storage keys. Must be called on the
  // UI thread.
  virtual void GetStorageKeyUsageMap(
      base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
          callback) = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<NativeIOContext>;
  friend class base::DeleteHelper<NativeIOContext>;
  virtual ~NativeIOContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_IO_CONTEXT_H_
