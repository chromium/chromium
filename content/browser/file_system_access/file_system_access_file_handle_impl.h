// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_

#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_capacity_allocation_host.mojom.h"
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
  void Move(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                destination_directory,
            const std::string& new_entry_name,
            MoveCallback callback) override;
  void Rename(const std::string& new_entry_name,
              RenameCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void OpenAccessHandle(OpenAccessHandleCallback callback) override;
  void IsSameEntry(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      IsSameEntryCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;
  void GetUniqueId(GetUniqueIdCallback callback) override;

  void set_max_swap_files_for_testing(int max) { max_swap_files_ = max; }
  storage::FileSystemURL get_swap_url_for_testing(
      const base::FilePath& swap_path) {
    return GetSwapURL(swap_path);
  }

 private:
  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void CreateFileWriterImpl(bool keep_existing_data,
                            bool auto_close,
                            CreateFileWriterCallback callback);
  void DidVerifyHasWritePermissions(bool keep_existing_data,
                                    bool auto_close,
                                    CreateFileWriterCallback callback,
                                    bool can_write);
  storage::FileSystemURL GetSwapURL(const base::FilePath& swap_path);
  void CreateSwapFile(
      int count,
      bool keep_existing_data,
      bool auto_close,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock>,
      CreateFileWriterCallback callback);
  void DidCreateSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      bool keep_existing_data,
      bool auto_close,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DidCopySwapFile(
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DoOpenIncognitoFile(
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      OpenAccessHandleCallback callback);
  void DoOpenFile(
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      OpenAccessHandleCallback callback);
  void DoGetLengthAfterOpenFile(
      OpenAccessHandleCallback callback,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      base::File file,
      base::ScopedClosureRunner on_close_callback);
  void DidOpenFileAndGetLength(
      OpenAccessHandleCallback callback,
      scoped_refptr<FileSystemAccessWriteLockManager::WriteLock> lock,
      base::ScopedClosureRunner on_close_callback,
      std::pair<base::File, base::FileErrorOr<int64_t>> file_and_length);

  void IsSameEntryImpl(IsSameEntryCallback callback,
                       FileSystemAccessTransferTokenImpl* other);

  // A FileWriter will write to a "swap" file until the `Close()` operation is
  // called to swap the file into the target path. For each writer, a new swap
  // file is created. This sets the limit on the number of swap files per
  // handle.
  int max_swap_files_ = 100;

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessFileHandleImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
