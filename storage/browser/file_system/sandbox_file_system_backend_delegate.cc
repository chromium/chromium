// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/quota/quota_backend_impl.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/quota/quota_reservation_manager.h"
#include "storage/browser/file_system/sandbox_file_stream_reader.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/file_system/sandbox_quota_observer.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

int64_t kMinimumStatsCollectionIntervalHours = 1;

// For type directory names in ObfuscatedFileUtil.
// TODO(kinuko,nhiroki): Each type string registration should be done
// via its own backend.
const char kTemporaryDirectoryName[] = "t";
const char kPersistentDirectoryName[] = "p";
const char kSyncableDirectoryName[] = "s";

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
const base::FilePath::CharType* const kRestrictedNames[] = {
    FILE_PATH_LITERAL("."),
    FILE_PATH_LITERAL(".."),
};

// Restricted chars.
const base::FilePath::CharType kRestrictedChars[] = {
    FILE_PATH_LITERAL('/'),
    FILE_PATH_LITERAL('\\'),
};

std::set<std::string> GetKnownTypeStrings() {
  std::set<std::string> known_type_strings;
  known_type_strings.insert(kTemporaryDirectoryName);
  known_type_strings.insert(kPersistentDirectoryName);
  known_type_strings.insert(kSyncableDirectoryName);
  return known_type_strings;
}

class SandboxObfuscatedStorageKeyEnumerator
    : public SandboxFileSystemBackendDelegate::StorageKeyEnumerator {
 public:
  explicit SandboxObfuscatedStorageKeyEnumerator(
      ObfuscatedFileUtil* file_util) {
    enum_ = file_util->CreateStorageKeyEnumerator();
  }
  ~SandboxObfuscatedStorageKeyEnumerator() override = default;

  std::optional<blink::StorageKey> Next() override { return enum_->Next(); }

  bool HasFileSystemType(FileSystemType type) const override {
    return enum_->HasTypeDirectory(
        SandboxFileSystemBackendDelegate::GetTypeString(type));
  }

 private:
  std::unique_ptr<ObfuscatedFileUtil::AbstractStorageKeyEnumerator> enum_;
};

base::File::Error OpenSandboxFileSystemOnFileTaskRunner(
    ObfuscatedFileUtil* file_util,
    const BucketLocator& bucket_locator,
    FileSystemType type,
    OpenFileSystemMode mode) {
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  return file_util->GetDirectoryForBucketAndType(bucket_locator, type, create)
      .error_or(base::File::FILE_OK);
  // The reference of file_util will be derefed on the FILE thread
  // when the storage of this callback gets deleted regardless of whether
  // this method is called or not.
}

void DidOpenFileSystem(
    base::WeakPtr<SandboxFileSystemBackendDelegate> delegate,
    base::OnceClosure quota_callback,
    base::OnceCallback<void(base::File::Error error)> callback,
    base::File::Error error) {
  if (delegate)
    delegate->CollectOpenFileSystemMetrics(error);
  if (error == base::File::FILE_OK)
    std::move(quota_callback).Run();
  std::move(callback).Run(error);
}

template <typename T>
void DeleteSoon(base::SequencedTaskRunner* runner, T* ptr) {
  if (!runner->DeleteSoon(FROM_HERE, ptr))
    delete ptr;
}

}  // namespace

// static
std::string SandboxFileSystemBackendDelegate::GetTypeString(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return kTemporaryDirectoryName;
    case kFileSystemTypePersistent:
      return kPersistentDirectoryName;
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return kSyncableDirectoryName;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED() << "Unknown filesystem type requested:" << type;
  }
}

