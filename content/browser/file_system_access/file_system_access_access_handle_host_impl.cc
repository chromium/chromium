// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_access_handle_host_impl.h"

namespace content {

FileSystemAccessAccessHandleHostImpl::FileSystemAccessAccessHandleHostImpl(
    FileSystemAccessManagerImpl* manager,
    const storage::FileSystemURL& url,
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessAccessHandleHost>
        receiver)
    : manager_(manager), url_(url), receiver_(this, std::move(receiver)) {
  DCHECK(manager_);

  receiver_.set_disconnect_handler(
      base::BindOnce(&FileSystemAccessAccessHandleHostImpl::OnDisconnect,
                     base::Unretained(this)));
}

FileSystemAccessAccessHandleHostImpl::~FileSystemAccessAccessHandleHostImpl() =
    default;

void FileSystemAccessAccessHandleHostImpl::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
  receiver_.reset();
  manager_->RemoveAccessHandleHost(url_);
}

void FileSystemAccessAccessHandleHostImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.reset();
  manager_->RemoveAccessHandleHost(url_);
}

}  // namespace content
