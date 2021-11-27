// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/file_system_access/file_system_access_capacity_allocation_host_impl.h"
#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessHandleHost mojom interface. Instances of this class are
// owned by the FileSystemAccessManagerImpl instance passed in to the
// constructor.
class FileSystemAccessAccessHandleHostImpl
    : public blink::mojom::FileSystemAccessAccessHandleHost {
 public:
  // Crates an AccessHandleHost that acts as an exclusive write lock on the
  // file. AccessHandleHosts should only be created via the
  // FileSystemAccessManagerImpl.
  FileSystemAccessAccessHandleHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      base::PassKey<FileSystemAccessManagerImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessAccessHandleHost>
          receiver,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          file_delegate_receiver,
      mojo::PendingReceiver<
          blink::mojom::FileSystemAccessCapacityAllocationHost>
          capacity_allocation_host_receiver,
      int64_t file_size);
  FileSystemAccessAccessHandleHostImpl(
      const FileSystemAccessAccessHandleHostImpl&) = delete;
  FileSystemAccessAccessHandleHostImpl& operator=(
      const FileSystemAccessAccessHandleHostImpl&) = delete;
  ~FileSystemAccessAccessHandleHostImpl() override;

  // blink::mojom::FileSystemAccessFileHandleHost:
  void Close(CloseCallback callback) override;

  // Returns the the total capacity allocated for the file whose capacity is
  // managed through this host.
  int64_t granted_capacity() const {
    DCHECK(capacity_allocation_host_)
        << "Capacity allocation requires a CapacityAllocationHost";
    return capacity_allocation_host_->granted_capacity();
  }

  storage::FileSystemURL url() const { return url_; }

 private:
  // If the mojo pipe is severed before Close() is invoked, the lock will be
  // released from the OnDisconnect method.
  void OnDisconnect();

  // The FileSystemAccessManagerImpl that owns this instance.
  const raw_ptr<FileSystemAccessManagerImpl> manager_;

  // Exclusive write lock on the file. It is released on destruction.
  scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock_;

  mojo::Receiver<blink::mojom::FileSystemAccessAccessHandleHost> receiver_;

  std::unique_ptr<FileSystemAccessFileDelegateHostImpl> incognito_host_;

  // Manages capacity allocations for the file managed through this host.
  // This variable is only initialized for non-incognito contexts.
  //
  // Non-incognito file I/O operations on Access Handles are performed in the
  // renderer process. Before increasing a file's size, the renderer must
  // request additional capacity from the
  // FileSystemAccessCapacityAllocationHostImpl. The host grants capacity if the
  // quota management system allows it. From the browser's perspective, all
  // granted capacity is fully used by the file.
  //
  // When the Access Handle closes, the browser must clean up the discrepancy
  // between the perceived file size, as reported by `granted_capacity()`, and
  // the actual file size on disk. This step is
  // performed by the FileSystemAccessManagerImpl owning this host.
  std::unique_ptr<FileSystemAccessCapacityAllocationHostImpl>
      capacity_allocation_host_;

  const storage::FileSystemURL url_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_
