// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/thread_annotations.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

class NativeIOManager;

// Helper object on the UI thread whose sole responsibility is to maintain a
// NativeIOManager on the IO thread, where it can be called by the QuotaClient.
//
// This class is RefCountedDeleteOnSequence because it has members that must be
// accessed on the IO thread, and therefore must be destroyed on the IO thread.
// Conceptually, NativeIOContext instances are owned by StoragePartitionImpl.
class CONTENT_EXPORT NativeIOContext
    : public base::RefCountedDeleteOnSequence<NativeIOContext> {
 public:
  // Creates an empty NativeIOContext shell.
  //
  // Newly created instances must be initialized via Initialize() before any
  // other methods are used.
  NativeIOContext();

  NativeIOContext(const NativeIOContext&) = delete;
  NativeIOContext& operator=(const NativeIOContext&) = delete;

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
  void BindReceiver(const url::Origin& origin,
                    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver);

 private:
  friend class base::RefCountedDeleteOnSequence<NativeIOContext>;
  friend class base::DeleteHelper<NativeIOContext>;

  ~NativeIOContext();

  void InitializeOnIOThread(
      const base::FilePath& profile_root,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  void BindReceiverOnIOThread(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver);

  // Only to be accessed on the IO thread.
  std::unique_ptr<NativeIOManager> native_io_manager_;

#if DCHECK_IS_ON()
  // Only accessed on the UI thread.
  bool initialize_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_CONTEXT_H_
