// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/process/process.h"
#include "base/types/pass_key.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/file_system/file_system_operation_context.h"

namespace base {
class Time;
}

namespace storage {

class CopyOrMoveHookDelegate;
class FileSystemContext;
class FileSystemURL;
class FileWriterDelegate;
class ShareableFileReference;

// Operation type called by FileSystemOperationRunner.
enum class OperationType {
  kNone,
  kCreateFile,
  kCreateDirectory,
  kCreateSnapshotFile,
  kCopy,
  kCopyInForeignFile,
  kMove,
  kDirectoryExists,
  kFileExists,
  kGetMetadata,
  kReadDirectory,
  kRemove,
  kWrite,
  kTruncate,
  kTouchFile,
  kOpenFile,
  kCloseFile,
  kGetLocalPath,
  kCancel,
};

// The interface class for FileSystemOperation implementations.
//
// This interface defines file system operations required to implement
// "File API: Directories and System"
// http://www.w3.org/TR/file-system-api/
//
// DESIGN NOTES
//
// This class is designed to
//
// 1) Serve one-time file system operation per instance.  Only one
// method(CreateFile, CreateDirectory, Copy, Move, DirectoryExists,
// GetMetadata, ReadDirectory and Remove) may be called during the
// lifetime of this object and it should be called no more than once.
//
// 2) Deliver the results of operations to the client via the callback function
// passed as the last parameter of the method.
//
// Note that it is valid to delete an operation while it is running.
// The callback will NOT be fired if the operation is deleted before
// it gets called.
class FileSystemOperation {
 public:
  COMPONENT_EXPORT(STORAGE_BROWSER)
  static std::unique_ptr<FileSystemOperation> Create(
      OperationType type,
      const FileSystemURL& url,
      FileSystemContext* file_system_context,
      std::unique_ptr<FileSystemOperationContext> operation_context);

  FileSystemOperation(const FileSystemOperation&) = delete;
  FileSystemOperation& operator=(const FileSystemOperation&) = delete;
  virtual ~FileSystemOperation() = default;

  // Used for CreateFile(), etc. |result| is the return code of the operation.
  using StatusCallback = base::OnceCallback<void(base::File::Error result)>;

  // Used for GetMetadata(). |result| is the return code of the operation,
  // |file_info| is the obtained file info.
  using GetMetadataCallback =
      base::OnceCallback<void(base::File::Error result,
                              const base::File::Info& file_info)>;

  // Used for OpenFile(). File system implementations can specify an
  // `on_close_callback` if an operation is needed after closing a file. If
  // non-null, OpenFile() callers must run the callback (on the IO thread)
  // after the file closes. If the file is duped, the callback should not be run
  // until all dups of the file have been closed.
  using OpenFileCallback =
      base::OnceCallback<void(base::File file,
                              base::OnceClosure on_close_callback)>;

  // Used for ReadDirectoryCallback.
  using FileEntryList = std::vector<filesystem::mojom::DirectoryEntry>;

  // Used for ReadDirectory(). |result| is the return code of the operation,
  // |file_list| is the list of files read, and |has_more| is true if some files
  // are yet to be read.
  using ReadDirectoryCallback = base::RepeatingCallback<
      void(base::File::Error result, FileEntryList file_list, bool has_more)>;

  // Used for CreateSnapshotFile(). (Please see the comment at
  // CreateSnapshotFile() below for how the method is called)
  // |result| is the return code of the operation.
  // |file_info| is the metadata of the snapshot file created.
  // |platform_path| is the path to the snapshot file created.
  //
  // The snapshot file could simply be of the local file pointed by the given
  // filesystem URL in local filesystem cases; remote filesystems
  // may want to download the file into a temporary snapshot file and then
  // return the metadata of the temporary file.
  //
  // |file_ref| is used to manage the lifetime of the returned
  // snapshot file.  It can be set to let the chromium backend take
  // care of the life time of the snapshot file.  Otherwise (if the returned
  // file does not require any handling) the implementation can just
  // return nullptr.  In a more complex case, the implementation can manage
  // the lifetime of the snapshot file on its own (e.g. by its cache system)
  // but also can be notified via the reference when the file becomes no
  // longer necessary in the javascript world.
  // Please see the comment for ShareableFileReference for details.
  //
  using SnapshotFileCallback =
      base::OnceCallback<void(base::File::Error result,
                              const base::File::Info& file_info,
                              const base::FilePath& platform_path,
                              scoped_refptr<ShareableFileReference> file_ref)>;

