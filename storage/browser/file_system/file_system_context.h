// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_CONTEXT_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_request_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/open_file_system_mode.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace leveldb {
class Env;
}  // namespace leveldb

namespace storage {

class AsyncFileUtil;
class CopyOrMoveFileValidatorFactory;
class ExternalMountPoints;
class FileStreamReader;
class FileStreamWriter;
class FileSystemBackend;
class FileSystemOperation;
class FileSystemOperationRunner;
class FileSystemOptions;
class FileSystemQuotaClient;
class FileSystemQuotaUtil;
class FileSystemURL;
class IsolatedFileSystemBackend;
class MountPoints;
class QuotaClientCallbackWrapper;
class QuotaManagerProxy;
class QuotaReservation;
class SandboxFileSystemBackend;
class SandboxFileSystemBackendDelegate;
class SpecialStoragePolicy;
class WatcherManager;

enum class OperationType;

struct BucketInfo;
struct FileSystemInfo;

// An auto mount handler will attempt to mount the file system requested in
// `request_info`. If the URL is for this auto mount handler, it returns true
// and calls `callback` when the attempt is complete. If the auto mounter
// does not recognize the URL, it returns false and does not call `callback`.
// Called on the IO thread.
using URLRequestAutoMountHandler = base::RepeatingCallback<bool(
    const FileSystemRequestInfo& request_info,
    const FileSystemURL& filesystem_url,
    base::OnceCallback<void(base::File::Error result)> callback)>;

// This class keeps and provides a file system context for FileSystem API.
// An instance of this class is created and owned by profile.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemContext
    : public base::RefCountedDeleteOnSequence<FileSystemContext> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  FileSystemContext() = delete;
  FileSystemContext(const FileSystemContext&) = delete;
  FileSystemContext& operator=(const FileSystemContext&) = delete;

  // Returns file permission policy we should apply for the given `type`.
  // The return value must be bitwise-or'd of FilePermissionPolicy.
  //
  // Note: if a part of a filesystem is returned via 'Isolated' mount point,
  // its per-filesystem permission overrides the underlying filesystem's
  // permission policy.
  static int GetPermissionPolicy(FileSystemType type);

