// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessFileHandle mojom interface. Instances of this class are
// owned by the FileSystemAccessManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence.
class CONTENT_EXPORT FileSystemAccessFileHandleImpl
    : public FileSystemAccessHandleBase,
      public blink::mojom::FileSystemAccessFileHandle {
 public:
  FileSystemAccessFileHandleImpl(FileSystemAccessManagerImpl* manager,
                                 const BindingContext& context,
                                 const storage::FileSystemURL& url,
                                 const SharedHandleState& handle_state);
  FileSystemAccessFileHandleImpl(const FileSystemAccessFileHandleImpl&) =
      delete;
  FileSystemAccessFileHandleImpl& operator=(
      const FileSystemAccessFileHandleImpl&) = delete;
  ~FileSystemAccessFileHandleImpl() override;

  // blink::mojom::FileSystemAccessFileHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void AsBlob(AsBlobCallback callback) override;
  void CreateFileWriter(bool keep_existing_data,
                        bool auto_close,
                        CreateFileWriterCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void OpenAccessHandle(OpenAccessHandleCallback callback) override;
  void IsSameEntry(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      IsSameEntryCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;

  void set_max_swap_files_for_testing(int max) { max_swap_files_ = max; }

 private:
  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void CreateFileWriterImpl(bool keep_existing_data,
                            bool auto_close,
                            CreateFileWriterCallback callback);
  void CreateSwapFile(int count,
                      bool keep_existing_data,
                      bool auto_close,
                      CreateFileWriterCallback callback);
  // |swap_file_system| is set to the isolated file system the swap url was
  // created in (if any) as that file system might be different than the file
  // system |this| was created from.
  void DidCreateSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      bool keep_existing_data,
      bool auto_close,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DidCopySwapFile(
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DoOpenIncognitoFile(
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
          access_handle_host_remote,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileDelegateHost>
          file_delegate_host_remote,
      OpenAccessHandleCallback callback);
  void DoOpenFile(
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
          access_handle_host_remote,
      OpenAccessHandleCallback callback);

  void DidOpenFile(
      OpenAccessHandleCallback callback,
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
          access_handle_host_remote,
      base::File file,
      base::OnceClosure on_close_callback);

  void IsSameEntryImpl(IsSameEntryCallback callback,
                       FileSystemAccessTransferTokenImpl* other);

  // A FileWriter will write to a "swap" file until the `Close()` operation is
  // called to swap the file into the target path. For each writer, a new swap
  // file is created. This sets the limit on the number of swap files per
  // handle.
  int max_swap_files_ = 100;

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessFileHandleImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
