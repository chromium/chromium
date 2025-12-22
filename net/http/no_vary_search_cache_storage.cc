// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_storage.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/pickle.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "net/base/pickle.h"
#include "net/base/pickle_base_types.h"
#include "net/base/pickle_traits.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/no_vary_search_cache.h"
#include "net/http/no_vary_search_cache_storage_file_operations.h"

namespace net {

namespace {

using FileOperations = NoVarySearchCacheStorageFileOperations;

// Efficiently creates an HttpNoVarySearchData object whose contents don't
// matter because it is going to be overwritten anyway.
HttpNoVarySearchData CreateHttpNoVarySearchData() {
  return HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
}

// Persists `pickle` to kSnapshotFilename with kSnapshotMagicNumber using
// `operations`.
bool PersistPickleAsSnapshot(FileOperations* operations, base::Pickle pickle) {
  auto magic =
      base::U32ToBigEndian(NoVarySearchCacheStorage::kSnapshotMagicNumber);
  auto result = operations->AtomicSave(
      NoVarySearchCacheStorage::kSnapshotFilename, {magic, pickle});
  if (!result.has_value()) {
    base::UmaHistogramExactLinear("HttpCache.NoVarySearch.SnapshotSaveError",
                                  -result.error(), -base::File::FILE_ERROR_MAX);
    return false;
  }

  return true;
}

// Serializes and persists `cache` using `operations`. Returns the size of the
// created file on success, or zero on error.
size_t PersistCache(FileOperations* operations,
                    const NoVarySearchCache& cache) {
  base::Pickle pickle;
  WriteToPickle(pickle, cache);
  const size_t snapshot_size = pickle.size();
  return PersistPickleAsSnapshot(operations, std::move(pickle)) ? snapshot_size
                                                                : 0;
}

// Do not modify this enum without changing kJournalMagicNumber.
enum class JournalEntryType : uint32_t {
  kInsert = 0,
  kErase = 1,
};

}  // namespace

// Make JournalEntryType serializable.
template <>
struct PickleTraits<JournalEntryType> {
  static void Serialize(base::Pickle& pickle, const JournalEntryType& value) {
    WriteToPickle(pickle, std::to_underlying(value));
  }

  static std::optional<JournalEntryType> Deserialize(
      base::PickleIterator& iter) {
    auto underlying =
        ReadValueFromPickle<std::underlying_type_t<JournalEntryType>>(iter);
    if (!underlying) {
      return std::nullopt;
    }
    JournalEntryType value = static_cast<JournalEntryType>(underlying.value());
    if (value != JournalEntryType::kInsert &&
        value != JournalEntryType::kErase) {
      return std::nullopt;
    }
    return value;
  }

  static size_t PickleSize(const JournalEntryType& value) {
    return EstimatePickleSize(std::to_underlying(value));
  }
};

// Performs journalling operations on the background sequence on behalf of
// NoVarySearchCacheStorage. Is created, destroyed, and called exclusively on
// the background sequence.
class NoVarySearchCacheStorage::Journaller final {
 public:
  enum CreateResult {
    kSuccess,
    kCouldntCreateJournal,
    kCouldntStartJournal,
  };

  // Journal using `operations` for `storage_ptr`, which should be notified
  // about important events by posting tasks to `parent_sequence`.
  // `snapshot_size` is the size of the "snapshot.baf" file, which is used to
  // decide when the journal has got too big and we should trigger a new
  // snapshot of the cache. This object is always constructed by
  // NoVarySearchCacheStorage::Loader.
  Journaller(std::unique_ptr<FileOperations> operations,
             base::WeakPtr<NoVarySearchCacheStorage> storage_ptr,
             scoped_refptr<base::SequencedTaskRunner> parent_sequence,
             size_t snapshot_size)
      : operations_(std::move(operations)),
        storage_ptr_(std::move(storage_ptr)),
        parent_sequence_(std::move(parent_sequence)),
        snapshot_size_(snapshot_size) {}

