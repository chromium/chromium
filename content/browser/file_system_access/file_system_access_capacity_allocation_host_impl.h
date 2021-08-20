// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CAPACITY_ALLOCATION_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CAPACITY_ALLOCATION_HOST_IMPL_H_

#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_capacity_allocation_host.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessCapacityAllocationHost mojom interface. Instances of this
// class are owned by the FileSystemAccessHandleHost instance passed in to the
// constructor.
class CONTENT_EXPORT FileSystemAccessCapacityAllocationHostImpl
    : public blink::mojom::FileSystemAccessCapacityAllocationHost {
 public:
  // Crates a FileSystemAccessCapacityAllocationHost that manages capacity
  // reservations for the file. CapacityAllocationHosts should only be created
  // via the FileSystemAccessHandleHost.
  FileSystemAccessCapacityAllocationHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
      mojo::PendingReceiver<
          blink::mojom::FileSystemAccessCapacityAllocationHost> receiver);

  FileSystemAccessCapacityAllocationHostImpl(
      const FileSystemAccessCapacityAllocationHostImpl&) = delete;
  FileSystemAccessCapacityAllocationHostImpl& operator=(
      const FileSystemAccessCapacityAllocationHostImpl&) = delete;

  ~FileSystemAccessCapacityAllocationHostImpl() override;

  // blink::mojom::FileSystemAccessCapacityAllocationHost:
  void RequestCapacityChange(int64_t capacity_delta,
                             RequestCapacityChangeCallback callback) override;

 private:
  // Called when the receiver is disconnected.
  void OnReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer use is safe, since the manager owns the
  // FileSystemAccessAccessHandleHostImpl which owns this class.
  FileSystemAccessManagerImpl* const manager_;

  // URL of the file whose capacity is managed through this host.
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessCapacityAllocationHost>
      receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CAPACITY_ALLOCATION_HOST_IMPL_H_
