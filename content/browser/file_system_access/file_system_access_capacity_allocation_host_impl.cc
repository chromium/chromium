// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_capacity_allocation_host_impl.h"

#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

FileSystemAccessCapacityAllocationHostImpl::
    FileSystemAccessCapacityAllocationHostImpl(
        FileSystemAccessManagerImpl* manager,
        const storage::FileSystemURL& url,
        base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
        mojo::PendingReceiver<
            blink::mojom::FileSystemAccessCapacityAllocationHost> receiver)
    : manager_(manager), url_(url), receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(manager_);
  // base::Unretained is safe here because this
  // FileSystemAccessCapacityAllocationHostImpl owns `receiver_`. So, the
  // unretained FileSystemAccessCapacityAllocationHostImpl is guaranteed to
  // outlive `receiver_` and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindOnce(
      &FileSystemAccessCapacityAllocationHostImpl::OnReceiverDisconnect,
      base::Unretained(this)));
}

FileSystemAccessCapacityAllocationHostImpl::
    ~FileSystemAccessCapacityAllocationHostImpl() = default;

void FileSystemAccessCapacityAllocationHostImpl::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
}

void FileSystemAccessCapacityAllocationHostImpl::RequestCapacityChange(
    int64_t capacity_delta,
    RequestCapacityChangeCallback callback) {
  // TODO(https://crbug.com/1240056): Complete implementation of the quota
  // integration for Access Handles.
  NOTIMPLEMENTED();
  std::move(callback).Run(capacity_delta);
}
}  // namespace content
