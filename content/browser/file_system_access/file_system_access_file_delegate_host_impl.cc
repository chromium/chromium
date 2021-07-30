// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

FileSystemAccessFileDelegateHostImpl::FileSystemAccessFileDelegateHostImpl(
    FileSystemAccessManagerImpl* manager,
    const storage::FileSystemURL& url,
    base::PassKey<FileSystemAccessAccessHandleHostImpl> /*pass_key*/,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
        receiver)
    : manager_(manager), url_(url), receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(manager_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&FileSystemAccessFileDelegateHostImpl::OnDisconnect,
                     base::Unretained(this)));
}

FileSystemAccessFileDelegateHostImpl::~FileSystemAccessFileDelegateHostImpl() =
    default;

void FileSystemAccessFileDelegateHostImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
}

}  // namespace content