  // Used to specify how recursive operation delegate behaves for errors.
  // With ERROR_BEHAVIOR_ABORT, it stops following operation when it fails an
  // operation.
  // With ERROR_BEHAVIOR_SKIP, it continues following operation even when it
  // fails some of the operations.
  enum ErrorBehavior { ERROR_BEHAVIOR_ABORT, ERROR_BEHAVIOR_SKIP };

  // Used for CopyFileLocal() to report progress update.
  // |size| is the cumulative copied bytes for the copy.
  // At the beginning the progress callback should be called with |size| = 0,
  // and also at the ending the progress callback should be called with |size|
  // set to the copied file size.
  using CopyFileProgressCallback = base::RepeatingCallback<void(int64_t size)>;

  // The possible options for copy or move operations. Used as an EnumSet to
  // allow multiple options to be specified.
  enum class CopyOrMoveOption {
    // Preserves last modified time if possible. If the operation to update
    // last modified time is not supported on the file system for the
    // destination file, this option would be simply ignored (i.e. Copy would
    // be successfully done without preserving last modified time).
    kPreserveLastModified,

    // Preserves permissions of the destination file. If the operation to update
    // permissions is not supported on the file system for the destination file,
    // this option will simply be ignored (i.e. Copy would be successfully done
    // without preserving permissions of the destination file).
    kPreserveDestinationPermissions,

    // Forces the copy or move operation to use the cross-filesystem
    // implementation.
    kForceCrossFilesystem,

    // Removes copied files that result in an error (potentially a
    // cancellation), as these files are potentially partial/corrupted.
    // Directories are not removed recursively, as it can lead to data loss
    // (e.g. user changing the content of the destination folder during a copy
    // or a move). Therefore, all successfully copied entries are preserved.
    // The removal is best-effort: depending on the origin of the error,
    // removing the destination file can fail.
    // This option can impact cross-filesystem moves since they are implemented
    // as copy + delete (only the copy step is impacted), but not
    // same-filesystem moves where the file paths are just renamed.
    kRemovePartiallyCopiedFilesOnError,

    kFirst = kPreserveLastModified,
    kLast = kRemovePartiallyCopiedFilesOnError
  };

  using CopyOrMoveOptionSet = base::EnumSet<CopyOrMoveOption,
                                            CopyOrMoveOption::kFirst,
                                            CopyOrMoveOption::kLast>;

  // Fields requested for the GetMetadata method. Used as an EnumSet to allow
  // multiple fields to be specified.
  enum class GetMetadataField {
    // Returns the size of the target. Undefined for directories. See also
    // kRecursiveSize.
    kSize,

    kIsDirectory,

    kLastModified,

    // If the target is directory, then total size of directory contents
    // is returned, otherwise it's identical to kSize.
    kRecursiveSize,

    kFirst = kSize,
    kLast = kRecursiveSize
  };

  using GetMetadataFieldSet = base::EnumSet<GetMetadataField,
                                            GetMetadataField::kFirst,
                                            GetMetadataField::kLast>;

  // Used for Write().
  using WriteCallback = base::RepeatingCallback<
      void(base::File::Error result, int64_t bytes, bool complete)>;

  // Creates a file at |path|. If |exclusive| is true, an error is raised
  // in case a file is already present at the URL.
  virtual void CreateFile(const FileSystemURL& path,
                          bool exclusive,
                          StatusCallback callback) = 0;

  // Creates a directory at |path|. If |exclusive| is true, an error is
  // raised in case a directory is already present at the URL. If
  // |recursive| is true, create parent directories as needed just like
  // mkdir -p does.
  virtual void CreateDirectory(const FileSystemURL& path,
                               bool exclusive,
                               bool recursive,
                               StatusCallback callback) = 0;

