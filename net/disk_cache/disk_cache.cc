// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_file_enumerator.h"
#include "net/disk_cache/simple/simple_util.h"

namespace {

using FileEnumerator = disk_cache::BackendFileOperations::FileEnumerator;
using ApplicationStatusListenerGetter =
    disk_cache::ApplicationStatusListenerGetter;

// Builds an instance of the backend depending on platform, type, experiments
// etc. Takes care of the retry state. This object will self-destroy when
// finished.
class CacheCreator {
 public:
  CacheCreator(const base::FilePath& path,
               disk_cache::ResetHandling reset_handling,
               int64_t max_bytes,
               net::CacheType type,
               net::BackendType backend_type,
               scoped_refptr<disk_cache::BackendFileOperationsFactory>
                   file_operations_factory,
#if BUILDFLAG(IS_ANDROID)
               ApplicationStatusListenerGetter app_status_listener_getter,
#endif
               net::NetLog* net_log,
               base::OnceClosure post_cleanup_callback,
               disk_cache::BackendResultCallback callback);

  CacheCreator(const CacheCreator&) = delete;
  CacheCreator& operator=(const CacheCreator&) = delete;

  // Wait for any previous backends for given path to finish clean up and then
  // attempt to create a new one. This will never succeed synchronously, though
  // it may fail synchronously.
  net::Error TryCreateCleanupTrackerAndRun();

  // Creates the backend, the cleanup context for it having been already
  // established... or purposefully left as null. This will never succeed
  // synchronously, though it may fail synchronously.
  net::Error Run();

 private:
  ~CacheCreator();

  void DoCallback(int result);

  void OnIOComplete(int result);
  void OnCacheCleanupComplete(int original_error, bool cleanup_result);

  const base::FilePath path_;
  disk_cache::ResetHandling reset_handling_;
  bool retry_ = false;
  int64_t max_bytes_;
  net::CacheType type_;
  net::BackendType backend_type_;
  scoped_refptr<disk_cache::BackendFileOperationsFactory>
      file_operations_factory_;
  std::unique_ptr<disk_cache::BackendFileOperations> file_operations_;
#if BUILDFLAG(IS_ANDROID)
  ApplicationStatusListenerGetter app_status_listener_getter_;
#endif
  base::OnceClosure post_cleanup_callback_;
  disk_cache::BackendResultCallback callback_;
  std::unique_ptr<disk_cache::Backend> created_cache_;
  raw_ptr<net::NetLog> net_log_;
  scoped_refptr<disk_cache::BackendCleanupTracker> cleanup_tracker_;
};

CacheCreator::CacheCreator(
    const base::FilePath& path,
    disk_cache::ResetHandling reset_handling,
    int64_t max_bytes,
    net::CacheType type,
    net::BackendType backend_type,
    scoped_refptr<disk_cache::BackendFileOperationsFactory> file_operations,
#if BUILDFLAG(IS_ANDROID)
    ApplicationStatusListenerGetter app_status_listener_getter,
#endif
    net::NetLog* net_log,
    base::OnceClosure post_cleanup_callback,
    disk_cache::BackendResultCallback callback)
    : path_(path),
      reset_handling_(reset_handling),
      max_bytes_(max_bytes),
      type_(type),
      backend_type_(backend_type),
      file_operations_factory_(std::move(file_operations)),
#if BUILDFLAG(IS_ANDROID)
      app_status_listener_getter_(std::move(app_status_listener_getter)),
#endif
      post_cleanup_callback_(std::move(post_cleanup_callback)),
      callback_(std::move(callback)),
      net_log_(net_log) {
}

CacheCreator::~CacheCreator() = default;

net::Error CacheCreator::Run() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  static const bool kSimpleBackendIsDefault = true;
#else
  static const bool kSimpleBackendIsDefault = false;
#endif
  if (!retry_ && reset_handling_ == disk_cache::ResetHandling::kReset) {
    // Pretend that we failed to create a cache, so that we can handle `kReset`
    // and `kResetOnError` in a unified way, in CacheCreator::OnIOComplete.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CacheCreator::OnIOComplete,
                                  base::Unretained(this), net::ERR_FAILED));
    return net::ERR_IO_PENDING;
  }
  if (backend_type_ == net::CACHE_BACKEND_SIMPLE ||
      (backend_type_ == net::CACHE_BACKEND_DEFAULT &&
       kSimpleBackendIsDefault)) {
    auto cache = std::make_unique<disk_cache::SimpleBackendImpl>(
        file_operations_factory_, path_, cleanup_tracker_.get(),
        /* file_tracker = */ nullptr, max_bytes_, type_, net_log_);
    disk_cache::SimpleBackendImpl* simple_cache = cache.get();
    created_cache_ = std::move(cache);
#if BUILDFLAG(IS_ANDROID)
    if (app_status_listener_getter_) {
      simple_cache->set_app_status_listener_getter(app_status_listener_getter_);
    }
#endif
    simple_cache->Init(
        base::BindOnce(&CacheCreator::OnIOComplete, base::Unretained(this)));
    return net::ERR_IO_PENDING;
  }

