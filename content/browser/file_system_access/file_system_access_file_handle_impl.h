// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom.h"

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
  void CreateFileWriter(
      bool keep_existing_data,
      bool auto_close,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
      CreateFileWriterCallback callback) override;
  void Move(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                destination_directory,
            const std::string& new_entry_name,
            MoveCallback callback) override;
  void Rename(const std::string& new_entry_name,
              RenameCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void OpenAccessHandle(blink::mojom::FileSystemAccessAccessHandleLockMode mode,
                        OpenAccessHandleCallback callback) override;
  void IsSameEntry(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      IsSameEntryCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;
  void GetUniqueId(GetUniqueIdCallback callback) override;
  void GetCloudIdentifiers(GetCloudIdentifiersCallback callback) override;

  void set_max_swap_files_for_testing(int max) { max_swap_files_ = max; }
#if BUILDFLAG(IS_MAC)
  void set_swap_file_cloning_will_fail_for_testing() {
    swap_file_cloning_will_fail_for_testing_ = true;
  }
  const std::optional<base::File::Error>&
  get_swap_file_clone_result_for_testing() const {
    return swap_file_clone_result_for_testing_;
  }
#endif  // BUILDFLAG(IS_MAC)

 private:
#if BUILDFLAG(IS_MAC)
  // Returns whether the swap file is eligible to be created using a
  // copy-on-write file. This is only relevant on file systems which support
  // COW, such as APFS.
  bool CanUseCowSwapFile() const;
#endif  // BUILDFLAG(IS_MAC)

  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void CreateFileWriterImpl(
      bool keep_existing_data,
      bool auto_close,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
      CreateFileWriterCallback callback);
  void DidVerifyHasWritePermissions(
      bool keep_existing_data,
      bool auto_close,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
      CreateFileWriterCallback callback,
      bool can_write);
  // Find an unused swap file. We cannot use a swap file which is locked or
  // which already exists on disk.
  void StartCreateSwapFile(
      int start_count,
      bool keep_existing_data,
      bool auto_close,
      CreateFileWriterCallback callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock);
  void DidTakeSwapLock(
      int count,
      const storage::FileSystemURL& swap_url,
      bool keep_existing_data,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      CreateFileWriterCallback callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock);
  void DidCheckSwapFileExists(
      int count,
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void CreateSwapFileFromCopy(
      int count,
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      CreateFileWriterCallback callback);
#if BUILDFLAG(IS_MAC)
  // Attempts to create a swap file using the underlying platform's support for
  // copy-on-write files. This will automatically keep the existing contents of
  // the source file.
  void CreateClonedSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      CreateFileWriterCallback callback);
  void DidCloneSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      CreateFileWriterCallback callback,
      base::File::Error result);
#endif  // BUILDFLAG(IS_MAC)
  void DidCreateSwapFile(
      int count,
      const storage::FileSystemURL& swap_url,
      bool keep_existing_data,
      bool auto_close,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      CreateFileWriterCallback callback,
      base::File::Error result);
  void DidTakeAccessHandleLock(
      OpenAccessHandleCallback callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock);
  void DoOpenIncognitoFile(
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      OpenAccessHandleCallback callback);
  void DoOpenFile(scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
                  OpenAccessHandleCallback callback);
  void DoGetLengthAfterOpenFile(
      OpenAccessHandleCallback callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      base::File file,
      base::ScopedClosureRunner on_close_callback);
  void DidOpenFileAndGetLength(
      OpenAccessHandleCallback callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      base::ScopedClosureRunner on_close_callback,
      std::pair<base::File, base::FileErrorOr<int64_t>> file_and_length);

  void IsSameEntryImpl(IsSameEntryCallback callback,
                       FileSystemAccessTransferTokenImpl* other);

  // A FileWriter will write to a "swap" file until the `Close()` operation is
  // called to swap the file into the target path. For each writer, a new swap
  // file is created. This sets the limit on the number of swap files per
  // handle.
  int max_swap_files_ = 100;

#if BUILDFLAG(IS_ANDROID)
  // Directory in app cache dir for writing swap files.
  base::FilePath swap_dir_;
#endif

#if BUILDFLAG(IS_MAC)
  // Used to test that swap file creation attempts to use file cloning in some
  // circumstances, and gracefully handles file cloning errors.
  bool swap_file_cloning_will_fail_for_testing_ = false;
  std::optional<base::File::Error> swap_file_clone_result_for_testing_ =
      std::nullopt;
#endif  // BUILDFLAG(IS_MAC)

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessFileHandleImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_HANDLE_IMPL_H_