  // Copies a file or directory from |src_path| to |dest_path|. If
  // |src_path| is a directory, the contents of |src_path| are copied to
  // |dest_path| recursively. A new file or directory is created at
  // |dest_path| as needed.
  // |option| specifies the minor behavior of Copy(). See CopyOrMoveOption's
  // comment for details.
  // |error_behavior| specifies whether this continues operation after it
  // failed an operation or not.
  // |copy_or_move_hook_delegate|'s functions are periodically called to report
  // the current state of the operation. See also the comments of
  // CopyOrMoveHookDelegate. |copy_or_move_hook_delegate| is required.
  //
  // For recursive case this internally creates new FileSystemOperations and
  // calls:
  // - ReadDirectory, CopyFileLocal and CreateDirectory
  //   for same-filesystem case, or
  // - ReadDirectory and CreateSnapshotFile on source filesystem and
  //   CopyInForeignFile and CreateDirectory on dest filesystem
  //   for cross-filesystem case.
  //
  virtual void Copy(
      const FileSystemURL& src_path,
      const FileSystemURL& dest_path,
      CopyOrMoveOptionSet options,
      ErrorBehavior error_behavior,
      std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
      StatusCallback callback) = 0;

  // Moves a file or directory from |src_path| to |dest_path|. A new file
  // or directory is created at |dest_path| as needed.
  // |option| specifies the minor behavior of Copy(). See CopyOrMoveOption's
  // comment for details.
  // |error_behavior| specifies whether this continues operation after it
  // failed an operation or not.
  // |copy_or_move_hook_delegate|'s functions are periodically called to report
  // the current state of the operation. See also the comments of
  // CopyOrMoveHookDelegate. |copy_or_move_hook_delegate| is required.
  //
  // For recursive case this internally creates new FileSystemOperations and
  // calls:
  // - ReadDirectory, MoveFileLocal, CreateDirectory and Remove
  //   for same-filesystem case, or
  // - ReadDirectory, CreateSnapshotFile and Remove on source filesystem and
  //   CopyInForeignFile and CreateDirectory on dest filesystem
  //   for cross-filesystem case.
  //
  // TODO(crbug.com/40960653): Restore directory timestamps after the Move
  //                         operation.
  virtual void Move(
      const FileSystemURL& src_path,
      const FileSystemURL& dest_path,
      CopyOrMoveOptionSet options,
      ErrorBehavior error_behavior,
      std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
      StatusCallback callback) = 0;

  // Checks if a directory is present at |path|.
  virtual void DirectoryExists(const FileSystemURL& path,
                               StatusCallback callback) = 0;

  // Checks if a file is present at |path|.
  virtual void FileExists(const FileSystemURL& path,
                          StatusCallback callback) = 0;

  // Gets the metadata of a file or directory at |path|.
  virtual void GetMetadata(const FileSystemURL& path,
                           GetMetadataFieldSet fields,
                           GetMetadataCallback callback) = 0;

  // Reads contents of a directory at |path|.
  virtual void ReadDirectory(const FileSystemURL& path,
                             const ReadDirectoryCallback& callback) = 0;

  // Removes a file or directory at |path|. If |recursive| is true, remove
  // all files and directories under the directory at |path| recursively.
  virtual void Remove(const FileSystemURL& path,
                      bool recursive,
                      StatusCallback callback) = 0;

  // Writes the data read from |blob_reader| using |writer_delegate|.
  virtual void WriteBlob(const FileSystemURL& url,
                         std::unique_ptr<FileWriterDelegate> writer_delegate,
                         std::unique_ptr<BlobReader> blob_reader,
                         const WriteCallback& callback) = 0;

  // Writes the data read from |data_pipe| using |writer_delegate|.
  virtual void Write(const FileSystemURL& url,
                     std::unique_ptr<FileWriterDelegate> writer_delegate,
                     mojo::ScopedDataPipeConsumerHandle data_pipe,
                     const WriteCallback& callback) = 0;

