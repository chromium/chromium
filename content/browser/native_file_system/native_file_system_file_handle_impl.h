// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom.h"

namespace content {

// This is the browser side implementation of the
// NativeFileSystemFileHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence.
class CONTENT_EXPORT NativeFileSystemFileHandleImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemFileHandle {
 public:
  NativeFileSystemFileHandleImpl(NativeFileSystemManagerImpl* manager,
                                 const BindingContext& context,
                                 const storage::FileSystemURL& url,
                                 const SharedHandleState& handle_state);
  ~NativeFileSystemFileHandleImpl() override;

  // blink::mojom::NativeFileSystemFileHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void AsBlob(AsBlobCallback callback) override;
  void CreateFileWriter(bool keep_existing_data,
                        CreateFileWriterCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> token)
      override;

  void set_max_swap_files_for_testing(int max) { max_swap_files_ = max; }

 private:
  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void CreateFileWriterImpl(bool keep_existing_data,
                            CreateFileWriterCallback callback);
  void CreateSwapFile(int count,
                      bool keep_existing_data,
                      CreateFileWriterCallback callback);
  // |swap_file_system| is set to the isolated file system the swap url was
  // created in (if any) as that file system might be different than the file
  // system |this| was created from.
  void DidCreateSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      storage::IsolatedContext::ScopedFSHandle swap_file_system,
      bool keep_existing_data,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DidCopySwapFile(
      const storage::FileSystemURL& swap_url,
      storage::IsolatedContext::ScopedFSHandle swap_file_system,
      CreateFileWriterCallback callback,
      base::File::Error result);

  // A FileWriter will write to a "swap" file until the `Close()` operation is
  // called to swap the file into the target path. For each writer, a new swap
  // file is created. This sets the limit on the number of swap files per
  // handle.
  int max_swap_files_ = 100;

  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<NativeFileSystemFileHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemFileHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