  ~Journaller() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // Creates a new "journal.baj" file and writes the magic number to it. Called
  // by NoVarySearchCacheStorage::Loader and the WriteSnapshot() method.
  CreateResult Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    CHECK(!failed_);
    auto maybe_writer = operations_->CreateWriter(kJournalFilename);
    if (!maybe_writer.has_value()) {
      base::UmaHistogramExactLinear("HttpCache.NoVarySearch.JournalCreateError",
                                    -maybe_writer.error(),
                                    -base::File::FILE_ERROR_MAX);
      return CreateResult::kCouldntCreateJournal;
    }
    writer_ = std::move(maybe_writer.value());
    size_ = 0u;
    if (!writer_->Write(base::U32ToBigEndian(kJournalMagicNumber))) {
      base::UmaHistogramBoolean("HttpCache.NoVarySearch.JournalStartError",
                                true);
      return CreateResult::kCouldntStartJournal;
    }
    size_ += sizeof(kJournalMagicNumber);
    return CreateResult::kSuccess;
  }

  // Appends an update to the journal. Called via PostTask by
  // NoVarySearchCacheStorage.
  void Append(const base::Pickle& pickle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Multiple calls to `Append()` may be pending in the task queue. After one
    // has failed the rest should be ignored.
    if (failed_) {
      return;
    }

    // The Pickle format has a length field, but because it supports arbitrary
    // header lengths it is not properly self-delimiting. So we need to prepend
    // our own length field to be able to read it back reliably. It's better to
    // do a single write to increase the chance that it will succeed or fail
    // atomically, so pre-assemble the output.
    std::vector<uint8_t> assembled;
    assembled.reserve(sizeof(uint32_t) + pickle.size());
    const auto size_as_bytes =
        base::U32ToLittleEndian(base::checked_cast<uint32_t>(pickle.size()));
    assembled.insert(assembled.end(), size_as_bytes.begin(),
                     size_as_bytes.end());
    assembled.insert(assembled.end(), pickle.begin(), pickle.end());

    if (!writer_->Write(assembled)) {
      base::UmaHistogramBoolean("HttpCache.NoVarySearch.JournalAppendError",
                                true);
      JournallingFailed();
      return;
    }
    size_ += assembled.size();
    if (size_ > std::max(snapshot_size_, kMinimumAutoSnapshotSize)) {
      MaybeRequestSnapshot();
    }
  }

  // Rewrites the "snapshot.baf" file from `pickle` and then creates a new empty
  // journal file. Called via PostTask by NoVarySearchCacheStorage.
  void WriteSnapshot(base::Pickle pickle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (failed_) {
      return;
    }

    requested_snapshot_ = false;
    snapshot_size_ = pickle.size();

    if (!PersistPickleAsSnapshot(operations_.get(), std::move(pickle))) {
      JournallingFailed();
      return;
    }

    if (!Start()) {
      JournallingFailed();
    }
  }

 private:
  // Notify NoVarySearchCacheStorage that journalling has failed and it should
  // destroy this object.
  void JournallingFailed() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    CHECK(!failed_);
    failed_ = true;
    // This object will be deleted shortly.
    parent_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&NoVarySearchCacheStorage::OnJournallingFailed,
                       storage_ptr_));
  }

  // Request NoVarySearchCacheStorage to serialize the NoVarySearchCache object
  // and call WriteSnapshot() with it.
  void MaybeRequestSnapshot() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    if (requested_snapshot_) {
      // TODO(https://crbug.com/399562754): Write a unit test for this line.
      return;
    }
    requested_snapshot_ = true;
    parent_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&NoVarySearchCacheStorage::TakeSnapshot, storage_ptr_));
  }

  // The FileOperations implementation to be used for file operations.
  const std::unique_ptr<FileOperations> operations_;

  // A pointer back to the object that owns this Journaller. This pointer must
  // not be dereferenced on this sequence; it is only for use in binding
  // closures to be posted to `parent_sequence_`.
  const base::WeakPtr<NoVarySearchCacheStorage> storage_ptr_;

  // The sequence that the NoVarySearchCacheStorage object lives on.
  const scoped_refptr<base::SequencedTaskRunner> parent_sequence_;

  // Handle to write to "journal.baj".
  std::unique_ptr<FileOperations::Writer> writer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The current size of "journal.baj".
  size_t size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The current size of "snapshot.baf".
  size_t snapshot_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // True if journalling has failed. There may still be tasks in flight.
  bool failed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // True if we've requested a snapshot since the last one. This stops us
  // sending multiple requests.
  bool requested_snapshot_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Loading the previous cache is a synchronous operation and could be defined
