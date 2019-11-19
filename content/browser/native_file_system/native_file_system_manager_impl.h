// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_file_system_entry_factory.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_writer.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {
class NativeFileSystemFileHandleImpl;
class NativeFileSystemDirectoryHandleImpl;
class NativeFileSystemTransferTokenImpl;
class StoragePartitionImpl;

// This is the browser side implementation of the
// NativeFileSystemManager mojom interface. This is the main entry point for
// the native file system API in the browser process.Instances of this class are
// owned by StoragePartitionImpl.
//
// This class owns all the NativeFileSystemFileHandleImpl,
// NativeFileSystemDirectoryHandleImpl and NativeFileSystemTransferTokenImpl
// instances for a specific storage partition.
//
// This class is not thread safe, it must be constructed and used on the UI
// thread only.
class CONTENT_EXPORT NativeFileSystemManagerImpl
    : public NativeFileSystemEntryFactory,
      public blink::mojom::NativeFileSystemManager {
 public:
  using BindingContext = NativeFileSystemEntryFactory::BindingContext;

  // State that is shared between handles that are derived from each other.
  // Handles that are created through ChooseEntries or GetSandboxedFileSystem
  // get new values for these properties, while any handles derived from those
  // (i.e. children of a directory) will inherit these properties from their
  // parent.
  struct CONTENT_EXPORT SharedHandleState {
    SharedHandleState(
        scoped_refptr<NativeFileSystemPermissionGrant> read_grant,
        scoped_refptr<NativeFileSystemPermissionGrant> write_grant,
        storage::IsolatedContext::ScopedFSHandle file_system);
    SharedHandleState(const SharedHandleState& other);
    ~SharedHandleState();

    // Should never be null. These are the read and write permissions for this
    // handle.
    const scoped_refptr<NativeFileSystemPermissionGrant> read_grant;
    const scoped_refptr<NativeFileSystemPermissionGrant> write_grant;
    // Can be empty, if this handle is not backed by an isolated file system.
    const storage::IsolatedContext::ScopedFSHandle file_system;
  };

  // The caller is responsible for ensuring that |permission_context| outlives
  // this instance.
  NativeFileSystemManagerImpl(
      scoped_refptr<storage::FileSystemContext> context,
      scoped_refptr<ChromeBlobStorageContext> blob_context,
      NativeFileSystemPermissionContext* permission_context,
      bool off_the_record);

  void BindReceiver(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver);

  // blink::mojom::NativeFileSystemManager:
  void GetSandboxedFileSystem(GetSandboxedFileSystemCallback callback) override;
  void ChooseEntries(
      blink::mojom::ChooseFileSystemEntryType type,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ChooseEntriesCallback callback) override;
  void GetFileHandleFromToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemFileHandle>
          file_handle_receiver) override;
  void GetDirectoryHandleFromToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemDirectoryHandle>
          directory_handle_receiver) override;

  // NativeFileSystemEntryFactory:
  blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& file_path) override;
  blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& directory_path) override;

  // Same as CreateFileEntryFromPath, except informs the permission context that
  // the returned entry should be writable, because this entry was the result of
  // a "save" operation.
  blink::mojom::NativeFileSystemEntryPtr CreateWritableFileEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& file_path);

  // Creates a new NativeFileSystemFileHandleImpl for a given url. Assumes the
  // passed in URL is valid and represents a file.
  mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle>
  CreateFileHandle(const BindingContext& binding_context,
                   const storage::FileSystemURL& url,
                   const SharedHandleState& handle_state);

  // Creates a new NativeFileSystemDirectoryHandleImpl for a given url. Assumes
  // the passed in URL is valid and represents a directory.
  mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle>
  CreateDirectoryHandle(const BindingContext& context,
                        const storage::FileSystemURL& url,
                        const SharedHandleState& handle_state);

  // Creates a new NativeFileSystemFileWriterImpl for a given target and
  // swap file URLs. Assumes the passed in URLs are valid and represent files.
  mojo::PendingRemote<blink::mojom::NativeFileSystemFileWriter>
  CreateFileWriter(const BindingContext& binding_context,
                   const storage::FileSystemURL& url,
                   const storage::FileSystemURL& swap_url,
                   const SharedHandleState& handle_state);

  // Create a transfer token for a specific file or directory.
  void CreateTransferToken(
      const NativeFileSystemFileHandleImpl& file,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  void CreateTransferToken(
      const NativeFileSystemDirectoryHandleImpl& directory,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);

  // Given a mojom transfer token, looks up the token in our internal list of
  // valid tokens. Calls the callback with the found token, or nullptr if no
  // valid token was found.
  using ResolvedTokenCallback =
      base::OnceCallback<void(NativeFileSystemTransferTokenImpl*)>;
  void ResolveTransferToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
      ResolvedTokenCallback callback);

  void DidResolveTransferTokenForFileHandle(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemFileHandle>
          file_handle_receiver,
      NativeFileSystemTransferTokenImpl* resolved_token);
  void DidResolveTransferTokenForDirectoryHandle(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemDirectoryHandle>
          directory_handle_receiver,
      NativeFileSystemTransferTokenImpl* resolved_token);

  storage::FileSystemContext* context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return context_.get();
  }
  ChromeBlobStorageContext* blob_context() { return blob_context_.get(); }
  const base::SequenceBound<storage::FileSystemOperationRunner>&
  operation_runner();

  NativeFileSystemPermissionContext* permission_context() {
    return permission_context_;
  }

  bool is_off_the_record() const { return off_the_record_; }

  void SetPermissionContextForTesting(
      NativeFileSystemPermissionContext* permission_context) {
    permission_context_ = permission_context;
  }

  // Remove |token| from |transfer_tokens_|. It is an error to try to remove a
  // token that doesn't exist.
  void RemoveToken(const base::UnguessableToken& token);

 private:
  friend class NativeFileSystemFileHandleImpl;

  ~NativeFileSystemManagerImpl() override;
  void DidOpenSandboxedFileSystem(const BindingContext& binding_context,
                                  GetSandboxedFileSystemCallback callback,
                                  const GURL& root,
                                  const std::string& filesystem_name,
                                  base::File::Error result);

  void DidChooseEntries(const BindingContext& binding_context,
                        const FileSystemChooser::Options& options,
                        ChooseEntriesCallback callback,
                        blink::mojom::NativeFileSystemErrorPtr result,
                        std::vector<base::FilePath> entries);
  void DidVerifySensitiveDirectoryAccess(
      const BindingContext& binding_context,
      const FileSystemChooser::Options& options,
      ChooseEntriesCallback callback,
      std::vector<base::FilePath> entries,
      NativeFileSystemPermissionContext::SensitiveDirectoryResult result);
  void DidCreateOrTruncateSaveFile(const BindingContext& binding_context,
                                   const base::FilePath& path,
                                   ChooseEntriesCallback callback,
                                   bool success);
  void DidChooseDirectory(
      const BindingContext& binding_context,
      const base::FilePath& path,
      ChooseEntriesCallback callback,
      NativeFileSystemPermissionContext::PermissionStatus permission);

  void CreateTransferTokenImpl(
      const storage::FileSystemURL& url,
      const SharedHandleState& handle_state,
      bool is_directory,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  void DoResolveTransferToken(
      mojo::Remote<blink::mojom::NativeFileSystemTransferToken>,
      ResolvedTokenCallback callback,
      const base::UnguessableToken& token);

  // Creates a FileSystemURL which corresponds to a FilePath and Origin.
  struct FileSystemURLAndFSHandle {
    storage::FileSystemURL url;
    std::string base_name;
    storage::IsolatedContext::ScopedFSHandle file_system;
  };
  FileSystemURLAndFSHandle CreateFileSystemURLFromPath(
      const url::Origin& origin,
      const base::FilePath& path);

  blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPathImpl(
      const BindingContext& binding_context,
      const base::FilePath& file_path,
      NativeFileSystemPermissionContext::UserAction user_action);

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<storage::FileSystemContext> context_;
  const scoped_refptr<ChromeBlobStorageContext> blob_context_;
  base::SequenceBound<storage::FileSystemOperationRunner> operation_runner_;
  NativeFileSystemPermissionContext* permission_context_;

  // All the mojo receivers for this NativeFileSystemManager itself. Keeps
  // track of associated origin and other state as well to not have to rely on
  // the renderer passing that in, and to be able to do security checks around
  // transferability etc.
  mojo::ReceiverSet<blink::mojom::NativeFileSystemManager, BindingContext>
      receivers_;

  // All the receivers for file and directory handles that have references to
  // them.
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemFileHandle>
      file_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemDirectoryHandle>
      directory_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemFileWriter>
      writer_receivers_;

  bool off_the_record_;

  // NativeFileSystemTransferTokenImpl owns a Transfer token receiver and is
  // removed from this map when the mojo connection is closed.
  std::map<base::UnguessableToken,
           std::unique_ptr<NativeFileSystemTransferTokenImpl>>
      transfer_tokens_;

  base::WeakPtrFactory<NativeFileSystemManagerImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