// Avoid references to blockfile functions on Android to reduce binary size.
#if BUILDFLAG(IS_ANDROID)
  return net::ERR_FAILED;
#else
  auto cache = std::make_unique<disk_cache::BackendImpl>(
      path_, cleanup_tracker_.get(),
      /*cache_thread = */ nullptr, type_, net_log_);
  disk_cache::BackendImpl* new_cache = cache.get();
  created_cache_ = std::move(cache);
  new_cache->SetMaxSize(max_bytes_);
  new_cache->Init(
      base::BindOnce(&CacheCreator::OnIOComplete, base::Unretained(this)));
  return net::ERR_IO_PENDING;
#endif
}

net::Error CacheCreator::TryCreateCleanupTrackerAndRun() {
  // Before creating a cache Backend, a BackendCleanupTracker object is needed
  // so there is a place to keep track of outstanding I/O even after the backend
  // object itself is destroyed, so that further use of the directory
  // doesn't race with those outstanding disk I/O ops.

  // This method's purpose it to grab exlusive ownership of a fresh
  // BackendCleanupTracker for the cache path, and then move on to Run(),
  // which will take care of creating the actual cache backend. It's possible
  // that something else is currently making use of the directory, in which
  // case BackendCleanupTracker::TryCreate will fail, but will just have
  // TryCreateCleanupTrackerAndRun run again at an opportune time to make
  // another attempt.

  // The resulting BackendCleanupTracker is stored into a scoped_refptr member
  // so that it's kept alive while |this| CacheCreator exists , so that in the
  // case Run() needs to retry Backend creation the same BackendCleanupTracker
  // is used for both attempts, and |post_cleanup_callback_| gets called after
  // the second try, not the first one.
  cleanup_tracker_ = disk_cache::BackendCleanupTracker::TryCreate(
      path_, base::BindOnce(base::IgnoreResult(
                                &CacheCreator::TryCreateCleanupTrackerAndRun),
                            base::Unretained(this)));
  if (!cleanup_tracker_)
    return net::ERR_IO_PENDING;
  if (!post_cleanup_callback_.is_null())
    cleanup_tracker_->AddPostCleanupCallback(std::move(post_cleanup_callback_));
  return Run();
}

void CacheCreator::DoCallback(int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  disk_cache::BackendResult result;
  if (net_error == net::OK) {
    result = disk_cache::BackendResult::Make(std::move(created_cache_));
  } else {
    LOG(ERROR) << "Unable to create cache";
    result = disk_cache::BackendResult::MakeError(
        static_cast<net::Error>(net_error));
    created_cache_.reset();
  }
  std::move(callback_).Run(std::move(result));
  delete this;
}