// in a single function, but for readability it is split into methods.
// Everything in this class happens on the background thread.
class NoVarySearchCacheStorage::Loader final {
 public:
  // This must match the argument type of
  // NoVarySearchCacheStorage::OnStartComplete().
  using ResultType = base::expected<CacheAndJournalPointers, LoadFailed>;

  // Creates a Loader object, loads the cache, and returns the result. On
  // success, returns the loaded NoVarySearchCache object and a Journaller
  // object which will post tasks to the NoVarySearchCacheStorage object pointed
  // to by `storage_ptr` using `parent_sequence`. `default_max_size` is the
  // max_size to use for a new NoVarySearchCache object if we are unable to load
  // an existing one. This is called via PostTaskAndReplyWithResult() from
  // NoVarySearchCacheStorage::Load(), which arranges for the result to be sent
  // to the OnLoadComplete() method of that object on the original thread.
  static ResultType CreateAndLoad(
      std::unique_ptr<FileOperations> operations,
      base::WeakPtr<NoVarySearchCacheStorage> storage_ptr,
      scoped_refptr<base::SequencedTaskRunner> parent_sequence,
      size_t default_max_size) {
    // As this whole process is synchronous, the Loader object can be allocated
    // on the stack.
    Loader loader(std::move(operations), std::move(storage_ptr),
                  std::move(parent_sequence), default_max_size);
    return loader.Load();
  }

  Loader(const Loader&) = delete;
  Loader& operator=(const Loader&) = delete;

 private:
  // Possible results of attempted load.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(Result)
  enum class Result {
    kSuccess = 0,
    kSnapshotFileTooSmall = 1,
    kSnapshotLoadFailed = 2,
    kBadSnapshotMagicNumber = 3,
    kInvalidSnapshotPickle = 4,
    kJournalLoadFailed = 5,
    kJournalTooOld = 6,
    kJournalTooSmall = 7,
    kBadJournalMagicNumber = 8,
    kCorruptJournal = 9,
    kCorruptJournalEntry = 10,
    kCouldntCreateCacheFile = 11,
    kCouldntCreateJournal = 12,
    kCouldntStartJournal = 13,
    kOperationsInitFailed = 14,
    kMaxValue = kOperationsInitFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NoVarySearchCacheStorageLoadResult)

  struct ReplayJournalResult {
    size_t replayed_journal_entries = 0u;
    bool had_error = false;
  };

  Loader(std::unique_ptr<FileOperations> operations,
         base::WeakPtr<NoVarySearchCacheStorage> storage_ptr,
         scoped_refptr<base::SequencedTaskRunner> parent_sequence,
         size_t default_max_size)
      : operations_(std::move(operations)),
        storage_ptr_(std::move(storage_ptr)),
        parent_sequence_(std::move(parent_sequence)),
        default_max_size_(default_max_size) {}

  ~Loader() = default;