SandboxFileSystemBackendDelegate::SandboxFileSystemBackendDelegate(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const base::FilePath& profile_path,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const FileSystemOptions& file_system_options,
    leveldb::Env* env_override)
    : file_task_runner_(std::move(file_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      sandbox_file_util_(std::make_unique<AsyncFileUtilAdapter>(
          std::make_unique<ObfuscatedFileUtil>(
              special_storage_policy,
              profile_path,
              env_override,
              GetKnownTypeStrings(),
              this,
              file_system_options.is_incognito()))),
      file_system_usage_cache_(std::make_unique<FileSystemUsageCache>(
          file_system_options.is_incognito())),
      quota_observer_(
          std::make_unique<SandboxQuotaObserver>(quota_manager_proxy_,
                                                 file_task_runner_,
                                                 obfuscated_file_util(),
                                                 usage_cache())),
      quota_reservation_manager_(std::make_unique<QuotaReservationManager>(
          std::make_unique<QuotaBackendImpl>(file_task_runner_,
                                             obfuscated_file_util(),
                                             usage_cache(),
                                             quota_manager_proxy_))),
      special_storage_policy_(std::move(special_storage_policy)),
      file_system_options_(file_system_options),
      is_filesystem_opened_(false) {}

SandboxFileSystemBackendDelegate::~SandboxFileSystemBackendDelegate() {
  DETACH_FROM_THREAD(io_thread_checker_);

  if (!file_task_runner_->RunsTasksInCurrentSequence()) {
    DeleteSoon(file_task_runner_.get(), quota_reservation_manager_.release());
    // `quota_observer_` depends on `sandbox_file_util_` and
    // `file_system_usage_cache_` so it must be released first.
    DeleteSoon(file_task_runner_.get(), quota_observer_.release());
    // Clear pointer to |this| to avoid holding a dangling ptr.
    obfuscated_file_util()->sandbox_delegate_ = nullptr;
    DeleteSoon(file_task_runner_.get(), sandbox_file_util_.release());
    DeleteSoon(file_task_runner_.get(), file_system_usage_cache_.release());
  }
}

SandboxFileSystemBackendDelegate::StorageKeyEnumerator*
SandboxFileSystemBackendDelegate::CreateStorageKeyEnumerator() {
  return new SandboxObfuscatedStorageKeyEnumerator(obfuscated_file_util());
}

base::FilePath
SandboxFileSystemBackendDelegate::GetBaseDirectoryForStorageKeyAndType(
    const blink::StorageKey& storage_key,
    FileSystemType type,
    bool create) {
  base::FileErrorOr<base::FilePath> path =
      obfuscated_file_util()->GetDirectoryForStorageKeyAndType(storage_key,
                                                               type, create);
  return path.value_or(base::FilePath());
}

base::FilePath
SandboxFileSystemBackendDelegate::GetBaseDirectoryForBucketAndType(
    const BucketLocator& bucket_locator,
    FileSystemType type,
    bool create) {
  base::FileErrorOr<base::FilePath> path =
      obfuscated_file_util()->GetDirectoryForBucketAndType(bucket_locator, type,
                                                           create);
  return path.value_or(base::FilePath());
}

void SandboxFileSystemBackendDelegate::OpenFileSystem(
    const BucketLocator& bucket_locator,
    FileSystemType type,
    OpenFileSystemMode mode,
    ResolveURLCallback callback,
    const GURL& root_url) {
  if (!IsAllowedScheme(bucket_locator.storage_key.origin().GetURL())) {
    std::move(callback).Run(GURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  std::string name =
      GetFileSystemName(bucket_locator.storage_key.origin().GetURL(), type);

  // |quota_manager_proxy_| may be null in unit tests.
  base::OnceClosure quota_callback;
  if (quota_manager_proxy_) {
    quota_callback =
        base::BindOnce(&QuotaManagerProxy::NotifyBucketAccessed,
                       quota_manager_proxy_, bucket_locator, base::Time::Now());
  } else {
    quota_callback = base::DoNothing();
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OpenSandboxFileSystemOnFileTaskRunner,
                     obfuscated_file_util(), bucket_locator, type, mode),
      base::BindOnce(&DidOpenFileSystem, weak_factory_.GetWeakPtr(),
                     std::move(quota_callback),
                     base::BindOnce(std::move(callback), root_url, name)));

  DETACH_FROM_THREAD(io_thread_checker_);
  is_filesystem_opened_ = true;
}

std::unique_ptr<FileSystemOperationContext>
SandboxFileSystemBackendDelegate::CreateFileSystemOperationContext(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  if (!IsAccessValid(url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return nullptr;
  }

  const UpdateObserverList* update_observers = GetUpdateObservers(url.type());
  const ChangeObserverList* change_observers = GetChangeObservers(url.type());
  DCHECK(update_observers);

  auto operation_context =
      std::make_unique<FileSystemOperationContext>(context);
  operation_context->set_update_observers(*update_observers);
  operation_context->set_change_observers(
      change_observers ? *change_observers : ChangeObserverList());

  return operation_context;
}

std::unique_ptr<FileStreamReader>
SandboxFileSystemBackendDelegate::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  if (!IsAccessValid(url))
    return nullptr;
  return std::make_unique<SandboxFileStreamReader>(context, url, offset,
                                                   expected_modification_time);
}

std::unique_ptr<FileStreamWriter>
SandboxFileSystemBackendDelegate::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context,
    FileSystemType type) const {
  if (!IsAccessValid(url))
    return nullptr;
  const UpdateObserverList* observers = GetUpdateObservers(type);
  DCHECK(observers);
  return std::make_unique<SandboxFileStreamWriter>(context, url, offset,
                                                   *observers);
}

void SandboxFileSystemBackendDelegate::DeleteCachedDefaultBucket(
    const blink::StorageKey& storage_key) {
  // Delete cache for the default bucket in obfuscated_file_util() if one
  // exists. NOTE: one StorageKey may map to many BucketLocators depending on
  // the type. We only want to cache and delete kFileSystemTypeTemporary
  // buckets. Otherwise, we may accidentally delete the wrong databases.
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  obfuscated_file_util()->DeleteDefaultBucketForStorageKey(storage_key);
}

base::File::Error
SandboxFileSystemBackendDelegate::DeleteBucketDataOnFileTaskRunner(
    FileSystemContext* file_system_context,
    QuotaManagerProxy* proxy,
    const BucketLocator& bucket_locator,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  int64_t usage = (proxy) ? GetBucketUsageOnFileTaskRunner(file_system_context,
                                                           bucket_locator, type)
                          : 0;
  usage_cache()->CloseCacheFiles();
  bool result = obfuscated_file_util()->DeleteDirectoryForBucketAndType(
      bucket_locator, type);
  if (result && proxy && usage) {
    proxy->NotifyBucketModified(
        QuotaClientType::kFileSystem, bucket_locator, -usage, base::Time::Now(),
        base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  }

  if (result)
    return base::File::FILE_OK;
  return base::File::FILE_ERROR_FAILED;
}

void SandboxFileSystemBackendDelegate::PerformStorageCleanupOnFileTaskRunner(
    FileSystemContext* context,
    QuotaManagerProxy* proxy,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  obfuscated_file_util()->RewriteDatabases();
}

std::vector<blink::StorageKey>
SandboxFileSystemBackendDelegate::GetStorageKeysForTypeOnFileTaskRunner(
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<StorageKeyEnumerator> enumerator(
      CreateStorageKeyEnumerator());
  std::vector<blink::StorageKey> storage_keys;
  std::optional<blink::StorageKey> storage_key;
  while ((storage_key = enumerator->Next()).has_value()) {
    if (enumerator->HasFileSystemType(type))
      storage_keys.push_back(std::move(storage_key).value());
  }
  return storage_keys;
}

int64_t SandboxFileSystemBackendDelegate::GetBucketUsageOnFileTaskRunner(
    FileSystemContext* file_system_context,
    const BucketLocator& bucket_locator,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  if (base::Contains(
          sticky_dirty_origins_,
          std::make_pair(bucket_locator.storage_key.origin(), type))) {
    return RecalculateBucketUsage(file_system_context, bucket_locator, type);
  }

  base::FilePath path =
      GetBaseDirectoryForBucketAndType(bucket_locator, type, false);
  if (!obfuscated_file_util()->delegate()->DirectoryExists(path)) {
    return 0;
  }
  base::FilePath usage_file_path =
      path.Append(FileSystemUsageCache::kUsageFileName);

  bool is_valid = usage_cache()->IsValid(usage_file_path);
  uint32_t dirty_status = 0;
  bool dirty_status_available =
      usage_cache()->GetDirty(usage_file_path, &dirty_status);
  bool visited =
      !visited_origins_.insert(bucket_locator.storage_key.origin()).second;
  if (is_valid && (dirty_status == 0 || (dirty_status_available && visited))) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    int64_t usage = 0;
    return usage_cache()->GetUsage(usage_file_path, &usage) ? usage : -1;
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  usage_cache()->Delete(usage_file_path);

  int64_t usage =
      RecalculateBucketUsage(file_system_context, bucket_locator, type);

  // This clears the dirty flag too.
  usage_cache()->UpdateUsage(usage_file_path, usage);
  return usage;
}

scoped_refptr<QuotaReservation>
SandboxFileSystemBackendDelegate::CreateQuotaReservationOnFileTaskRunner(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(quota_reservation_manager_);
  return quota_reservation_manager_->CreateReservation(storage_key.origin(),
                                                       type);
}

void SandboxFileSystemBackendDelegate::AddFileUpdateObserver(
    FileSystemType type,
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  update_observers_[type] =
      update_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  change_observers_[type] =
      change_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileAccessObserver(
    FileSystemType type,
    FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  access_observers_[type] =
      access_observers_[type].AddObserver(observer, task_runner);
}

const UpdateObserverList* SandboxFileSystemBackendDelegate::GetUpdateObservers(
    FileSystemType type) const {
  auto iter = update_observers_.find(type);
  if (iter == update_observers_.end())
    return nullptr;
  return &iter->second;
}

const ChangeObserverList* SandboxFileSystemBackendDelegate::GetChangeObservers(
    FileSystemType type) const {
  auto iter = change_observers_.find(type);
  if (iter == change_observers_.end())
    return nullptr;
  return &iter->second;
}

const AccessObserverList* SandboxFileSystemBackendDelegate::GetAccessObservers(
    FileSystemType type) const {
  auto iter = access_observers_.find(type);
  if (iter == access_observers_.end())
    return nullptr;
  return &iter->second;
}

void SandboxFileSystemBackendDelegate::RegisterQuotaUpdateObserver(
    FileSystemType type) {
  AddFileUpdateObserver(type, quota_observer_.get(), file_task_runner_.get());
}

void SandboxFileSystemBackendDelegate::InvalidateUsageCache(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  ASSIGN_OR_RETURN(base::FilePath usage_file_path,
                   GetUsageCachePathForStorageKeyAndType(obfuscated_file_util(),
                                                         storage_key, type),
                   [](auto) {});
  usage_cache()->IncrementDirty(std::move(usage_file_path));
}

void SandboxFileSystemBackendDelegate::StickyInvalidateUsageCache(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  sticky_dirty_origins_.insert(std::make_pair(storage_key.origin(), type));
  quota_observer()->SetUsageCacheEnabled(storage_key.origin(), type, false);
  InvalidateUsageCache(storage_key, type);
}

FileSystemFileUtil* SandboxFileSystemBackendDelegate::sync_file_util() {
  return static_cast<AsyncFileUtilAdapter*>(file_util())->sync_file_util();
}

bool SandboxFileSystemBackendDelegate::IsAccessValid(
    const FileSystemURL& url) const {
  if (!IsAllowedScheme(url.origin().GetURL()))
    return false;

  if (url.path().ReferencesParent())
    return false;

  // Return earlier if the path is '/', because VirtualPath::BaseName()
  // returns '/' for '/' and we fail the "basename != '/'" check below.
  // (We exclude '.' because it's disallowed by spec.)
  if (VirtualPath::IsRootPath(url.path()) &&
      url.path() != base::FilePath(base::FilePath::kCurrentDirectory))
    return true;

  // Restricted names specified in
  // http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
  base::FilePath filename = VirtualPath::BaseName(url.path());
  // See if the name is allowed to create.
  for (const auto* const restricted_name : kRestrictedNames) {
    if (filename.value() == restricted_name)
      return false;
  }
  for (const auto& restricted_char : kRestrictedChars) {
    if (filename.value().find(restricted_char) !=
        base::FilePath::StringType::npos)
      return false;
  }

  return true;
}

bool SandboxFileSystemBackendDelegate::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  if (url.SchemeIsHTTPOrHTTPS())
    return true;
  if (url.SchemeIsFileSystem())
    return url.inner_url() && IsAllowedScheme(*url.inner_url());

  for (size_t i = 0;
       i < file_system_options_.additional_allowed_schemes().size(); ++i) {
    if (url.SchemeIs(
            file_system_options_.additional_allowed_schemes()[i].c_str()))
      return true;
  }
  return false;
}

base::FileErrorOr<base::FilePath>
SandboxFileSystemBackendDelegate::GetUsageCachePathForStorageKeyAndType(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  return GetUsageCachePathForStorageKeyAndType(obfuscated_file_util(),
                                               storage_key, type);
}

// static
base::FileErrorOr<base::FilePath>
SandboxFileSystemBackendDelegate::GetUsageCachePathForStorageKeyAndType(
    ObfuscatedFileUtil* sandbox_file_util,
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  ASSIGN_OR_RETURN(base::FilePath base_path,
                   sandbox_file_util->GetDirectoryForStorageKeyAndType(
                       storage_key, type, false /* create */));
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

base::FileErrorOr<base::FilePath>
SandboxFileSystemBackendDelegate::GetUsageCachePathForBucketAndType(
    const BucketLocator& bucket_locator,
    FileSystemType type) {
  return GetUsageCachePathForBucketAndType(obfuscated_file_util(),
                                           bucket_locator, type);
}

// static
base::FileErrorOr<base::FilePath>
SandboxFileSystemBackendDelegate::GetUsageCachePathForBucketAndType(
    ObfuscatedFileUtil* sandbox_file_util,
    const BucketLocator& bucket_locator,
    FileSystemType type) {
  ASSIGN_OR_RETURN(
      base::FilePath base_path,
      sandbox_file_util->GetDirectoryForBucketAndType(bucket_locator, type,
                                                      /*create=*/false));
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

int64_t SandboxFileSystemBackendDelegate::RecalculateBucketUsage(
    FileSystemContext* context,
    const BucketLocator& bucket_locator,
    FileSystemType type) {
  FileSystemOperationContext operation_context(context);
  FileSystemURL url = context->CreateCrackedFileSystemURL(
      bucket_locator.storage_key, type, base::FilePath());
  url.SetBucket(bucket_locator);
  std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      obfuscated_file_util()->CreateFileEnumerator(&operation_context, url,
                                                   true));

  base::FilePath file_path_each;
  int64_t usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }

  return usage;
}

void SandboxFileSystemBackendDelegate::CollectOpenFileSystemMetrics(
    base::File::Error error_code) {
  base::Time now = base::Time::Now();
  bool throttled = now < next_release_time_for_open_filesystem_stat_;
  if (!throttled) {
    next_release_time_for_open_filesystem_stat_ =
        now + base::Hours(kMinimumStatsCollectionIntervalHours);
  }
}

ObfuscatedFileUtil* SandboxFileSystemBackendDelegate::obfuscated_file_util() {
  return static_cast<ObfuscatedFileUtil*>(sync_file_util());
}

base::WeakPtr<ObfuscatedFileUtilMemoryDelegate>
SandboxFileSystemBackendDelegate::memory_file_util_delegate() {
  DCHECK(obfuscated_file_util()->is_incognito());
  return static_cast<ObfuscatedFileUtilMemoryDelegate*>(
             obfuscated_file_util()->delegate())
      ->GetWeakPtr();
}

// Declared in obfuscated_file_util.h.
// static
std::unique_ptr<ObfuscatedFileUtil> ObfuscatedFileUtil::CreateForTesting(
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const base::FilePath& profile_path,
    leveldb::Env* env_override,
    bool is_incognito) {
  return std::make_unique<ObfuscatedFileUtil>(
      std::move(special_storage_policy), profile_path, env_override,
      GetKnownTypeStrings(), /*sandbox_delegate=*/nullptr, is_incognito);
}

}  // namespace storage
