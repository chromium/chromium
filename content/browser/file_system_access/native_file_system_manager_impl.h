// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/native_file_system_context.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_file_system_entry_factory.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "content/public/browser/native_file_system_permission_grant.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_writer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_manager.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {
class NativeFileSystemFileHandleImpl;
class NativeFileSystemDirectoryHandleImpl;
class NativeFileSystemTransferTokenImpl;
class NativeFileSystemDragDropTokenImpl;
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
      public blink::mojom::NativeFileSystemManager,
      public storage::mojom::NativeFileSystemContext {
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

  void BindInternalsReceiver(
      mojo::PendingReceiver<storage::mojom::NativeFileSystemContext> receiver);

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
  void GetEntryFromDragDropToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemDragDropToken> token,
      GetEntryFromDragDropTokenCallback token_resolved_callback) override;

  // storage::mojom::NativeFileSystemContext:
  void SerializeHandle(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
      SerializeHandleCallback callback) override;
  void DeserializeHandle(
      const url::Origin& origin,
      const std::vector<uint8_t>& bits,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> token)
      override;

  // NativeFileSystemEntryFactory:
  blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& file_path,
      UserAction user_action) override;
  blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& directory_path,
      UserAction user_action) override;

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

  // Creates an instance of NativeFileSystemDragDropTokenImpl with `file_path`
  // and `renderer_id` and attaches the instance to `receiver`. The `receiver`'s
  // associated remote can be redeemed for a NativeFileSystemEntry object by a
  // process with ID matching `renderer_id`.
  void CreateNativeFileSystemDragDropToken(
      const base::FilePath& file_path,
      int renderer_id,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken>
          receiver);

  // Given a mojom transfer token, looks up the token in our internal list of
  // valid tokens. Calls the callback with the found token, or nullptr if no
  // valid token was found.
  using ResolvedTokenCallback =
      base::OnceCallback<void(NativeFileSystemTransferTokenImpl*)>;
  void ResolveTransferToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
      ResolvedTokenCallback callback);

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

  // Remove `token` from `drag_drop_tokens_`. It is an error to try to remove a
  // token that doesn't exist.
  void RemoveDragDropToken(const base::UnguessableToken& token);

  SharedHandleState GetSharedHandleStateForPath(
      const base::FilePath& path,
      const url::Origin& origin,
      storage::IsolatedContext::ScopedFSHandle file_system,
      NativeFileSystemPermissionContext::HandleType handle_type,
      NativeFileSystemPermissionContext::UserAction user_action);

  // Creates a FileSystemURL which corresponds to a FilePath and Origin.
  struct FileSystemURLAndFSHandle {
    storage::FileSystemURL url;
    std::string base_name;
    storage::IsolatedContext::ScopedFSHandle file_system;
  };
  FileSystemURLAndFSHandle CreateFileSystemURLFromPath(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path);

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
                        std::vector<FileSystemChooser::ResultEntry> entries);
  void DidVerifySensitiveDirectoryAccess(
      const BindingContext& binding_context,
      const FileSystemChooser::Options& options,
      ChooseEntriesCallback callback,
      std::vector<FileSystemChooser::ResultEntry> entries,
      NativeFileSystemPermissionContext::SensitiveDirectoryResult result);
  void DidCreateAndTruncateSaveFile(const BindingContext& binding_context,
                                    const FileSystemChooser::ResultEntry& entry,
                                    FileSystemURLAndFSHandle url,
                                    ChooseEntriesCallback callback,
                                    bool success);
  void DidChooseDirectory(
      const BindingContext& binding_context,
      const FileSystemChooser::ResultEntry& entry,
      ChooseEntriesCallback callback,
      const SharedHandleState& shared_handle_state,
      NativeFileSystemPermissionGrant::PermissionRequestOutcome outcome);

  void CreateTransferTokenImpl(
      const storage::FileSystemURL& url,
      const url::Origin& origin,
      const SharedHandleState& handle_state,
      NativeFileSystemPermissionContext::HandleType handle_type,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  void DoResolveTransferToken(
      mojo::Remote<blink::mojom::NativeFileSystemTransferToken>,
      ResolvedTokenCallback callback,
      const base::UnguessableToken& token);

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
  void DidResolveForSerializeHandle(
      SerializeHandleCallback callback,
      NativeFileSystemTransferTokenImpl* resolved_token);

  // Calls `token_resolved_callback` with a NativeFileSystemEntry object
  // that's at the file path of the NativeFileSystemDragDropToken with token
  // value `token`. If no such token exists, calls
  // `failed_token_redemption_callback`.
  void ResolveDragDropToken(
      mojo::Remote<blink::mojom::NativeFileSystemDragDropToken>,
      const BindingContext& binding_context,
      GetEntryFromDragDropTokenCallback token_resolved_callback,
      mojo::ReportBadMessageCallback failed_token_redemption_callback,
      const base::UnguessableToken& token);

  // Calls `token_resolved_callback` with a NativeFileSystemEntry representing
  // the file/directory at `file_path`. Called by
  // NativeFileSystemManager::ResolveDragDropToken after it looks up
  // whether the token's file path refers to a file or directory.
  void ResolveDragDropTokenWithFileType(
      const BindingContext& binding_context,
      const base::FilePath& file_path,
      GetEntryFromDragDropTokenCallback token_resolved_callback,
      NativeFileSystemPermissionContext::HandleType file_type);

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

  mojo::ReceiverSet<storage::mojom::NativeFileSystemContext>
      internals_receivers_;

  // All the receivers for file and directory handles that have references to
  // them.
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemFileHandle>
      file_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemDirectoryHandle>
      directory_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::NativeFileSystemFileWriter>
      writer_receivers_;

  bool off_the_record_;

  // NativeFileSystemTransferTokenImpl owns a Transfer token receiver set and is
  // removed from this map when all mojo connections are closed.
  std::map<base::UnguessableToken,
           std::unique_ptr<NativeFileSystemTransferTokenImpl>>
      transfer_tokens_;

  // This map is used to associate NativeFileSystemDragDropTokenImpl instances
  // with UnguessableTokens so that this class can find an associated
  // NativeFileSystemDragDropTokenImpl for a
  // mojo::PendingRemote<NativeFileSystemDragDropToken>.
  std::map<base::UnguessableToken,
           std::unique_ptr<NativeFileSystemDragDropTokenImpl>>
      drag_drop_tokens_;

  base::WeakPtrFactory<NativeFileSystemManagerImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_MANAGER_IMPL_H_