  // Starts loading persisted cache files and preparing to journal. The return
  // value from this object will be posted back to the main thread. Calls into
  // other methods of this object to handle various exceptional conditions.
  [[nodiscard]] ResultType Load() {
    if (!operations_->Init()) {
      return GiveUp(Result::kOperationsInitFailed);
    }

    auto maybe_load_result = operations_->Load(kSnapshotFilename, kMaxFileSize);
    if (!maybe_load_result.has_value()) {
      base::UmaHistogramExactLinear("HttpCache.NoVarySearch.SnapshotLoadError",
                                    -maybe_load_result.error(),
                                    -base::File::FILE_ERROR_MAX);
      return StartFromScratch(Result::kSnapshotLoadFailed);
    }
    auto [snapshot_contents, snapshot_last_modified] =
        std::move(maybe_load_result.value());
    static constexpr size_t kMagicNumberSize = sizeof(kSnapshotMagicNumber);
    snapshot_size_ = snapshot_contents.size();
    if (snapshot_size_ < kMagicNumberSize) {
      return StartFromScratch(Result::kSnapshotFileTooSmall);
    }
    auto [snapshot_magic_number, snapshot_pickle] =
        base::span(snapshot_contents).split_at<kMagicNumberSize>();
    if (base::U32FromBigEndian(snapshot_magic_number) != kSnapshotMagicNumber) {
      return StartFromScratch(Result::kBadSnapshotMagicNumber);
    }

    auto pickle = base::Pickle::WithUnownedBuffer(snapshot_pickle);
    auto maybe_cache = ReadValueFromPickle<NoVarySearchCache>(pickle);
    if (!maybe_cache) {
      return StartFromScratch(Result::kInvalidSnapshotPickle);
    }
    cache_ =
        std::make_unique<NoVarySearchCache>(std::move(maybe_cache.value()));

    auto journal_load_result =
        operations_->Load(kJournalFilename, kMaxFileSize);
    if (!journal_load_result.has_value()) {
      base::UmaHistogramExactLinear("HttpCache.NoVarySearch.JournalLoadError",
                                    -journal_load_result.error(),
                                    -base::File::FILE_ERROR_MAX);
      return StartJournal(Result::kJournalLoadFailed);
    }
    auto [journal_contents, journal_last_modified] =
        std::move(journal_load_result.value());
    if (journal_last_modified < snapshot_last_modified) {
      // This can happen if the previous run was interrupted during rewriting
      // the cache.
      return StartJournal(Result::kJournalTooOld);
    }
    if (journal_contents.size() < kMagicNumberSize) {
      return StartJournal(Result::kJournalTooSmall);
    }
    auto [journal_magic_number, journal_pickles] =
        base::span(journal_contents).split_at<kMagicNumberSize>();
    if (base::U32FromBigEndian(journal_magic_number) != kJournalMagicNumber) {
      return StartJournal(Result::kBadJournalMagicNumber);
    }
    if (journal_pickles.empty()) {
      return StartJournal(Result::kSuccess);
    }
    auto [replayed_journal_entries, had_error] = ReplayJournal(journal_pickles);
    if (replayed_journal_entries == 0u) {
      return StartJournal(had_error ? Result::kCorruptJournal
                                    : Result::kSuccess);
    }
    // Now that we've replayed the journal into `cache`, write a new snapshot
    // of it to disk along with a fresh empty journal.
    return WriteCache(had_error ? Result::kCorruptJournalEntry
                                : Result::kSuccess);
  }

  // Creates an empty cache and journal file. Used when we couldn't load a
  // persisted cache.
  [[nodiscard]] ResultType StartFromScratch(Result result) {
    cache_ = std::make_unique<NoVarySearchCache>(default_max_size_);
    return WriteCache(result);
  }

  // Deserializes base::Pickle objects representing cache mutations from
  // `pickles` and applies them to `cache`. Each base::Pickle is preceded by a
  // 32-bit length field. Continues until it encounters the end of `pickles` or
  // an error.
  ReplayJournalResult ReplayJournal(base::span<const uint8_t> pickles) {
    size_t replayed_journal_entries = 0u;
    bool had_error = false;

    while (!pickles.empty() && !had_error) {
      if (pickles.size() < sizeof(uint32_t)) {
        had_error = true;
        break;
      }
      const auto size_as_bytes = pickles.take_first<sizeof(uint32_t)>();
      const uint32_t size = base::U32FromLittleEndian(size_as_bytes);
      if (pickles.size() < size) {
        had_error = true;
        break;
      }
      const auto pickle_span = pickles.take_first(size);
      const auto pickle = base::Pickle::WithUnownedBuffer(pickle_span);
      if (pickle.size() == 0) {
        // The Pickle header was invalid.
        had_error = true;
        break;
      }
      base::PickleIterator iter(pickle);
      auto maybe_type = ReadValueFromPickle<JournalEntryType>(iter);
      if (!maybe_type) {
        had_error = true;
        break;
      }
      switch (maybe_type.value()) {
        case JournalEntryType::kInsert: {
          std::string partition_key;
          std::string base_url;
          auto nvs_data = CreateHttpNoVarySearchData();
          std::optional<std::string> query;
          base::Time update_time;
          if (!ReadPickleInto(iter, partition_key, base_url, nvs_data, query,
                              update_time) ||
              !iter.ReachedEnd()) {
            had_error = true;
            break;
          }
          cache_->ReplayInsert(std::move(partition_key), std::move(base_url),
                               std::move(nvs_data), std::move(query),
                               update_time);
          ++replayed_journal_entries;
          break;
        }

        case JournalEntryType::kErase: {
          std::string partition_key;
          std::string base_url;
          auto nvs_data = CreateHttpNoVarySearchData();
          std::optional<std::string> query;
          if (!ReadPickleInto(iter, partition_key, base_url, nvs_data, query) ||
              !iter.ReachedEnd()) {
            had_error = true;
            break;
          }
          cache_->ReplayErase(partition_key, base_url, nvs_data, query);
          ++replayed_journal_entries;
          break;
        }
      }
    }
    base::UmaHistogramCounts100000(
        "HttpCache.NoVarySearch.ReplayedJournalEntries",
        replayed_journal_entries);

    return {replayed_journal_entries, had_error};
  }

