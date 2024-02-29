// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_MODIFICATION_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_MODIFICATION_HOST_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content {

class FileSystemAccessFileModificationHostImplTest;

// This is the browser side implementation of the
// FileSystemAccessFileModificationHost mojom interface. Instances of this
// class are owned by the FileSystemAccessHandleHost instance passed in to the
// constructor.
class CONTENT_EXPORT FileSystemAccessFileModificationHostImpl
    : public blink::mojom::FileSystemAccessFileModificationHost {
 public:
  // Creates a FileSystemAccessFileModificationHost that manages capacity
  // reservations for the file. FileModificationHosts should only be created
  // via the FileSystemAccessHandleHost.
  FileSystemAccessFileModificationHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileModificationHost>
          receiver,
      int64_t file_size);

  // Constructor for testing
  FileSystemAccessFileModificationHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessFileModificationHostImplTest> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileModificationHost>
          receiver,
      int64_t file_size);

  FileSystemAccessFileModificationHostImpl(
      const FileSystemAccessFileModificationHostImpl&) = delete;
  FileSystemAccessFileModificationHostImpl& operator=(
      const FileSystemAccessFileModificationHostImpl&) = delete;

  ~FileSystemAccessFileModificationHostImpl() override;

  // blink::mojom::FileSystemAccessFileModificationHost:
  void RequestCapacityChange(int64_t capacity_delta,
                             RequestCapacityChangeCallback callback) override;
  void OnContentsModified() override;

  // Returns the the total space allocated for the file whose capacity is
  // managed through this host. Initially, this is the file's size.
  int64_t granted_capacity() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return granted_capacity_;
  }

 private:
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return manager_->context()->quota_manager_proxy();
  }

  // Called when the receiver is disconnected.
  void OnReceiverDisconnect();

  void DidGetUsageAndQuota(int64_t capacity_delta,
                           RequestCapacityChangeCallback callback,
                           blink::mojom::QuotaStatusCode status,
                           int64_t usage,
                           int64_t quota);

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer use is safe, since the manager owns the
  // FileSystemAccessAccessHandleHostImpl which owns this class.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;

  // URL of the file whose capacity is managed through this host.
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessFileModificationHost> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Total capacity granted to the file managed through this host. Initially,
  // this is the file's size. Later, this value is modified through mojo calls
  // reaching `RequestCapacityChange()`.
  int64_t granted_capacity_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessFileModificationHostImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_MODIFICATION_HOST_IMPL_H_