// If the initialization of the cache fails, and |reset_handling| isn't set to
// kNeverReset, we will discard the whole cache and create a new one.
void CacheCreator::OnIOComplete(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (result == net::OK ||
      reset_handling_ == disk_cache::ResetHandling::kNeverReset || retry_) {
    return DoCallback(result);
  }

  // We are supposed to try again, so delete the object and all files and do so.
  retry_ = true;
  created_cache_.reset();

  if (!file_operations_) {
    if (file_operations_factory_) {
      file_operations_ = file_operations_factory_->Create(
          base::SequencedTaskRunner::GetCurrentDefault());
    } else {
      file_operations_ = std::make_unique<disk_cache::TrivialFileOperations>();
    }
  }
  file_operations_->CleanupDirectory(
      path_, base::BindOnce(&CacheCreator::OnCacheCleanupComplete,
                            base::Unretained(this), result));
}

void CacheCreator::OnCacheCleanupComplete(int original_result,
                                          bool cleanup_result) {
  if (!cleanup_result) {
    // Cleaning up the cache directory fails, so this operation should be
    // considered failed.
    DCHECK_NE(original_result, net::OK);
    DCHECK_NE(original_result, net::ERR_IO_PENDING);
    DoCallback(original_result);
    return;
  }

  // The worker thread may be deleting files, but the original folder
  // is not there anymore... let's create a new set of files.
  int rv = Run();
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

class TrivialFileEnumerator final : public FileEnumerator {
 public:
  using FileEnumerationEntry =
      disk_cache::BackendFileOperations::FileEnumerationEntry;

  explicit TrivialFileEnumerator(const base::FilePath& path)
      : enumerator_(path) {}
  ~TrivialFileEnumerator() override = default;

  std::optional<FileEnumerationEntry> Next() override {
    return enumerator_.Next();
  }
  bool HasError() const override { return enumerator_.HasError(); }

 private:
  disk_cache::SimpleFileEnumerator enumerator_;
};

class UnboundTrivialFileOperations
    : public disk_cache::UnboundBackendFileOperations {
 public:
  std::unique_ptr<disk_cache::BackendFileOperations> Bind(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override {
    return std::make_unique<disk_cache::TrivialFileOperations>();
  }
};

}  // namespace

namespace disk_cache {

BackendResult::BackendResult() = default;
BackendResult::~BackendResult() = default;
BackendResult::BackendResult(BackendResult&&) = default;
BackendResult& BackendResult::operator=(BackendResult&&) = default;

// static
BackendResult BackendResult::MakeError(net::Error error_in) {
  DCHECK_NE(error_in, net::OK);
  BackendResult result;
  result.net_error = error_in;
  return result;
}

// static
BackendResult BackendResult::Make(std::unique_ptr<Backend> backend_in) {
  DCHECK(backend_in);
  BackendResult result;
  result.net_error = net::OK;
  result.backend = std::move(backend_in);
  return result;
}

BackendResult CreateCacheBackendImpl(
    net::CacheType type,
    net::BackendType backend_type,
    scoped_refptr<BackendFileOperationsFactory> file_operations,
    const base::FilePath& path,
    int64_t max_bytes,
    ResetHandling reset_handling,
#if BUILDFLAG(IS_ANDROID)
    ApplicationStatusListenerGetter app_status_listener_getter,
#endif
    net::NetLog* net_log,
    base::OnceClosure post_cleanup_callback,
    BackendResultCallback callback) {
  DCHECK(!callback.is_null());

  if (type == net::MEMORY_CACHE) {
    std::unique_ptr<MemBackendImpl> mem_backend_impl =
        disk_cache::MemBackendImpl::CreateBackend(max_bytes, net_log);
    if (mem_backend_impl) {
      mem_backend_impl->SetPostCleanupCallback(
          std::move(post_cleanup_callback));
      return BackendResult::Make(std::move(mem_backend_impl));
    } else {
      if (!post_cleanup_callback.is_null())
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(post_cleanup_callback));
      return BackendResult::MakeError(net::ERR_FAILED);
    }
  }

  bool had_post_cleanup_callback = !post_cleanup_callback.is_null();
  CacheCreator* creator = new CacheCreator(
      path, reset_handling, max_bytes, type, backend_type,
      std::move(file_operations),
#if BUILDFLAG(IS_ANDROID)
      std::move(app_status_listener_getter),
#endif
      net_log, std::move(post_cleanup_callback), std::move(callback));
  if (type == net::DISK_CACHE) {
    DCHECK(!had_post_cleanup_callback);
    return BackendResult::MakeError(creator->Run());
  }

  return BackendResult::MakeError(creator->TryCreateCleanupTrackerAndRun());
}

BackendResult CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    scoped_refptr<BackendFileOperationsFactory> file_operations,
    const base::FilePath& path,
    int64_t max_bytes,
    ResetHandling reset_handling,
    net::NetLog* net_log,
    BackendResultCallback callback) {
  return CreateCacheBackendImpl(type, backend_type, std::move(file_operations),
                                path, max_bytes, reset_handling,
#if BUILDFLAG(IS_ANDROID)
                                ApplicationStatusListenerGetter(),
#endif
                                net_log, base::OnceClosure(),
                                std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
NET_EXPORT BackendResult
CreateCacheBackend(net::CacheType type,
                   net::BackendType backend_type,
                   scoped_refptr<BackendFileOperationsFactory> file_operations,
                   const base::FilePath& path,
                   int64_t max_bytes,
                   ResetHandling reset_handling,
                   net::NetLog* net_log,
                   BackendResultCallback callback,
                   ApplicationStatusListenerGetter app_status_listener_getter) {
  return CreateCacheBackendImpl(type, backend_type, std::move(file_operations),
                                path, max_bytes, reset_handling,
                                std::move(app_status_listener_getter), net_log,
                                base::OnceClosure(), std::move(callback));
}
#endif

BackendResult CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    scoped_refptr<BackendFileOperationsFactory> file_operations,
    const base::FilePath& path,
    int64_t max_bytes,
    ResetHandling reset_handling,
    net::NetLog* net_log,
    base::OnceClosure post_cleanup_callback,
    BackendResultCallback callback) {
  return CreateCacheBackendImpl(type, backend_type, std::move(file_operations),
                                path, max_bytes, reset_handling,
#if BUILDFLAG(IS_ANDROID)
                                ApplicationStatusListenerGetter(),
#endif
                                net_log, std::move(post_cleanup_callback),
                                std::move(callback));
}

void FlushCacheThreadForTesting() {
  // For simple backend.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Block backend.
  BackendImpl::FlushForTesting();
}

void FlushCacheThreadAsynchronouslyForTesting(base::OnceClosure callback) {
  auto repeating_callback = base::BarrierClosure(2, std::move(callback));

  // For simple backend.
  base::ThreadPoolInstance::Get()->FlushAsyncForTesting(  // IN-TEST
      base::BindPostTaskToCurrentDefault(repeating_callback));

  // Block backend.
  BackendImpl::FlushAsynchronouslyForTesting(repeating_callback);
}

int64_t Backend::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

uint8_t Backend::GetEntryInMemoryData(const std::string& key) {
  return 0;
}

void Backend::SetEntryInMemoryData(const std::string& key, uint8_t data) {}

EntryResult::EntryResult() = default;
EntryResult::~EntryResult() = default;

EntryResult::EntryResult(EntryResult&& other) {
  net_error_ = other.net_error_;
  entry_ = std::move(other.entry_);
  opened_ = other.opened_;

  other.net_error_ = net::ERR_FAILED;
  other.opened_ = false;
}

EntryResult& EntryResult::operator=(EntryResult&& other) {
  net_error_ = other.net_error_;
  entry_ = std::move(other.entry_);
  opened_ = other.opened_;

  other.net_error_ = net::ERR_FAILED;
  other.opened_ = false;
  return *this;
}

// static
EntryResult EntryResult::MakeOpened(Entry* new_entry) {
  DCHECK(new_entry);

  EntryResult result;
  result.net_error_ = net::OK;
  result.entry_.reset(new_entry);
  result.opened_ = true;
  return result;
}

// static
EntryResult EntryResult::MakeCreated(Entry* new_entry) {
  DCHECK(new_entry);

  EntryResult result;
  result.net_error_ = net::OK;
  result.entry_.reset(new_entry);
  result.opened_ = false;
  return result;
}

// static
EntryResult EntryResult::MakeError(net::Error status) {
  DCHECK_NE(status, net::OK);

  EntryResult result;
  result.net_error_ = status;
  return result;
}

Entry* EntryResult::ReleaseEntry() {
  Entry* ret = entry_.release();
  net_error_ = net::ERR_FAILED;
  opened_ = false;
  return ret;
}

TrivialFileOperations::TrivialFileOperations() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TrivialFileOperations::~TrivialFileOperations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TrivialFileOperations::CreateDirectory(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  // This is needed for some unittests.
  if (path.empty()) {
    return false;
  }

  DCHECK(path.IsAbsolute());

  bool result = base::CreateDirectory(path);
  return result;
}

bool TrivialFileOperations::PathExists(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  // This is needed for some unittests.
  if (path.empty()) {
    return false;
  }

  DCHECK(path.IsAbsolute());

  bool result = base::PathExists(path);
  return result;
}

bool TrivialFileOperations::DirectoryExists(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  bool result = base::DirectoryExists(path);
  return result;
}

base::File TrivialFileOperations::OpenFile(const base::FilePath& path,
                                           uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  base::File file(path, flags);
  return file;
}

bool TrivialFileOperations::DeleteFile(const base::FilePath& path,
                                       DeleteFileMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  bool result = false;
  switch (mode) {
    case DeleteFileMode::kDefault:
      result = base::DeleteFile(path);
      break;
    case DeleteFileMode::kEnsureImmediateAvailability:
      result = disk_cache::simple_util::SimpleCacheDeleteFile(path);
      break;
  }
  return result;
}

bool TrivialFileOperations::ReplaceFile(const base::FilePath& from_path,
                                        const base::FilePath& to_path,
                                        base::File::Error* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(from_path.IsAbsolute());
  DCHECK(to_path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  return base::ReplaceFile(from_path, to_path, error);
}

std::optional<base::File::Info> TrivialFileOperations::GetFileInfo(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  base::File::Info file_info;
  if (!base::GetFileInfo(path, &file_info)) {
    return std::nullopt;
  }
  return file_info;
}

std::unique_ptr<FileEnumerator> TrivialFileOperations::EnumerateFiles(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif
  return std::make_unique<TrivialFileEnumerator>(path);
}

void TrivialFileOperations::CleanupDirectory(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is needed for some unittests.
  if (path.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  disk_cache::CleanupDirectory(path, std::move(callback));
}

std::unique_ptr<UnboundBackendFileOperations> TrivialFileOperations::Unbind() {
#if DCHECK_IS_ON()
  DCHECK(bound_);
  bound_ = false;
#endif
  return std::make_unique<UnboundTrivialFileOperations>();
}

TrivialFileOperationsFactory::TrivialFileOperationsFactory() = default;
TrivialFileOperationsFactory::~TrivialFileOperationsFactory() = default;

std::unique_ptr<BackendFileOperations> TrivialFileOperationsFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<TrivialFileOperations>();
}

std::unique_ptr<UnboundBackendFileOperations>
TrivialFileOperationsFactory::CreateUnbound() {
  return std::make_unique<UnboundTrivialFileOperations>();
}

}  // namespace disk_cache