  // Truncates a file at |path| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  virtual void Truncate(const FileSystemURL& path,
                        int64_t length,
                        StatusCallback callback) = 0;

  // Tries to cancel the current operation [we support cancelling write or
  // truncate only]. Reports failure for the current operation, then reports
  // success for the cancel operation itself via the |cancel_dispatcher|.
  //
  // E.g. a typical cancel implementation would look like:
  //
  //   virtual void SomeOperationImpl::Cancel(
  //       StatusCallback cancel_callback) {
  //     // Abort the current inflight operation first.
  //     ...
  //
  //     // Dispatch ABORT error for the current operation by invoking
  //     // the callback function for the ongoing operation,
  //     operation_callback.Run(base::File::FILE_ERROR_ABORT, ...);
  //
  //     // Dispatch 'success' for the cancel (or dispatch appropriate
  //     // error code with DidFail() if the cancel has somehow failed).
  //     std::move(cancel_callback).Run(base::File::FILE_OK);
  //   }
  //
  // Note that, for reporting failure, the callback function passed to a
  // cancellable operations are kept around with the operation instance
  // (as |operation_callback_| in the code example).
  virtual void Cancel(StatusCallback cancel_callback) = 0;

  // Modifies timestamps of a file or directory at |path| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // This function is used only by Pepper as of writing.
  virtual void TouchFile(const FileSystemURL& path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         StatusCallback callback) = 0;

  // Opens a file at |path| with |file_flags|, where flags are OR'ed
  // values of base::File::Flags.
  //
  // This function is used only by Pepper as of writing.
  virtual void OpenFile(const FileSystemURL& path,
                        uint32_t file_flags,
                        OpenFileCallback callback) = 0;

  // Creates a local snapshot file for a given |path| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // In local filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in remote filesystem case the backend may want to download the file
  // into a temporary snapshot file and return the metadata of the
  // temporary file.  Or if the implementation already has the local cache
  // data for |path| it can simply return the path to the cache.
  virtual void CreateSnapshotFile(const FileSystemURL& path,
                                  SnapshotFileCallback callback) = 0;

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
  virtual void CopyInForeignFile(const base::FilePath& src_local_disk_path,
                                 const FileSystemURL& dest_url,
                                 StatusCallback callback) = 0;

  // Removes a single file.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  virtual void RemoveFile(const FileSystemURL& url,
                          StatusCallback callback) = 0;

  // Removes a single empty directory.
  //
  // This returns:
  // - File::FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - File::FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - File::FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  virtual void RemoveDirectory(const FileSystemURL& url,
                               StatusCallback callback) = 0;

  // Copies a file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // |option| specifies the minor behavior of Copy(). See CopyOrMoveOption's
  // comment for details.
  // |progress_callback| is periodically called to report the progress
  // update. See also the comment of CopyFileProgressCallback. This callback is
  // optional.
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
  virtual void CopyFileLocal(const FileSystemURL& src_url,
                             const FileSystemURL& dest_url,
                             CopyOrMoveOptionSet options,
                             const CopyFileProgressCallback& progress_callback,
                             StatusCallback callback) = 0;

  // Moves a local file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  // |option| specifies the minor behavior of Copy(). See CopyOrMoveOption's
  // comment for details.
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
  virtual void MoveFileLocal(const FileSystemURL& src_url,
                             const FileSystemURL& dest_url,
                             CopyOrMoveOptionSet options,
                             StatusCallback callback) = 0;

  // Synchronously gets the platform path for the given |url|.
  // This may fail if the given |url|'s filesystem type is neither
  // temporary nor persistent.
  // In such a case, base::File::FILE_ERROR_INVALID_OPERATION will be
  // returned.
  virtual base::File::Error SyncGetPlatformPath(
      const FileSystemURL& url,
      base::FilePath* platform_path) = 0;

 protected:
  FileSystemOperation() = default;

  // Allows subclasses to call the FileSystemOperationImpl constructor.
  static base::PassKey<FileSystemOperation> CreatePassKey() {
    return base::PassKey<FileSystemOperation>();
  }
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_H_
