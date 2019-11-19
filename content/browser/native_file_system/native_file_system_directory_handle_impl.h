// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"

namespace content {
// This is the browser side implementation of the
// NativeFileSystemDirectoryHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence.
class NativeFileSystemDirectoryHandleImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemDirectoryHandle {
 public:
  NativeFileSystemDirectoryHandleImpl(NativeFileSystemManagerImpl* manager,
                                      const BindingContext& context,
                                      const storage::FileSystemURL& url,
                                      const SharedHandleState& handle_state);
  ~NativeFileSystemDirectoryHandleImpl() override;

  // blink::mojom::NativeFileSystemDirectoryHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void GetFile(const std::string& basename,
               bool create,
               GetFileCallback callback) override;
  void GetDirectory(const std::string& basename,
                    bool create,
                    GetDirectoryCallback callback) override;
  void GetEntries(mojo::PendingRemote<
                  blink::mojom::NativeFileSystemDirectoryEntriesListener>
                      pending_listener) override;
  void RemoveEntry(const std::string& basename,
                   bool recurse,
                   RemoveEntryCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> token)
      override;

 private:
  // This method creates the file if it does not currently exists. I.e. it is
  // the implementation for passing create=true to GetFile.
  void GetFileWithWritePermission(const storage::FileSystemURL& child_url,
                                  GetFileCallback callback);
  void DidGetFile(const storage::FileSystemURL& url,
                  GetFileCallback callback,
                  base::File::Error result);
  // This method creates the directory if it does not currently exists. I.e. it
  // is the implementation for passing create=true to GetDirectory.
  void GetDirectoryWithWritePermission(const storage::FileSystemURL& child_url,
                                       GetDirectoryCallback callback);
  void DidGetDirectory(const storage::FileSystemURL& url,
                       GetDirectoryCallback callback,
                       base::File::Error result);
  void DidReadDirectory(
      mojo::Remote<blink::mojom::NativeFileSystemDirectoryEntriesListener>*
          listener,
      base::File::Error result,
      std::vector<filesystem::mojom::DirectoryEntry> file_list,
      bool has_more_entries);

  void RemoveEntryImpl(const storage::FileSystemURL& url,
                       bool recurse,
                       RemoveEntryCallback callback);

  // Calculates a FileSystemURL for a (direct) child of this directory with the
  // given basename.  Returns an error when |basename| includes invalid input
  // like "/".
  blink::mojom::NativeFileSystemErrorPtr GetChildURL(
      const std::string& basename,
      storage::FileSystemURL* result) WARN_UNUSED_RESULT;

  // Helper to create a blink::mojom::NativeFileSystemEntry struct.
  blink::mojom::NativeFileSystemEntryPtr CreateEntry(
      const std::string& basename,
      const storage::FileSystemURL& url,
      bool is_directory);

  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<NativeFileSystemDirectoryHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemDirectoryHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_IMPL_H_