  // file_task_runner is used as default TaskRunner.
  // Unless a FileSystemBackend is overridden in CreateFileSystemOperation,
  // it is used for all file operations and file related meta operations.
  // The code assumes that file_task_runner->RunsTasksInCurrentSequence()
  // returns false if the current task is not running on the sequence that
  // allows blocking file operations (like SequencedWorkerPool implementation
  // does).
  //
  // `external_mount_points` contains non-system external mount points available
  // in the context. If not nullptr, it will be used during URL cracking.
  // `external_mount_points` may be nullptr only on platforms different from
  // ChromeOS (i.e. platforms that don't use external_mount_point_provider).
  //
  // `additional_backends` are added to the internal backend map
  // to serve filesystem requests for non-regular types.
  // If none is given, this context only handles HTML5 Sandbox FileSystem
  // and Drag-and-drop Isolated FileSystem requests.
  //
  // `auto_mount_handlers` are used to resolve calls to
  // AttemptAutoMountForURLRequest. Only external filesystems are auto mounted
  // when a filesystem: URL request is made.
  static scoped_refptr<FileSystemContext> Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      scoped_refptr<ExternalMountPoints> external_mount_points,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
      std::vector<std::unique_ptr<FileSystemBackend>> additional_backends,
      const std::vector<URLRequestAutoMountHandler>& auto_mount_handlers,
      const base::FilePath& partition_path,
      const FileSystemOptions& options);

  // Exposed for base::MakeRefCounted(). Instances should be obtained from the
  // factory method Create().
  FileSystemContext(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      scoped_refptr<ExternalMountPoints> external_mount_points,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
      std::vector<std::unique_ptr<FileSystemBackend>> additional_backends,
      const std::vector<URLRequestAutoMountHandler>& auto_mount_handlers,
      const base::FilePath& partition_path,
      const FileSystemOptions& options,
      base::PassKey<FileSystemContext>);

  // Creates a new QuotaReservation for the given `storage_key` and `type`.
  // Returns nullptr if `type` does not support quota or reservation fails.
  // This should be run on `default_file_task_runner_` and the returned value
  // should be destroyed on the runner.
  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const blink::StorageKey& storage_key,
      FileSystemType type);

  const scoped_refptr<QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  // Discards inflight operations in the operation runner.
  void Shutdown();

  // Returns a quota util for a given filesystem type.  This may
  // return nullptr if the type does not support the usage tracking or
  // it is not a quota-managed storage.
  FileSystemQuotaUtil* GetQuotaUtil(FileSystemType type) const;

  // Returns the appropriate AsyncFileUtil instance for the given `type`.
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) const;

  // Returns the appropriate CopyOrMoveFileValidatorFactory for the given
  // `type`.  If `error_code` is File::FILE_OK and the result is nullptr,
  // then no validator is required.
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) const;

  // Returns the file system backend instance for the given `type`.
  // This may return nullptr if it is given an invalid or unsupported filesystem
  // type.
  FileSystemBackend* GetFileSystemBackend(FileSystemType type) const;

  // Returns the watcher manager for the given `type`.
  // This may return nullptr if the type does not support watching.
  WatcherManager* GetWatcherManager(FileSystemType type) const;

  // Returns true for sandboxed filesystems. Currently this does
  // the same as GetQuotaUtil(type) != nullptr. (In an assumption that
  // all sandboxed filesystems must cooperate with QuotaManager so that
  // they can get deleted)
  bool IsSandboxFileSystem(FileSystemType type) const;

  // Returns observers for the given filesystem type.
  const UpdateObserverList* GetUpdateObservers(FileSystemType type) const;
  const ChangeObserverList* GetChangeObservers(FileSystemType type) const;
  const AccessObserverList* GetAccessObservers(FileSystemType type) const;

  // Returns all registered filesystem types.
  std::vector<FileSystemType> GetFileSystemTypes() const;

  // Used for OpenFileSystem.
  using OpenFileSystemCallback =
      base::OnceCallback<void(const FileSystemURL& root_url,
                              const std::string& name,
                              base::File::Error result)>;

  // Used for ResolveURL.
  enum ResolvedEntryType {
    RESOLVED_ENTRY_FILE,
    RESOLVED_ENTRY_DIRECTORY,
    RESOLVED_ENTRY_NOT_FOUND,
  };
  using ResolveURLCallback =
      base::OnceCallback<void(base::File::Error result,
                              const FileSystemInfo& info,
                              const base::FilePath& file_path,
                              ResolvedEntryType type)>;

  // Used for DeleteFileSystem.
  using StatusCallback = base::OnceCallback<void(base::File::Error result)>;

  // Opens the filesystem for the given `storage_key` and `type`, and dispatches
  // `callback` on completion.
  // If `create` is true this may actually set up a filesystem instance
  // (e.g. by creating the root directory or initializing the database
  // entry etc).
  // Provide a non-null BucketLocator to override the default storage bucket
  // for the root URL (which will be propagated to child URLs).
  void OpenFileSystem(const blink::StorageKey& storage_key,
                      const std::optional<storage::BucketLocator>& bucket,
                      FileSystemType type,
                      OpenFileSystemMode mode,
                      OpenFileSystemCallback callback);

  // Opens the filesystem for the given `url` as read-only, if the filesystem
  // backend referred by the URL allows opening by resolveURL. Otherwise it
  // fails with FILE_ERROR_SECURITY. The entry pointed by the URL can be
  // absent; in that case RESOLVED_ENTRY_NOT_FOUND type is returned to the
  // callback for indicating the absence. Can be called from any thread with
  // a message loop. `callback` is invoked on the caller thread.
  void ResolveURL(const FileSystemURL& url, ResolveURLCallback callback);

  // Attempts to mount the filesystem needed to satisfy `request_info` made from
  // `request_info.storage_domain_`. If an appropriate file system is not found,
  // callback will return an error.
  void AttemptAutoMountForURLRequest(const FileSystemRequestInfo& request_info,
                                     StatusCallback callback);

  // Deletes the filesystem for the given `storage_key` and `type`. This should
  // be called on the IO thread.
  void DeleteFileSystem(const blink::StorageKey& storage_key,
                        FileSystemType type,
                        StatusCallback callback);

  // Creates new FileStreamReader instance to read a file pointed by the given
  // filesystem URL `url` starting from `offset`. `expected_modification_time`
  // specifies the expected last modification if the value is non-null, the
  // reader will check the underlying file's actual modification time to see if
  // the file has been modified, and if it does any succeeding read operations
  // should fail with ERR_UPLOAD_FILE_CHANGED error.
  // This method internally cracks the `url`, get an appropriate
  // FileSystemBackend for the URL and call the backend's CreateFileReader.
  // The resolved FileSystemBackend could perform further specialization
  // depending on the filesystem type pointed by the `url`.
  // At most `max_bytes_to_read` can be fetched from the file stream reader.
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access = base::NullCallback());

  // Creates new FileStreamWriter instance to write into a file pointed by
  // `url` from `offset`.
  std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset);

  // Creates a new FileSystemOperationRunner. Callers have to make sure that
  // this FileSystemContext outlives the returned FileSystemOperationRunner.
  // This must be called on the IO thread.
  std::unique_ptr<FileSystemOperationRunner> CreateFileSystemOperationRunner();

  // Similar to above, but this method can be called on any thread.
  base::SequenceBound<FileSystemOperationRunner>
  CreateSequenceBoundFileSystemOperationRunner();

  base::SequencedTaskRunner* default_file_task_runner() {
    return default_file_task_runner_.get();
  }

  FileSystemOperationRunner* operation_runner() {
    return operation_runner_.get();
  }

  const base::FilePath& partition_path() const { return partition_path_; }

  // Same as `CrackFileSystemURL`, but cracks FileSystemURL created from `url`
  // and `storage_key`.
  FileSystemURL CrackURL(const GURL& url,
                         const blink::StorageKey& storage_key) const;

  // Same as `CrackFileSystemURL`, but cracks FileSystemURL created from `url`
  // and a blink::StorageKey it derives from `url`. Note: never use this
  // function to crack URLs received from web contents. For all web-exposed
  // URLs, use the CrackURL function above and pass in the StorageKey of the
  // frame or worker that provided the URL.
  FileSystemURL CrackURLInFirstPartyContext(const GURL& url) const;

  // Same as `CrackFileSystemURL`, but cracks FileSystemURL created from method
  // arguments.
  FileSystemURL CreateCrackedFileSystemURL(const blink::StorageKey& storage_key,
                                           FileSystemType type,
                                           const base::FilePath& path) const;

  SandboxFileSystemBackendDelegate* sandbox_delegate() {
    return sandbox_delegate_.get();
  }

  // Returns true if the requested url is ok to be served.
  // (E.g. this returns false if the context is created for incognito mode)
  bool CanServeURLRequest(const FileSystemURL& url) const;

  bool is_incognito() { return is_incognito_; }

  void ResolveURLOnOpenFileSystemForTesting(
      const blink::StorageKey& storage_key,
      const std::optional<storage::BucketLocator>& bucket,
      FileSystemType type,
      OpenFileSystemMode mode,
      OpenFileSystemCallback callback) {
    ResolveURLOnOpenFileSystem(storage_key, bucket, type, mode,
                               std::move(callback));
  }

 private:
  // For CreateFileSystemOperation.
  friend class FileSystemOperationRunner;

  // For sandbox_backend().
  friend class SandboxFileSystemTestHelper;

  // Deleters.
  friend class base::DeleteHelper<FileSystemContext>;
  friend class base::RefCountedDeleteOnSequence<FileSystemContext>;
  ~FileSystemContext();

  // Must be called after creating the FileSystemContext.
  void Initialize();

  // The set of quota-managed storage types covered by file system backends.
  // This may be called before the file system backends are initialized.
  base::flat_set<blink::mojom::StorageType> QuotaManagedStorageTypes();

  // Creates a new FileSystemOperation instance by getting an appropriate
  // FileSystemBackend for `url` and calling the backend's corresponding
  // CreateFileSystemOperation method.
  // The resolved FileSystemBackend could perform further specialization
  // depending on the filesystem type pointed by the `url`.
  //
  // Called by FileSystemOperationRunner.
  std::unique_ptr<FileSystemOperation> CreateFileSystemOperation(
      OperationType type,
      const FileSystemURL& url,
      base::File::Error* error_code);

  // For non-cracked isolated and external mount points, returns a FileSystemURL
  // created by cracking `url`. The url is cracked using MountPoints registered
  // as `url_crackers_`. If the url cannot be cracked, returns invalid
  // FileSystemURL.
  //
  // If the original url does not point to an isolated or external filesystem,
  // returns the original url, without attempting to crack it.
  FileSystemURL CrackFileSystemURL(const FileSystemURL& url) const;

  // For initial backend_map construction. This must be called only from
  // the constructor.
  void RegisterBackend(FileSystemBackend* backend);

  void DidOpenFileSystemForResolveURL(const FileSystemURL& url,
                                      ResolveURLCallback callback,
                                      const GURL& filesystem_root,
                                      const std::string& filesystem_name,
                                      base::File::Error error);
  void OnGetBucketForDeleteFileSystem(FileSystemType type,
                                      StatusCallback callback,
                                      QuotaErrorOr<BucketInfo> result);
  // OnGetOrCreateBucket is the callback for calling
  // QuotaManagerProxy::GetOrCreateDefault.
  void OnGetOrCreateBucket(const blink::StorageKey& storage_key,
                           FileSystemType type,
                           OpenFileSystemMode mode,
                           OpenFileSystemCallback callback,
                           QuotaErrorOr<BucketInfo> result);
  // ResolveURLOnOpenFileSystem is called, either by OnGetOrCreateBucket
  // on successful bucket creation, or (tests onlyh) by OpenFileSystem
  // directly in the absence of a quota manager.
  // `bucket` will be populated if the non-default storage bucket was used.
  void ResolveURLOnOpenFileSystem(
      const blink::StorageKey& storage_key,
      const std::optional<storage::BucketLocator>& bucket,
      FileSystemType type,
      OpenFileSystemMode mode,
      OpenFileSystemCallback callback);
  void DidResolveURLOnOpenFileSystem(const FileSystemURL& filesystem_root_url,
                                     OpenFileSystemCallback callback,
                                     const GURL& filesystem_root,
                                     const std::string& filesystem_name,
                                     base::File::Error error);

  // Returns a FileSystemBackend, used only by test code.
  SandboxFileSystemBackend* sandbox_backend() const {
    return sandbox_backend_.get();
  }

  // Override the default leveldb Env with `env_override_` if set.
  std::unique_ptr<leveldb::Env> env_override_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> default_file_task_runner_;

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<FileSystemQuotaClient> quota_client_;
  std::unique_ptr<storage::QuotaClientCallbackWrapper> quota_client_wrapper_;

  const std::unique_ptr<SandboxFileSystemBackendDelegate> sandbox_delegate_;

  // Regular file system backends.
  const std::unique_ptr<SandboxFileSystemBackend> sandbox_backend_;
  std::unique_ptr<IsolatedFileSystemBackend> isolated_backend_;

  // Additional file system backends.
  const std::vector<std::unique_ptr<FileSystemBackend>> additional_backends_;

  std::vector<URLRequestAutoMountHandler> auto_mount_handlers_;

  // Registered file system backends.
  // The map must be constructed in the constructor since it can be accessed
  // on multiple threads.
  // This map itself doesn't retain each backend's ownership; ownerships
  // of the backends are held by additional_backends_ or other scoped_ptr
  // backend fields.
  std::map<FileSystemType, raw_ptr<FileSystemBackend, CtnExperimental>>
      backend_map_;

  // External mount points visible in the file system context (excluding system
  // external mount points).
  const scoped_refptr<ExternalMountPoints> external_mount_points_;

  // MountPoints used to crack FileSystemURLs. The MountPoints are ordered
  // in order they should try to crack a FileSystemURL.
  std::vector<raw_ptr<MountPoints, VectorExperimental>> url_crackers_;

  // The base path of the storage partition for this context.
  const base::FilePath partition_path_;

  const bool is_incognito_;

  const std::unique_ptr<FileSystemOperationRunner> operation_runner_;

  std::unique_ptr<mojo::Receiver<mojom::QuotaClient>> quota_client_receiver_;

  base::WeakPtrFactory<FileSystemContext> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_CONTEXT_H_
