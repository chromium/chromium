// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/open_file_system_mode.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

class GURL;

namespace storage {

class AsyncFileUtil;
class CopyOrMoveFileValidatorFactory;
class FileSystemURL;
class FileStreamReader;
class FileStreamWriter;
class FileSystemContext;
class FileSystemOperation;
class FileSystemQuotaUtil;
class WatcherManager;

enum class OperationType;

// Callback to take GURL.
using URLCallback = base::OnceCallback<void(const GURL& url)>;

// Maximum numer of bytes to be read by FileStreamReader classes. Used in
// FileSystemBackend::CreateFileStreamReader(), when it's not known how many
// bytes will be fetched in total.
const int64_t kMaximumLength = INT64_MAX;

// An interface for defining a file system backend.
//
// NOTE: when you implement a new FileSystemBackend for your own
// FileSystem module, please contact to kinuko@chromium.org.
//
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemBackend {
 public:
  // Callback for InitializeFileSystem.
  using OpenFileSystemCallback =
      base::OnceCallback<void(const FileSystemURL& root_url,
                              const std::string& name,
                              base::File::Error error)>;
  using ResolveURLCallback = base::OnceCallback<
      void(const GURL&, const std::string&, base::File::Error)>;
  virtual ~FileSystemBackend() = default;

  // Returns true if this filesystem backend can handle |type|.
  // One filesystem backend may be able to handle multiple filesystem types.
  virtual bool CanHandleType(FileSystemType type) const = 0;

  // This method is called right after the backend is registered in the
  // FileSystemContext and before any other methods are called. Each backend can
  // do additional initialization which depends on FileSystemContext here.
  virtual void Initialize(FileSystemContext* context) = 0;

  // Resolves the filesystem root URL and the name for the given |url|.
  // This verifies if it is allowed to request (or create) the filesystem and if
  // it can access (or create) the root directory.
  // If |mode| is CREATE_IF_NONEXISTENT calling this may also create the root
  // directory (and/or related database entries etc) for the filesystem if it
  // doesn't exist.
  virtual void ResolveURL(const FileSystemURL& url,
                          OpenFileSystemMode mode,
                          ResolveURLCallback callback) = 0;

  // Returns the specialized AsyncFileUtil for this backend.
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) = 0;

  // Returns the specialized WatcherManager for this backend.
  virtual WatcherManager* GetWatcherManager(FileSystemType type) = 0;

  // Returns the specialized CopyOrMoveFileValidatorFactory for this backend
  // and |type|.  If |error_code| is File::FILE_OK and the result is nullptr,
  // then no validator is required.
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) = 0;

  // Returns a new instance of the specialized FileSystemOperation for this
  // backend based on the given triplet of |origin_url|, |file_system_type|
  // and |virtual_path|. On failure to create a file system operation, set
  // |error_code| correspondingly.
  // This method is usually dispatched by
  // FileSystemContext::CreateFileSystemOperation.
  virtual std::unique_ptr<FileSystemOperation> CreateFileSystemOperation(
      OperationType type,
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const = 0;

  // Returns true if Blobs accessing |url| should use FileStreamReader.
  // If false, Blobs are accessed using a snapshot file by calling
  // AsyncFileUtil::CreateSnapshotFile.
  virtual bool SupportsStreaming(const FileSystemURL& url) const = 0;

  // Returns true if specified |type| of filesystem can handle Copy()
  // of the files in the same file system instead of streaming
  // read/write implementation.
  virtual bool HasInplaceCopyImplementation(FileSystemType type) const = 0;

  // Creates a new file stream reader for a given filesystem URL |url| with an
  // offset |offset|. |expected_modification_time| specifies the expected last
  // modification if the value is non-null, the reader will check the underlying
  // file's actual modification time to see if the file has been modified, and
  // if it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  // This method itself does *not* check if the given path exists and is a
  // regular file. At most |max_bytes_to_read| can be fetched from the file
  // stream reader. The callback `file_access` grants access to dlp restricted
  // files. Passing a NullCallback will lead to default behaviour of
  // ScopedFileAccessDelegate::RequestDefaultFilesAccessIO.
  virtual std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) const = 0;

  // Creates a new file stream writer for a given filesystem URL |url| with an
  // offset |offset|.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset,
      FileSystemContext* context) const = 0;

  // Returns the specialized FileSystemQuotaUtil for this backend.
  // This could return nullptr if this backend does not support quota.
  virtual FileSystemQuotaUtil* GetQuotaUtil() = 0;

  // Returns the update observer list for |type|. It may return nullptr when no
  // observers are added.
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const = 0;

  // Returns the change observer list for |type|. It may return nullptr when no
  // observers are added.
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const = 0;

  // Returns the access observer list for |type|. It may return nullptr when no
  // observers are added.
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_BACKEND_H_
