// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_IMPL_H_

#include "base/files/file_path.h"
#include "base/thread_annotations.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/native_io_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class NativeIOManager;

// Helper object on the UI thread whose sole responsibility is to maintain a
// NativeIOManager on the IO thread, where it can be called by the QuotaClient.
class NativeIOContextImpl : public NativeIOContext {
 public:
  // Creates an empty NativeIOContextImpl shell.
  //
  // Newly created instances must be initialized via Initialize() before any
  // other methods are used.
  NativeIOContextImpl();

  NativeIOContextImpl(const NativeIOContextImpl&) = delete;
  NativeIOContextImpl& operator=(const NativeIOContextImpl&) = delete;

  // Creates the underlying NativeIOManager.
  //
  // Must be called on the UI thread.
  void Initialize(
      const base::FilePath& profile_root,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  // Binds `receiver` to the NativeIOHost serving `origin`.
  //
  // Must be called on the UI thread.
  void BindReceiver(const blink::StorageKey& storage_key,
                    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver);

  // Deletes all data stored for storage key `storage_key`.
  //
  // Must be called on the UI thread.
  void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback) override;

  // Returns the usage in bytes for all storage keys.
  //
  // Must be called on the UI thread.
  void GetStorageKeyUsageMap(
      base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
          callback) override;

 protected:
  ~NativeIOContextImpl() override;

 private:
  void InitializeOnIOThread(
      const base::FilePath& profile_root,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  void BindReceiverOnIOThread(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
      mojo::ReportBadMessageCallback bad_message_callback);

  void DeleteStorageKeyDataOnIOThread(
      const blink::StorageKey& storage_key,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);

  void GetStorageKeyUsageMapOnIOThread(
      base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
          callback);

  // Only to be accessed on the IO thread.
  std::unique_ptr<NativeIOManager> native_io_manager_;

#if DCHECK_IS_ON()
  // Only accessed on the UI thread.
  bool initialize_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_IMPL_H_
