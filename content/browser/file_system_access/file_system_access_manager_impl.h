// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/file_system_access_permission_grant.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {
class FileSystemAccessFileHandleImpl;
class FileSystemAccessDirectoryHandleImpl;
class FileSystemAccessTransferTokenImpl;
class FileSystemAccessDataTransferTokenImpl;
class FileSystemAccessFileWriterImpl;
class FileSystemAccessAccessHandleHostImpl;
class StoragePartitionImpl;

// This is the browser side implementation of the
// FileSystemAccessManager mojom interface. This is the main entry point for
// the File System Access API in the browser process.Instances of this class are
// owned by StoragePartitionImpl.
//
// This class owns all the FileSystemAccessFileHandleImpl,
// FileSystemAccessDirectoryHandleImpl and FileSystemAccessTransferTokenImpl
// instances for a specific storage partition.
//
// This class is not thread safe, it must be constructed and used on the UI
// thread only.
class CONTENT_EXPORT FileSystemAccessManagerImpl
    : public FileSystemAccessEntryFactory,
      public blink::mojom::FileSystemAccessManager,
      public storage::mojom::FileSystemAccessContext {
 public:
  using BindingContext = FileSystemAccessEntryFactory::BindingContext;
  using PassKey = base::PassKey<FileSystemAccessManagerImpl>;

  // State that is shared between handles that are derived from each other.
  // Handles that are created through ChooseEntries or GetSandboxedFileSystem
  // get new values for these properties, while any handles derived from those
  // (i.e. children of a directory) will inherit these properties from their
  // parent.
  struct CONTENT_EXPORT SharedHandleState {
    SharedHandleState(
        scoped_refptr<FileSystemAccessPermissionGrant> read_grant,
        scoped_refptr<FileSystemAccessPermissionGrant> write_grant);
    SharedHandleState(const SharedHandleState& other);
    ~SharedHandleState();

    // Should never be null. These are the read and write permissions for this
    // handle.
    const scoped_refptr<FileSystemAccessPermissionGrant> read_grant;
    const scoped_refptr<FileSystemAccessPermissionGrant> write_grant;
  };

  // The caller is responsible for ensuring that `permission_context` outlives
  // this instance.
  FileSystemAccessManagerImpl(
      scoped_refptr<storage::FileSystemContext> context,
      scoped_refptr<ChromeBlobStorageContext> blob_context,
      FileSystemAccessPermissionContext* permission_context,
      bool off_the_record);
  FileSystemAccessManagerImpl(const FileSystemAccessManagerImpl&) = delete;
  FileSystemAccessManagerImpl& operator=(const FileSystemAccessManagerImpl&) =
      delete;

  void BindReceiver(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver);

  void BindInternalsReceiver(
      mojo::PendingReceiver<storage::mojom::FileSystemAccessContext> receiver);

  // blink::mojom::FileSystemAccessManager:
  void GetSandboxedFileSystem(GetSandboxedFileSystemCallback callback) override;
  void ChooseEntries(blink::mojom::FilePickerOptionsPtr options,
                     blink::mojom::CommonFilePickerOptionsPtr common_options,
                     ChooseEntriesCallback callback) override;
  void GetFileHandleFromToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileHandle>
          file_handle_receiver) override;
  void GetDirectoryHandleFromToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessDirectoryHandle>
          directory_handle_receiver) override;
  void GetEntryFromDataTransferToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
          token,
      GetEntryFromDataTransferTokenCallback token_resolved_callback) override;

  // storage::mojom::FileSystemAccessContext:
  void SerializeHandle(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      SerializeHandleCallback callback) override;
  void DeserializeHandle(
      const url::Origin& origin,
      const std::vector<uint8_t>& bits,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;

  // FileSystemAccessEntryFactory:
  blink::mojom::FileSystemAccessEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& file_path,
      UserAction user_action) override;
  blink::mojom::FileSystemAccessEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& directory_path,
      UserAction user_action) override;

  // Creates a new FileSystemAccessFileHandleImpl for a given url. Assumes the
  // passed in URL is valid and represents a file.
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
  CreateFileHandle(const BindingContext& binding_context,
                   const storage::FileSystemURL& url,
                   const SharedHandleState& handle_state);

  // Creates a new FileSystemAccessDirectoryHandleImpl for a given url. Assumes
  // the passed in URL is valid and represents a directory.
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
  CreateDirectoryHandle(const BindingContext& context,
                        const storage::FileSystemURL& url,
                        const SharedHandleState& handle_state);

  // Creates a new FileSystemAccessFileWriterImpl for a given target and
  // swap file URLs. Assumes the passed in URLs are valid and represent files.
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>
  CreateFileWriter(const BindingContext& binding_context,
                   const storage::FileSystemURL& url,
                   const storage::FileSystemURL& swap_url,
                   const SharedHandleState& handle_state,
                   bool auto_close);
  // Returns a weak pointer to a newly created FileSystemAccessFileWriterImpl.
  // Useful for tests
  base::WeakPtr<FileSystemAccessFileWriterImpl> CreateFileWriter(
      const BindingContext& binding_context,
      const storage::FileSystemURL& url,
      const storage::FileSystemURL& swap_url,
      const SharedHandleState& handle_state,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
      bool has_transient_user_activation,
      bool auto_close,
      download::QuarantineConnectionCallback quarantine_connection_callback);

  // Creates a new FileSystemAccessHandleHost for a given URL. If there is
  // already an access handle assigned to `url`, returns an invalid pending
  // remote. The `file_delegate_receiver` is only valid in incognito mode.
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
  CreateAccessHandleHost(
      const storage::FileSystemURL& url,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          file_delegate_receiver);

  // Create a transfer token for a specific file or directory.
  void CreateTransferToken(
      const FileSystemAccessFileHandleImpl& file,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
          receiver);
  void CreateTransferToken(
      const FileSystemAccessDirectoryHandleImpl& directory,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
          receiver);

  // Creates an instance of FileSystemAccessDataTransferTokenImpl with
  // `file_path` and `renderer_id` and attaches the instance to `receiver`. The
  // `receiver`'s associated remote can be redeemed for a FileSystemAccessEntry
  // object by a process with ID matching `renderer_id`.
  void CreateFileSystemAccessDataTransferToken(
      PathType path_type,
      const base::FilePath& file_path,
      int renderer_id,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
          receiver);

  // Given a mojom transfer token, looks up the token in our internal list of
  // valid tokens. Calls the callback with the found token, or nullptr if no
  // valid token was found.
  using ResolvedTokenCallback =
      base::OnceCallback<void(FileSystemAccessTransferTokenImpl*)>;
  void ResolveTransferToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      ResolvedTokenCallback callback);

  storage::FileSystemContext* context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return context_.get();
  }
  ChromeBlobStorageContext* blob_context() { return blob_context_.get(); }
  const base::SequenceBound<storage::FileSystemOperationRunner>&
  operation_runner();

  FileSystemAccessPermissionContext* permission_context() {
    return permission_context_;
  }

  bool is_off_the_record() const { return off_the_record_; }

  void SetPermissionContextForTesting(
      FileSystemAccessPermissionContext* permission_context) {
    permission_context_ = permission_context;
  }

  void SetFilePickerResultForTesting(
      absl::optional<FileSystemChooser::ResultEntry> result_entry) {
    auto_file_picker_result_for_test_ = result_entry;
  }

  // Remove `writer` from `writer_receivers_`. It is an error to try to remove
  // a writer that doesn't exist.
  void RemoveFileWriter(FileSystemAccessFileWriterImpl* writer);

  // Releases the exclusive lock on `url` and removes the associated access
  // handle host. It is an error to try to remove an access
  // handle host that doesn't exist.
  void RemoveAccessHandleHost(const storage::FileSystemURL& url);

  // Remove `token` from `transfer_tokens_`. It is an error to try to remove
  // a token that doesn't exist.
  void RemoveToken(const base::UnguessableToken& token);

  // Remove `token` from `data_transfer_tokens_`. It is an error to try to
  // remove a token that doesn't exist.
  void RemoveDataTransferToken(const base::UnguessableToken& token);

  SharedHandleState GetSharedHandleStateForPath(
      const base::FilePath& path,
      const url::Origin& origin,
      FileSystemAccessPermissionContext::HandleType handle_type,
      FileSystemAccessPermissionContext::UserAction user_action);

  // Creates a FileSystemURL which corresponds to a FilePath and Origin.
  storage::FileSystemURL CreateFileSystemURLFromPath(
      PathType path_type,
      const base::FilePath& path);

  void Shutdown();

 private:
  friend class FileSystemAccessFileHandleImpl;

  ~FileSystemAccessManagerImpl() override;
  void ResolveDefaultDirectory(
      const BindingContext& context,
      blink::mojom::FilePickerOptionsPtr options,
      blink::mojom::CommonFilePickerOptionsPtr common_options,
      ChooseEntriesCallback callback,
      FileSystemAccessTransferTokenImpl* resolved_starting_directory_token);
  void SetDefaultPathAndShowPicker(
      const BindingContext& context,
      blink::mojom::FilePickerOptionsPtr options,
      blink::mojom::CommonFilePickerOptionsPtr common_options,
      base::FilePath default_directory,
      ChooseEntriesCallback callback,
      base::File::Error result);
  void DidOpenSandboxedFileSystem(const BindingContext& binding_context,
                                  GetSandboxedFileSystemCallback callback,
                                  const GURL& root,
                                  const std::string& filesystem_name,
                                  base::File::Error result);

  void DidChooseEntries(const BindingContext& binding_context,
                        const FileSystemChooser::Options& options,
                        const std::string& starting_directory_id,
                        bool request_directory_write_access,
                        ChooseEntriesCallback callback,
                        blink::mojom::FileSystemAccessErrorPtr result,
                        std::vector<FileSystemChooser::ResultEntry> entries);
  void DidVerifySensitiveDirectoryAccess(
      const BindingContext& binding_context,
      const FileSystemChooser::Options& options,
      const std::string& starting_directory_id,
      bool request_directory_write_access,
      ChooseEntriesCallback callback,
      std::vector<FileSystemChooser::ResultEntry> entries,
      FileSystemAccessPermissionContext::SensitiveDirectoryResult result);
  void DidCreateAndTruncateSaveFile(const BindingContext& binding_context,
                                    const FileSystemChooser::ResultEntry& entry,
                                    const storage::FileSystemURL& url,
                                    ChooseEntriesCallback callback,
                                    bool success);
  void DidChooseDirectory(
      const BindingContext& binding_context,
      const FileSystemChooser::ResultEntry& entry,
      ChooseEntriesCallback callback,
      const SharedHandleState& shared_handle_state,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome);

  void CreateTransferTokenImpl(
      const storage::FileSystemURL& url,
      const url::Origin& origin,
      const SharedHandleState& handle_state,
      FileSystemAccessPermissionContext::HandleType handle_type,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
          receiver);
  void DoResolveTransferToken(
      mojo::Remote<blink::mojom::FileSystemAccessTransferToken>,
      ResolvedTokenCallback callback,
      const base::UnguessableToken& token);

  void DidResolveTransferTokenForFileHandle(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileHandle>
          file_handle_receiver,
      FileSystemAccessTransferTokenImpl* resolved_token);
  void DidResolveTransferTokenForDirectoryHandle(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessDirectoryHandle>
          directory_handle_receiver,
      FileSystemAccessTransferTokenImpl* resolved_token);
  void DidResolveForSerializeHandle(
      SerializeHandleCallback callback,
      FileSystemAccessTransferTokenImpl* resolved_token);

  // Calls `token_resolved_callback` with a FileSystemAccessEntry object
  // that's at the file path of the FileSystemAccessDataTransferToken with token
  // value `token`. If no such token exists, calls
  // `failed_token_redemption_callback`.
  void ResolveDataTransferToken(
      mojo::Remote<blink::mojom::FileSystemAccessDataTransferToken>,
      const BindingContext& binding_context,
      GetEntryFromDataTransferTokenCallback token_resolved_callback,
      mojo::ReportBadMessageCallback failed_token_redemption_callback,
      const base::UnguessableToken& token);

  // Calls `token_resolved_callback` with a FileSystemAccessEntry representing
  // the file/directory at `file_path`. Called by
  // FileSystemAccessManager::ResolveDataTransferToken after it looks up
  // whether the token's file path refers to a file or directory.
  void ResolveDataTransferTokenWithFileType(
      const BindingContext& binding_context,
      const base::FilePath& file_path,
      const storage::FileSystemURL& url,
      GetEntryFromDataTransferTokenCallback token_resolved_callback,
      FileSystemAccessPermissionContext::HandleType file_type);

  // Owns receivers that have write access to files (i.e., AccessHandleHost and
  // FileWriter), ensuring that the right locks are taken.
  //
  // Adding an access handle takes an exclusive lock, preventing the addition
  // of other access handles or file writers that operate on the same URL.
  //
  // Adding a file writer takes a shared lock, allowing the addition of other
  // file writers that operate on the same URL, but preventing it for similar
  // access handles.
  //
  // This class should only handle kFileSystemTypeTemporary URLs, since it
  // relies on a 1-to-1 URL to file mapping.
  class WriteLockManager {
   public:
    WriteLockManager();
    ~WriteLockManager();

    // Attempts to take an exclusive lock on `url` and takes ownsership of
    // `access_handle`. Returns true if successful, false otherwise.
    bool AddAccessHandle(
        const storage::FileSystemURL& url,
        std::unique_ptr<FileSystemAccessAccessHandleHostImpl> access_handle);
    // Attempts to take a shared lock on `url` and takes ownsership of
    // `writer`. Returns true if successful, false otherwise.
    bool AddWriter(const storage::FileSystemURL& url,
                   std::unique_ptr<FileSystemAccessFileWriterImpl> writer);
    // Releases the exclusive lock on `url` and the ownership over the
    // associated access handle. It is is a error to call this method if
    // there is no exclusive lock on the URL.
    void RemoveAccessHandle(const storage::FileSystemURL& url);
    // Releases the shared lock on `url` and the ownership over the
    // associated writer. It is is a error to call this method if
    // there is no shared lock on the URL.
    void RemoveWriter(const storage::FileSystemURL& url);

   private:
    std::map<storage::FileSystemURL,
             base::flat_set<std::unique_ptr<FileSystemAccessFileWriterImpl>,
                            base::UniquePtrComparator>,
             storage::FileSystemURL::Comparator>
        writer_receivers_;
    std::map<storage::FileSystemURL,
             std::unique_ptr<FileSystemAccessAccessHandleHostImpl>,
             storage::FileSystemURL::Comparator>
        access_handle_receivers_;
  };

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<storage::FileSystemContext> context_;
  const scoped_refptr<ChromeBlobStorageContext> blob_context_;
  base::SequenceBound<storage::FileSystemOperationRunner> operation_runner_;
  FileSystemAccessPermissionContext* permission_context_;

  // All the mojo receivers for this FileSystemAccessManager itself. Keeps
  // track of associated origin and other state as well to not have to rely on
  // the renderer passing that in, and to be able to do security checks around
  // transferability etc.
  mojo::ReceiverSet<blink::mojom::FileSystemAccessManager, BindingContext>
      receivers_;

  mojo::ReceiverSet<storage::mojom::FileSystemAccessContext>
      internals_receivers_;

  // All the receivers for file and directory handles that have references to
  // them.
  mojo::UniqueReceiverSet<blink::mojom::FileSystemAccessFileHandle>
      file_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_receivers_;
  base::flat_set<std::unique_ptr<FileSystemAccessFileWriterImpl>,
                 base::UniquePtrComparator>
      writer_receivers_;
  WriteLockManager write_lock_manager_;

  bool off_the_record_;

  // FileSystemAccessTransferTokenImpl owns a Transfer token receiver set and is
  // removed from this map when all mojo connections are closed.
  std::map<base::UnguessableToken,
           std::unique_ptr<FileSystemAccessTransferTokenImpl>>
      transfer_tokens_;

  // This map is used to associate FileSystemAccessDataTransferTokenImpl
  // instances with UnguessableTokens so that this class can find an associated
  // FileSystemAccessDataTransferTokenImpl for a
  // mojo::PendingRemote<FileSystemAccessDataTransferToken>.
  std::map<base::UnguessableToken,
           std::unique_ptr<FileSystemAccessDataTransferTokenImpl>>
      data_transfer_tokens_;

  absl::optional<FileSystemChooser::ResultEntry>
      auto_file_picker_result_for_test_;

  base::WeakPtrFactory<FileSystemAccessManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_
