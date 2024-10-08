// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_system_access.pb.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/file_system_access_permission_grant.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class FileSystemContext;
}  // namespace storage

namespace content {
class FileSystemAccessAccessHandleHostImpl;
class FileSystemAccessDataTransferTokenImpl;
class FileSystemAccessDirectoryHandleImpl;
class FileSystemAccessFileHandleImpl;
class FileSystemAccessFileWriterImpl;
class FileSystemAccessTransferTokenImpl;
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

  // Get the FileSystem with a custom bucket override. Must provide a binding
  // context for this request.
  void GetSandboxedFileSystem(
      const BindingContext& binding_context,
      const std::optional<storage::BucketLocator>& bucket,
      const std::vector<std::string>& directory_path_components,
      GetSandboxedFileSystemCallback callback);

  // blink::mojom::FileSystemAccessManager:
  void GetSandboxedFileSystem(GetSandboxedFileSystemCallback callback) override;
  void GetSandboxedFileSystemForDevtools(
      const std::vector<std::string>& directory_path_components,
      GetSandboxedFileSystemCallback callback) override;
  void ChooseEntries(blink::mojom::FilePickerOptionsPtr options,
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
  void BindObserverHost(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
          host_receiver) override;

  // storage::mojom::FileSystemAccessContext:
  void SerializeHandle(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      SerializeHandleCallback callback) override;
  void DeserializeHandle(
      const blink::StorageKey& storage_key,
      const std::vector<uint8_t>& bits,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;
  void Clone(mojo::PendingReceiver<storage::mojom::FileSystemAccessContext>
                 receiver) override;

  // FileSystemAccessEntryFactory:
  blink::mojom::FileSystemAccessEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const PathInfo& path_info,
      UserAction user_action) override;
  blink::mojom::FileSystemAccessEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const PathInfo& path_info,
      UserAction user_action) override;
  void ResolveTransferToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          transfer_token,
      base::OnceCallback<void(std::optional<storage::FileSystemURL>)> callback)
      override;

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

  // Attempts to take a lock of `lock_type` on `url`. Passes a handle of the
  // lock to `callback` if successful. The lock is released when there are no
  // handles to it.
  //
  // `binding_context` is the `BindingContext` of the frame that holds this
  // handle.
  //
  // If there is an existing lock that is in contention with the `lock_type`, it
  // will evict pages that hold the existing lock if they are all inactive (e.g.
  // in the BFCache).
  void TakeLock(const BindingContext& binding_context,
                const storage::FileSystemURL& url,
                FileSystemAccessLockManager::LockType lock_type,
                FileSystemAccessLockManager::TakeLockCallback callback);

  // Returns true if there is not an existing lock on `url` that is contentious
  // with `lock_type`.
  //
  // This may return `false` but the same arguments would succeed for `TakeLock`
  // since `TakeLock` may evict pages to take the lock.
  bool IsContentious(const storage::FileSystemURL& url,
                     FileSystemAccessLockManager::LockType lock_type);

  // Creates a new shared lock type for testing.
  [[nodiscard]] FileSystemAccessLockManager::LockType
  CreateSharedLockTypeForTesting() const;

  // Gets the exclusive lock type.
  [[nodiscard]] FileSystemAccessLockManager::LockType GetExclusiveLockType()
      const;

  // Gets the shared lock type for SyncAccessHandle's `readonly` mode.
  [[nodiscard]] FileSystemAccessLockManager::LockType GetSAHReadOnlyLockType()
      const;

  // Gets the shared lock type for SyncAccessHandle's `readwrite-unsafe` mode.
  [[nodiscard]] FileSystemAccessLockManager::LockType
  GetSAHReadwriteUnsafeLockType() const;

  // Gets the shared lock type for WritableFileStream's default `siloed` mode.
  [[nodiscard]] FileSystemAccessLockManager::LockType GetWFSSiloedLockType()
      const;

  // Gets the `ancestor_lock_type_` for testing.
  [[nodiscard]] FileSystemAccessLockManager::LockType
  GetAncestorLockTypeForTesting() const;

  // Gets a `WeakPtr` of the `FileSystemAccessLockManager` for testing if its
  // been destroyed.
  base::WeakPtr<FileSystemAccessLockManager> GetLockManagerWeakPtrForTesting()
      const;

  // Creates a new FileSystemAccessFileWriterImpl for a given target and
  // swap file URLs. Assumes the passed in URLs are valid and represent files.
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>
  CreateFileWriter(
      const BindingContext& binding_context,
      const storage::FileSystemURL& url,
      const storage::FileSystemURL& swap_url,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      const SharedHandleState& handle_state,
      bool auto_close);
  // Returns a weak pointer to a newly created FileSystemAccessFileWriterImpl.
  // Useful for tests
  base::WeakPtr<FileSystemAccessFileWriterImpl> CreateFileWriter(
      const BindingContext& binding_context,
      const storage::FileSystemURL& url,
      const storage::FileSystemURL& swap_url,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      const SharedHandleState& handle_state,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
      bool has_transient_user_activation,
      bool auto_close,
      download::QuarantineConnectionCallback quarantine_connection_callback);
  // Creates a new FileSystemAccessHandleHostImpl for a given URL. Assumes `url`
  // is valid and represents a file. The `file_delegate_receiver` is only valid
  // in incognito mode.
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
  CreateAccessHandleHost(
      const storage::FileSystemURL& url,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          file_delegate_receiver,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileModificationHost>
          file_modification_host_receiver,
      int64_t file_size,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      base::ScopedClosureRunner on_close_callback);

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
      const PathInfo& path_info,
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

  // Generates a unique serialization of a URL, which can be used to check
  // handles for equality. This is not cryptographically secure.
  std::string SerializeURL(const storage::FileSystemURL& url,
                           FileSystemAccessPermissionContext::HandleType type);

  base::WeakPtr<FileSystemAccessManagerImpl> AsWeakPtr();

  storage::FileSystemContext* context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return context_.get();
  }
  ChromeBlobStorageContext* blob_context() { return blob_context_.get(); }
  const base::SequenceBound<storage::FileSystemOperationRunner>&
  operation_runner();

  FileSystemAccessPermissionContext* permission_context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return permission_context_;
  }

  FileSystemAccessWatcherManager& watcher_manager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return watcher_manager_;
  }

  bool is_off_the_record() const { return off_the_record_; }

  void SetPermissionContextForTesting(
      FileSystemAccessPermissionContext* permission_context) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    permission_context_ = permission_context;
  }

  void SetFilePickerResultForTesting(std::optional<PathInfo> result_entry) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto_file_picker_result_for_test_ = result_entry;
  }

  // Remove `writer` from `writer_receivers_`. It is an error to try to remove
  // a writer that doesn't exist.
  void RemoveFileWriter(FileSystemAccessFileWriterImpl* writer);

  // Remove `access_handle_host` from `access_handle_host_receivers_`. It is an
  // error to try to remove an access handle that doesn't exist.
  void RemoveAccessHandleHost(
      FileSystemAccessAccessHandleHostImpl* access_handle_host);

  // Remove `token` from `transfer_tokens_`. It is an error to try to remove
  // a token that doesn't exist.
  void RemoveToken(const base::UnguessableToken& token);

  // Remove `token` from `data_transfer_tokens_`. It is an error to try to
  // remove a token that doesn't exist.
  void RemoveDataTransferToken(const base::UnguessableToken& token);

  // This method may only be called on local and external file paths. Paths in a
  // sandboxed file system should use the variant below.
  //
  // TODO(crbug.com/40198034): Consolidate these methods once the relationships
  // of permission grants between handles are better specified.
  SharedHandleState GetSharedHandleStateForNonSandboxedPath(
      const PathInfo& path_info,
      const blink::StorageKey& storage_key,
      FileSystemAccessPermissionContext::HandleType handle_type,
      FileSystemAccessPermissionContext::UserAction user_action);
  // Same as above, but for paths in a sandboxed file system.
  SharedHandleState GetSharedHandleStateForSandboxedPath();

  // Return a stable unique ID of the FileSystemHandle in UUID version 4 format.
  base::Uuid GetUniqueId(const FileSystemAccessFileHandleImpl& file);
  base::Uuid GetUniqueId(const FileSystemAccessDirectoryHandleImpl& directory);

  // Creates a FileSystemURL from `path_info`, which must
  // correspond to a "real" file path and not a virtual path in a sandboxed file
  // system.
  storage::FileSystemURL CreateFileSystemURLFromPath(const PathInfo& path_info);

  void Shutdown();

  // The File System Access API should not give access to files that might
  // trigger special handling from the operating system. This method is used to
  // validate that all paths passed to GetFileHandle/GetDirectoryHandle are safe
  // to be exposed to the web.
  // TODO(crbug.com/40159607): Merge this with
  // net::IsSafePortablePathComponent.
  bool IsSafePathComponent(storage::FileSystemType type,
                           const url::Origin& origin,
                           const std::string& name);

  // Invokes `method` on the correct sequence on the FileSystemOperationRunner,
  // passing `args` and a callback to the method.
  // The passed in `callback` is wrapped to make sure it is called on the
  // correct sequence before passing it off to the `method`.
  //
  // Note that `callback` is passed to this method before other arguments,
  // while the wrapped callback will be passed as last argument to the
  // underlying FileSystemOperation `method`.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::OnceCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    // Wrap the passed in callback in one that posts a task back to the
    // current sequence.
    auto wrapped_callback =
        base::BindPostTaskToCurrentDefault(std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes context() is kept alive, to make sure
    // this happens it is bound to a callback that otherwise does nothing.
    operation_runner()
        .AsyncCall(base::IgnoreResult(method), from_here)
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce([](scoped_refptr<storage::FileSystemContext>) {},
                             base::WrapRefCounted(context())));
  }
  // Same as the previous overload, but using RepeatingCallback and
  // BindRepeating instead.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::RepeatingCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    // Wrap the passed in callback in one that posts a task back to the
    // current sequence.
    auto wrapped_callback = base::BindRepeating(
        [](scoped_refptr<base::SequencedTaskRunner> runner,
           const base::RepeatingCallback<void(CallbackArgs...)>& callback,
           CallbackArgs... args) {
          runner->PostTask(
              FROM_HERE,
              base::BindOnce(callback, std::forward<CallbackArgs>(args)...));
        },
        base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes context() is kept alive, to make sure
    // this happens it is bound to a callback that otherwise does nothing.
    operation_runner()
        .AsyncCall(base::IgnoreResult(method), from_here)
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce([](scoped_refptr<storage::FileSystemContext>) {},
                             base::WrapRefCounted(context())));
  }

 private:
  friend class FileSystemAccessFileHandleImpl;

  ~FileSystemAccessManagerImpl() override;
  void ResolveDefaultDirectory(
      const BindingContext& context,
      blink::mojom::FilePickerOptionsPtr options,
      ChooseEntriesCallback callback,
      FileSystemAccessTransferTokenImpl* resolved_directory_token);
  void SetDefaultPathAndShowPicker(const BindingContext& context,
                                   blink::mojom::FilePickerOptionsPtr options,
                                   base::FilePath default_directory,
                                   ChooseEntriesCallback callback,
                                   bool default_directory_exists);
  void DidOpenSandboxedFileSystem(
      const BindingContext& binding_context,
      GetSandboxedFileSystemCallback callback,
      const storage::FileSystemURL& root,
      const std::string& filesystem_name,
      base::File::Error result,
      const std::vector<std::string>& directory_path_components);
  void DidResolveUrlAfterOpeningSandboxedFileSystem(
      const BindingContext& binding_context,
      GetSandboxedFileSystemCallback callback,
      const storage::FileSystemURL& url,
      base::File::Error result);

  void DidChooseEntries(const BindingContext& binding_context,
                        const FileSystemChooser::Options& options,
                        const std::string& starting_directory_id,
                        bool request_directory_write_access,
                        ChooseEntriesCallback callback,
                        blink::mojom::FileSystemAccessErrorPtr result,
                        std::vector<PathInfo> entries);
  void DidVerifySensitiveDirectoryAccess(
      const BindingContext& binding_context,
      const FileSystemChooser::Options& options,
      const std::string& starting_directory_id,
      bool request_directory_write_access,
      ChooseEntriesCallback callback,
      std::vector<PathInfo> entries,
      FileSystemAccessPermissionContext::SensitiveEntryResult result);
  void OnCheckPathsAgainstEnterprisePolicy(
      const BindingContext& binding_context,
      const FileSystemChooser::Options& options,
      const std::string& starting_directory_id,
      bool request_directory_write_access,
      ChooseEntriesCallback callback,
      std::vector<PathInfo> entries);

  void DidCreateAndTruncateSaveFile(const BindingContext& binding_context,
                                    const PathInfo& entry,
                                    const storage::FileSystemURL& url,
                                    ChooseEntriesCallback callback,
                                    bool success);
  void DidChooseDirectory(
      const BindingContext& binding_context,
      const PathInfo& entry,
      ChooseEntriesCallback callback,
      const SharedHandleState& shared_handle_state,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome);

  void CreateTransferTokenImpl(
      const storage::FileSystemURL& url,
      const blink::StorageKey& storage_key,
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
  void DidGetSandboxedBucketForDeserializeHandle(
      const FileSystemAccessHandleData& data,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token,
      const storage::FileSystemURL& url);

  // FileSystemAccessFileModificationHosts may reserve too much capacity
  // from the quota system. This function determines the file's actual size
  // and corrects its capacity usage in the quota system.
  void CleanupAccessHandleCapacityAllocation(const storage::FileSystemURL& url,
                                             int64_t allocated_file_size,
                                             base::OnceClosure callback);

  // Performs the actual work of `CleanupAccessHandleCapacityAllocation()` after
  // the file's size has been determined.
  void CleanupAccessHandleCapacityAllocationImpl(
      const storage::FileSystemURL& url,
      int64_t allocated_file_size,
      base::OnceClosure callback,
      base::File::Error result,
      const base::File::Info& file_info);

  // Called after `CleanupAccessHandleCapacityAllocationImpl()` has completed.
  // Removes `access_handle_host` from the set of active hosts.
  void DidCleanupAccessHandleCapacityAllocation(
      FileSystemAccessAccessHandleHostImpl* access_handle_host);

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
      const PathInfo& path_info,
      const storage::FileSystemURL& url,
      GetEntryFromDataTransferTokenCallback token_resolved_callback,
      FileSystemAccessPermissionContext::HandleType file_type);

  // Calls `token_resolved_callback` with a FileSystemAccessEntry representing
  // the file/directory at `file_path`. Called by
  // ResolveDataTransferTokenWithFileType after it verifies the token does not
  // refer to a sensitive path.
  void DidVerifySensitiveDirectoryAccessForDataTransfer(
      const BindingContext& binding_context,
      const PathInfo& path_info,
      const storage::FileSystemURL& url,
      FileSystemAccessPermissionContext::HandleType file_type,
      GetEntryFromDataTransferTokenCallback token_resolved_callback,
      FileSystemAccessPermissionContext::SensitiveEntryResult result);

  // `root_permission_path` is path that the user selected in a file or
  // directory picker which led to the site having access to this URL. All
  // permissions related to the URL are based on this path.
  std::string SerializeURLWithPermissionRoot(
      const storage::FileSystemURL& url,
      FileSystemAccessPermissionContext::HandleType type,
      const base::FilePath& root_permission_path,
      const std::string& display_name);

  void FilePickerDeactivated(GlobalRenderFrameHostId global_rfh_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<storage::FileSystemContext> context_;
  const scoped_refptr<ChromeBlobStorageContext> blob_context_;
  base::SequenceBound<storage::FileSystemOperationRunner> operation_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<FileSystemAccessPermissionContext> permission_context_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // All the mojo receivers for this FileSystemAccessManager itself. Keeps
  // track of associated origin and other state as well to not have to rely on
  // the renderer passing that in, and to be able to do security checks around
  // transferability etc.
  mojo::ReceiverSet<blink::mojom::FileSystemAccessManager, BindingContext>
      receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ReceiverSet<storage::mojom::FileSystemAccessContext>
      internals_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The `lock_manager_` manager should be destroyed after `writer_receivers_`
  // and `access_handle_host_receivers_`. The locks held by file writers and
  // access handles dereference the lock manager on destruction, so it should
  // outlive them.
  scoped_refptr<FileSystemAccessLockManager> lock_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  FileSystemAccessWatcherManager watcher_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // All the receivers for file and directory handles that have references to
  // them.
  mojo::UniqueReceiverSet<blink::mojom::FileSystemAccessFileHandle>
      file_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::UniqueReceiverSet<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<std::unique_ptr<FileSystemAccessFileWriterImpl>,
                 base::UniquePtrComparator>
      writer_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<std::unique_ptr<FileSystemAccessAccessHandleHostImpl>,
                 base::UniquePtrComparator>
      access_handle_host_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  const bool off_the_record_;

  // FileSystemAccessTransferTokenImpl owns a Transfer token receiver set and is
  // removed from this map when all mojo connections are closed.
  std::map<base::UnguessableToken,
           std::unique_ptr<FileSystemAccessTransferTokenImpl>>
      transfer_tokens_ GUARDED_BY_CONTEXT(sequence_checker_);

  // This map is used to associate FileSystemAccessDataTransferTokenImpl
  // instances with UnguessableTokens so that this class can find an associated
  // FileSystemAccessDataTransferTokenImpl for a
  // mojo::PendingRemote<FileSystemAccessDataTransferToken>.
  std::map<base::UnguessableToken,
           std::unique_ptr<FileSystemAccessDataTransferTokenImpl>>
      data_transfer_tokens_ GUARDED_BY_CONTEXT(sequence_checker_);

  // TODO(crbug.com/40852050): This is a temporary hack to put something
  // that works behind a flag. Persist handle IDs such that they're stable
  // across browsing sessions.
  std::map<storage::FileSystemURL,
           base::Uuid,
           storage::FileSystemURL::Comparator>
      file_ids_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::map<storage::FileSystemURL,
           base::Uuid,
           storage::FileSystemURL::Comparator>
      directory_ids_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::set<GlobalRenderFrameHostId> rfhs_with_active_file_pickers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::optional<PathInfo> auto_file_picker_result_for_test_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The shared lock type for SyncAccessHandle's `readonly` mode.
  FileSystemAccessLockManager::LockType sah_read_only_lock_type_ =
      lock_manager_->CreateSharedLockType();
  // The shared lock type for SyncAccessHandle's `readwrite-unsafe` mode.
  FileSystemAccessLockManager::LockType sah_readwrite_unsafe_lock_type_ =
      lock_manager_->CreateSharedLockType();
  // The shared lock type for WritableFileStream's default `siloed` mode.
  FileSystemAccessLockManager::LockType wfs_siloed_lock_type_ =
      lock_manager_->CreateSharedLockType();

  base::WeakPtrFactory<FileSystemAccessManagerImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_MANAGER_IMPL_H_
