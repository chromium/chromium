// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_

#include "base/memory/checked_ptr.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"

namespace content {

// Browser side implementation of the FileSystemAccessFileDelegateHost mojom
// interface. Instances of this class are owned by the
// FileSystemAccessAccessHandleHostImpl instance of the associated URL, which
// constructs it.
class CONTENT_EXPORT FileSystemAccessFileDelegateHostImpl
    : public blink::mojom::FileSystemAccessFileDelegateHost {
 public:
  FileSystemAccessFileDelegateHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          receiver);
  ~FileSystemAccessFileDelegateHostImpl() override;

 private:
  void OnDisconnect();

  FileSystemAccessManagerImpl* manager() { return manager_; }
  const storage::FileSystemURL& url() { return url_; }

  // This is safe, since the manager owns the
  // FileSystemAccessAccessHandleHostImpl which owns this class.
  const CheckedPtr<FileSystemAccessManagerImpl> manager_;
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessFileDelegateHost> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessFileDelegateHostImpl> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_