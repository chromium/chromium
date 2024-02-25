// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_RUNNER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/containers/id_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {

class FileSystemURL;
class FileSystemContext;

// A central interface for running FileSystem API operations.
// All operation methods take callback and returns OperationID, which is
// an integer value which can be used for cancelling an operation.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemOperationRunner {
 public:
  using GetMetadataCallback = FileSystemOperation::GetMetadataCallback;
  using ReadDirectoryCallback = FileSystemOperation::ReadDirectoryCallback;
  using SnapshotFileCallback = FileSystemOperation::SnapshotFileCallback;
  using StatusCallback = FileSystemOperation::StatusCallback;
  using WriteCallback = FileSystemOperation::WriteCallback;
  // Implementers of FileSystemOperation::OpenFile() pass `on_close_callback` as
  // a OnceClosure, but pass consumers a ScopedClosureRunner to ensure the
  // callback is always run, and on the correct sequence (the I/O thread).
  using OpenFileCallback =
      base::OnceCallback<void(base::File file,
                              base::ScopedClosureRunner on_close_callback)>;
  using ErrorBehavior = FileSystemOperation::ErrorBehavior;
  using CopyFileProgressCallback =
      FileSystemOperation::CopyFileProgressCallback;
  using CopyOrMoveOptionSet = FileSystemOperation::CopyOrMoveOptionSet;
  using GetMetadataField = FileSystemOperation::GetMetadataField;
  using GetMetadataFieldSet = FileSystemOperation::GetMetadataFieldSet;

  using OperationID = uint64_t;

  // |file_system_context| is stored as a raw pointer. The caller must ensure
  // that |file_system_context| outlives the new instance.
  FileSystemOperationRunner(
      base::PassKey<FileSystemContext>,
      const scoped_refptr<FileSystemContext>& file_system_context);
  FileSystemOperationRunner(base::PassKey<FileSystemContext>,
                            FileSystemContext* file_system_context);

  FileSystemOperationRunner(const FileSystemOperationRunner&) = delete;
  FileSystemOperationRunner& operator=(const FileSystemOperationRunner&) =
      delete;

  virtual ~FileSystemOperationRunner();

  // Cancels all inflight operations.
  void Shutdown();

  // Creates a file at |url|. If |exclusive| is true, an error is raised
  // in case a file is already present at the URL.
  OperationID CreateFile(const FileSystemURL& url,
                         bool exclusive,
                         StatusCallback callback);

  OperationID CreateDirectory(const FileSystemURL& url,
                              bool exclusive,
                              bool recursive,
                              StatusCallback callback);

  // Copies a file or directory from |src_url| to |dest_url|. If
  // |src_url| is a directory, the contents of |src_url| are copied to
  // |dest_url| recursively. A new file or directory is created at
  // |dest_url| as needed.
  // For |option| and |copy_or_move_hook_delegate|, see file_system_operation.h
  // for details.
  OperationID Copy(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      ErrorBehavior error_behavior,
      std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
      StatusCallback callback);

  // Moves a file or directory from |src_url| to |dest_url|. A new file
  // or directory is created at |dest_url| as needed.
  // For |option| and |copy_or_move_hook_delegate|, see file_system_operation.h
  // for details.
  OperationID Move(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      ErrorBehavior error_behavior,
      std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
      StatusCallback callback);

  // Checks if a directory is present at |url|.
  OperationID DirectoryExists(const FileSystemURL& url,
                              StatusCallback callback);

  // Checks if a file is present at |url|.
  OperationID FileExists(const FileSystemURL& url, StatusCallback callback);

  // Gets the metadata of a file or directory at |url|.
  OperationID GetMetadata(const FileSystemURL& url,
                          GetMetadataFieldSet fields,
                          GetMetadataCallback callback);

  // Reads contents of a directory at |url|.
  OperationID ReadDirectory(const FileSystemURL& url,
                            const ReadDirectoryCallback& callback);

  // Removes a file or directory at |url|. If |recursive| is true, remove
  // all files and directories under the directory at |url| recursively.
  OperationID Remove(const FileSystemURL& url,
                     bool recursive,
                     StatusCallback callback);

  // Writes contents of |blob| to |url| at |offset|.
  OperationID Write(const FileSystemURL& url,
                    std::unique_ptr<BlobDataHandle> blob,
                    int64_t offset,
                    const WriteCallback& callback);

  // Writes contents of |data_pipe| to |url| at |offset|.
  OperationID WriteStream(const FileSystemURL& url,
                          mojo::ScopedDataPipeConsumerHandle data_pipe,
                          int64_t offset,
                          const WriteCallback& callback);

  // Truncates a file at |url| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  OperationID Truncate(const FileSystemURL& url,
                       int64_t length,
                       StatusCallback callback);

  // Tries to cancel the operation |id| [we support cancelling write or
  // truncate only]. Reports failure for the current operation, then reports
  // success for the cancel operation itself via the |callback|.
  void Cancel(OperationID id, StatusCallback callback);

  // Modifies timestamps of a file or directory at |url| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // This function is used only by Pepper as of writing.
  OperationID TouchFile(const FileSystemURL& url,
                        const base::Time& last_access_time,
                        const base::Time& last_modified_time,
                        StatusCallback callback);

  // Opens a file at |url| with |file_flags|, where flags are OR'ed values of
  // base::File::Flags. This operation is not supported on all filesystems or
  // all situation e.g. it will always fail for the sandboxed system when in
  // Incognito mode.
  OperationID OpenFile(const FileSystemURL& url,
                       uint32_t file_flags,
                       OpenFileCallback callback);

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform url of the snapshot file via |callback|.
  // In local filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in remote filesystem case the backend may want to download the file
  // into a temporary snapshot file and return the metadata of the
  // temporary file.  Or if the implementation already has the local cache
  // data for |url| it can simply return the url to the cache.
  OperationID CreateSnapshotFile(const FileSystemURL& url,
                                 SnapshotFileCallback callback);

  // Copies in a single file from a different filesystem.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_file_path|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID CopyInForeignFile(const base::FilePath& src_local_disk_path,
                                const FileSystemURL& dest_url,
                                StatusCallback callback);

  // Removes a single file.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  OperationID RemoveFile(const FileSystemURL& url, StatusCallback callback);

  // Removes a single empty directory.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - File::FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  OperationID RemoveDirectory(const FileSystemURL& url,
                              StatusCallback callback);

  // Copies a file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // For |option| and |progress_callback|, see file_system_operation.h for
  // details.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID CopyFileLocal(const FileSystemURL& src_url,
                            const FileSystemURL& dest_url,
                            CopyOrMoveOptionSet options,
                            const CopyFileProgressCallback& progress_callback,
                            StatusCallback callback);

  // Moves a local file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // For |option|, see file_system_operation.h for details.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - File::FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - File::FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  OperationID MoveFileLocal(const FileSystemURL& src_url,
                            const FileSystemURL& dest_url,
                            CopyOrMoveOptionSet options,
                            StatusCallback callback);

  // This is called only by pepper plugin as of writing to synchronously get
  // the underlying platform path to upload a file in the sandboxed filesystem
  // (e.g. TEMPORARY or PERSISTENT).
  base::File::Error SyncGetPlatformPath(const FileSystemURL& url,
                                        base::FilePath* platform_path);

 private:
  explicit FileSystemOperationRunner(FileSystemContext* file_system_context);

  void DidFinish(const OperationID id,
                 StatusCallback callback,
                 base::File::Error rv);
  void DidGetMetadata(const OperationID id,
                      GetMetadataCallback callback,
                      base::File::Error rv,
                      const base::File::Info& file_info);
  void DidReadDirectory(const OperationID id,
                        const ReadDirectoryCallback& callback,
                        base::File::Error rv,
                        std::vector<filesystem::mojom::DirectoryEntry> entries,
                        bool has_more);
  void DidWrite(const OperationID id,
                const WriteCallback& callback,
                base::File::Error rv,
                int64_t bytes,
                bool complete);
  void DidOpenFile(const OperationID id,
                   OpenFileCallback callback,
                   base::File file,
                   base::OnceClosure on_close_callback);
  void DidCreateSnapshot(const OperationID id,
                         SnapshotFileCallback callback,
                         base::File::Error rv,
                         const base::File::Info& file_info,
                         const base::FilePath& platform_path,
                         scoped_refptr<ShareableFileReference> file_ref);

  void PrepareForWrite(OperationID id, const FileSystemURL& url);
  void PrepareForRead(OperationID id, const FileSystemURL& url);

  // These must be called at the beginning and end of any async operations.
  OperationID BeginOperation(std::unique_ptr<FileSystemOperation> operation);
  // Cleans up the FileSystemOperation for |id|, which may result in the
  // FileSystemContext, and |this| being deleted, by the time the call returns.
  void FinishOperation(OperationID id);

  // Not owned; whatever owns this has to make sure context outlives this.
  raw_ptr<FileSystemContext, AcrossTasksDanglingUntriaged> file_system_context_;

  using Operations =
      std::map<OperationID, std::unique_ptr<FileSystemOperation>>;
  OperationID next_operation_id_ = 1;
  Operations operations_;

  // Used to detect synchronous invocation of completion callbacks by the
  // back-end, to re-post them to be notified asynchronously. Note that some
  // operations are recursive, so this may already be true when BeginOperation
  // is called.
  bool is_beginning_operation_ = false;

  // We keep track of the file to be modified by each operation so that
  // we can notify observers when we're done.
  std::map<OperationID, FileSystemURLSet> write_target_urls_;

  // Operations that are finished but not yet fire their callbacks.
  std::set<OperationID> finished_operations_;

  // Callbacks for stray cancels whose target operation is already finished.
  std::map<OperationID, StatusCallback> stray_cancel_callbacks_;

  base::WeakPtr<FileSystemOperationRunner> weak_ptr_;
  base::WeakPtrFactory<FileSystemOperationRunner> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_RUNNER_H_
