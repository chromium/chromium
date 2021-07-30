// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_

#include <memory>

#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessHandleHost mojom interface. Instances of this class are
// owned by the FileSystemAccessManagerImpl instance passed in to the
// constructor.
class CONTENT_EXPORT FileSystemAccessAccessHandleHostImpl
    : public blink::mojom::FileSystemAccessAccessHandleHost {
 public:
  // Crates an AccessHandleHost that acts as an exclusive write lock on the
  // file. AccessHandleHosts should only be created via the
  // FileSystemAccessManagerImpl.
  FileSystemAccessAccessHandleHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessManagerImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessAccessHandleHost>
          receiver,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          file_delegate_receiver);
  FileSystemAccessAccessHandleHostImpl(
      const FileSystemAccessAccessHandleHostImpl&) = delete;
  FileSystemAccessAccessHandleHostImpl& operator=(
      const FileSystemAccessAccessHandleHostImpl&) = delete;
  ~FileSystemAccessAccessHandleHostImpl() override;

  const storage::FileSystemURL& url() const { return url_; }

  // blink::mojom::FileSystemAccessFileHandleHost:
  void Close(CloseCallback callback) override;

 private:
  // If the mojo pipe is severed before Close() is invoked, the lock will be
  // released from the OnDisconnect method.
  void OnDisconnect();

  // The FileSystemAccessManagerImpl that owns this instance.
  FileSystemAccessManagerImpl* const manager_;

  // URL of the file associated with this handle. It is used to unlock the
  // exclusive write lock on closure/destruction.
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessAccessHandleHost> receiver_;

  std::unique_ptr<FileSystemAccessFileDelegateHostImpl> incognito_host_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ACCESS_HANDLE_HOST_IMPL_H_
