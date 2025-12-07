// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_access_handle_host_impl.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"
#include "content/browser/file_system_access/file_system_access_file_modification_host_impl.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

FileSystemAccessAccessHandleHostImpl::FileSystemAccessAccessHandleHostImpl(
    FileSystemAccessManagerImpl* manager,
    const storage::FileSystemURL& url,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessAccessHandleHost>
        receiver,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
        file_delegate_receiver,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileModificationHost>
        file_modification_host_receiver,
    int64_t file_size,
    base::ScopedClosureRunner on_close_callback)
    : manager_(manager),
      receiver_(this, std::move(receiver)),
      url_(url),
      on_close_callback_(std::move(on_close_callback)),
      lock_(std::move(lock)) {
  DCHECK(manager_);

  DCHECK(manager_->context()->is_incognito() ==
         file_delegate_receiver.is_valid());

  // Only create a file delegate host in incognito mode.
  incognito_host_ =
      manager_->context()->is_incognito()
          ? std::make_unique<FileSystemAccessFileDelegateHostImpl>(
                manager_, url_,
                base::PassKey<FileSystemAccessAccessHandleHostImpl>(),
                std::move(file_delegate_receiver))
          : nullptr;

  // Only create a file modification host in non-incognito mode.
  file_modification_host_ =
      !manager_->context()->is_incognito()
          ? std::make_unique<FileSystemAccessFileModificationHostImpl>(
                manager_, url_,
                base::PassKey<FileSystemAccessAccessHandleHostImpl>(),
                std::move(file_modification_host_receiver), file_size)
          : nullptr;

  receiver_.set_disconnect_handler(
      base::BindOnce(&FileSystemAccessAccessHandleHostImpl::OnDisconnect,
                     base::Unretained(this)));
}

FileSystemAccessAccessHandleHostImpl::~FileSystemAccessAccessHandleHostImpl() =
    default;

void FileSystemAccessAccessHandleHostImpl::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (close_callback_) {
    receiver_.ReportBadMessage("Close already called on SyncAccessHandle.");
    return;
  }

  // Run `callback` when this instance is destroyed, after file modification
  // host has been released.
  close_callback_ = base::ScopedClosureRunner(std::move(callback));

  // Removes `this`.
  manager_->RemoveAccessHandleHost(this);
}

void FileSystemAccessAccessHandleHostImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (close_callback_) {
    // A call has already been made to
    // `FileSystemAccessManagerImpl::RemoveAccessHandleHost`.
    return;
  }

  // No need to reset `receiver_` after it disconnected.
  // Removes `this`.
  manager_->RemoveAccessHandleHost(this);
}

}  // namespace content