  // Creates or replaces the snapshot.baf file with a new snapshot created by
  // serializing `cache`.
  [[nodiscard]] ResultType WriteCache(Result result) {
    snapshot_size_ = PersistCache(operations_.get(), *cache_);
    if (snapshot_size_ == 0u) {
      return GiveUp(Result::kCouldntCreateCacheFile);
    }
    return StartJournal(result);
  }

  // Stops attempting to restore the persisted data or create a new journal.
  // Logs a histogram with `result` and then returns a LoadFailed value which
  // will be posted back to the main thread.
  [[nodiscard]] ResultType GiveUp(Result result) {
    LogResult(result);
    return base::unexpected(LoadFailed::kCannotJournal);
  }

  // Starts a new journal.baj file. On success, `cache_` and `result` will be
  // passed back to the main thread. On failure, gives up.
  [[nodiscard]] ResultType StartJournal(Result result) {
    CHECK_GT(snapshot_size_, 0u);
    auto journal = std::make_unique<Journaller>(
        std::move(operations_), std::move(storage_ptr_),
        std::move(parent_sequence_), snapshot_size_);
    auto create_result = journal->Start();
    if (create_result != Journaller::CreateResult::kSuccess) {
      return GiveUp(create_result ==
                            Journaller::CreateResult::kCouldntCreateJournal
                        ? Result::kCouldntCreateJournal
                        : Result::kCouldntStartJournal);
    }
    LogResult(result);
    CHECK(cache_);
    return CacheAndJournalPointers(
        std::move(cache_),
        JournallerPtr(journal.release(),
                      base::OnTaskRunnerDeleter(
                          base::SequencedTaskRunner::GetCurrentDefault())));
  }

  // Logs a histogram with the final result of loading.
  void LogResult(Result result) {
    base::UmaHistogramEnumeration("HttpCache.NoVarySearch.LoadResult", result);
  }

  // The FileOperations implementation used by this object and passed to the
  // Journaller object when it is created.
  std::unique_ptr<FileOperations> operations_;

  // The NoVarySearchCacheStorage object is not directly used by this object,
  // but the pointer is retained to be transferred to the Journaller object when
  // it is created. This pointer must not be dereferenced on this thread.
  base::WeakPtr<NoVarySearchCacheStorage> storage_ptr_;

  // This object doesn't use `parent_sequence_` except to pass it to the
  // Journaller object.
  scoped_refptr<base::SequencedTaskRunner> parent_sequence_;

  // The size of the created "snapshot.baf" file.
  size_t snapshot_size_ = 0;

  // The default `max_size` parameter used if we create a NoVarySearchCache
  // object from scratch.
  size_t default_max_size_;

  // This will be passed back to the main thread on success.
  std::unique_ptr<NoVarySearchCache> cache_;
};

NoVarySearchCacheStorage::CacheAndJournalPointers::CacheAndJournalPointers(
    std::unique_ptr<NoVarySearchCache> cache,
    JournallerPtr journal)
    : cache(std::move(cache)), journal(std::move(journal)) {}

NoVarySearchCacheStorage::CacheAndJournalPointers::CacheAndJournalPointers(
    CacheAndJournalPointers&&) = default;
NoVarySearchCacheStorage::CacheAndJournalPointers::~CacheAndJournalPointers() =
    default;

NoVarySearchCacheStorage::NoVarySearchCacheStorage()
    : journal_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

NoVarySearchCacheStorage::~NoVarySearchCacheStorage() {
  if (cache_) {
    cache_->SetJournal(nullptr);
  }
}

void NoVarySearchCacheStorage::Load(
    std::unique_ptr<NoVarySearchCacheStorageFileOperations> file_operations,
    size_t default_max_size,
    LoadCallback callback) {
  CHECK(!cache_);
  CHECK(!background_task_runner_);
  CHECK(!journal_);
  CHECK(start_time_.is_null());

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  start_time_ = base::Time::Now();
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Loader::CreateAndLoad, std::move(file_operations),
                     weak_factory_.GetWeakPtr(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     default_max_size),
      base::BindOnce(&NoVarySearchCacheStorage::OnLoadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NoVarySearchCacheStorage::TakeSnapshot() {
  if (!cache_ || !journal_) {
    return;
  }
  CHECK(background_task_runner_);
  base::Pickle pickle;
  WriteToPickle(pickle, *cache_);
  // This use of `base::Unretained` is safe because `journal_` is owned by this
  // object and always deleted on `background_task_runner_`.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Journaller::WriteSnapshot,
                     base::Unretained(journal_.get()), std::move(pickle)));
}

void NoVarySearchCacheStorage::OnInsert(const std::string& partition_key,
                                        const std::string& base_url,
                                        const HttpNoVarySearchData& nvs_data,
                                        const std::optional<std::string>& query,
                                        base::Time update_time) {
  base::Pickle pickle;
  WriteToPickle(pickle, JournalEntryType::kInsert, partition_key, base_url,
                nvs_data, query, update_time);
  AppendToJournal(std::move(pickle));
}

void NoVarySearchCacheStorage::OnErase(
    const std::string& partition_key,
    const std::string& base_url,
    const HttpNoVarySearchData& nvs_data,
    const std::optional<std::string>& query) {
  base::Pickle pickle;
  WriteToPickle(pickle, JournalEntryType::kErase, partition_key, base_url,
                nvs_data, query);
  AppendToJournal(std::move(pickle));
}

void NoVarySearchCacheStorage::AppendToJournal(base::Pickle pickle) {
  CHECK(journal_);
  CHECK(background_task_runner_);
  // This use of `base::Unretained` is safe because `journal_` is owned by this
  // object and always deleted on `background_task_runner_`.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Journaller::Append, base::Unretained(journal_.get()),
                     std::move(pickle)));
}

void NoVarySearchCacheStorage::OnLoadComplete(
    LoadCallback callback,
    base::expected<CacheAndJournalPointers, LoadFailed> result) {
  const base::TimeDelta elapsed = base::Time::Now() - start_time_;
  if (!result.has_value()) {
    base::UmaHistogramTimes("HttpCache.NoVarySearch.LoadTime.Failure", elapsed);
    background_task_runner_ = nullptr;
    // Continue without persistence.
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }
  base::UmaHistogramTimes("HttpCache.NoVarySearch.LoadTime.Success", elapsed);
  auto [cache, journal] = std::move(result.value());
  CHECK(journal);
  CHECK(!journal_);
  journal_ = std::move(journal);
  CHECK(cache);
  base::UmaHistogramCounts10000("HttpCache.NoVarySearch.EntriesLoaded",
                                cache->size());
  CHECK(!cache_);
  cache_ = cache.get();
  cache_->SetJournal(this);
  std::move(callback).Run(std::move(cache));
}

void NoVarySearchCacheStorage::OnJournallingFailed() {
  cache_->SetJournal(nullptr);
  journal_ = nullptr;
  cache_ = nullptr;
  background_task_runner_ = nullptr;
}

}  // namespace net
