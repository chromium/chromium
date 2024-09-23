// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/slop_bucket.h"

#include <limits.h>
#include <string.h>

#include <algorithm>
#include <ostream>

#include "base/containers/heap_array.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/system/sys_info.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace network {

BASE_FEATURE(kSlopBucket, "SlopBucket", base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// The default for "require_priority". SlopBucket will not be used for requests
// with priority lower than this.
constexpr net::RequestPriority kDefaultRequirePriority = net::MEDIUM;

// The default for "chunk_size". All chunks that SlopBucket allocates will be of
// this size. Bigger values are more efficient, but waste more memory.
constexpr int kDefaultChunkSize = 512 * 1024;

// The default for "min_buffer_size". A read smaller than this size will not be
// attempted. Instead, a new chunk will be allocated, or, if that is not
// possible, the bucket will be considered full. Must be less than
// `kDefaultChunkSize`.
constexpr int kDefaultMinBufferSize = 32 * 1024;

// The default for "max_chunks_per_request". A single URLRequest will allocate
// this many chunks of buffering at most. The default is equivlant to 4MB. Must
// be greater than 0.
constexpr int kDefaultMaxChunksPerRequest = 8;

// The default for "max_chunks_total". This will cap the memory used by all
// SlopBuckets in the process. The default is equivalent to 16MB. Must be
// greater than `kDefaultMaxChunksPerRequest`.
constexpr int kDefaultMaxChunksTotal = 32;

// The default for "memory_pressure_disable_level". When a memory pressure
// notification of this level or higher arrives, SlopBucket will disable itself.
constexpr base::MemoryPressureListener::MemoryPressureLevel
    kDefaultMemoryPressureDisableLevel =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

// Mapping of name and value for net::RequestPriority enum.
constexpr base::FeatureParam<net::RequestPriority>::Option
    kRequestPriorityOptions[] = {
        {net::THROTTLED, "THROTTLED"}, {net::IDLE, "IDLE"},
        {net::LOWEST, "LOWEST"},       {net::LOW, "LOW"},
        {net::MEDIUM, "MEDIUM"},       {net::HIGHEST, "HIGHEST"},
};

// "require_priority" parameter.
constexpr base::FeatureParam<net::RequestPriority> kRequirePriorityParam(
    &kSlopBucket,
    "require_priority",
    net::MEDIUM,
    &kRequestPriorityOptions);

// "chunk_size" parameter.
constexpr base::FeatureParam<int> kChunkSizeParam(&kSlopBucket,
                                                  "chunk_size",
                                                  kDefaultChunkSize);

// "min_buffer_size" parameter.
constexpr base::FeatureParam<int> kMinBufferSizeParam(&kSlopBucket,
                                                      "min_buffer_size",
                                                      kDefaultMinBufferSize);

// "max_chunks_per_request" parameter.
constexpr base::FeatureParam<int> kMaxChunksPerRequestParam(
    &kSlopBucket,
    "max_chunks_per_request",
    kDefaultMaxChunksPerRequest);

// "max_chunks_total" parameter.
constexpr base::FeatureParam<int> kMaxChunksTotalParam(&kSlopBucket,
                                                       "max_chunks_total",
                                                       kDefaultMaxChunksTotal);

// Mapping of name and value for
// base::MemoryPressureListener::MemoryPressureLevel enum.
constexpr base::FeatureParam<
    base::MemoryPressureListener::MemoryPressureLevel>::Option
    kMemoryPressureLevelOptions[] = {
        {base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, "NONE"},
        {base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
         "MODERATE"},
        {base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
         "CRITICAL"},
};

// "memory_pressure_disable_level" parameter.
constexpr base::FeatureParam<base::MemoryPressureListener::MemoryPressureLevel>
    kMemoryPressureDisableLevelParam(
        &kSlopBucket,
        "memory_pressure_disable_level",
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
        &kMemoryPressureLevelOptions);

}  // namespace

// This class encapsulates the runtime configuration of SlopBucket. When
// constructed, it reads and validates the configuration. SlopBucket can be
// configured with an command-like flag like
// `--enable-features=SlopBucket/max_chunks_per_request/2`.
class SlopBucket::Configuration {
 public:
  Configuration() { InitializeFromFeatures(); }

  bool enabled_by_configuration() const { return enabled_by_configuration_; }
  net::RequestPriority require_priority() const { return require_priority_; }
  size_t chunk_size() const { return chunk_size_; }
  size_t min_buffer_size() const { return min_buffer_size_; }
  size_t max_chunks_per_request() const { return max_chunks_per_request_; }
  size_t max_chunks_total() const { return max_chunks_total_; }
  base::MemoryPressureListener::MemoryPressureLevel
  memory_pressure_disable_level() const {
    return memory_pressure_disable_level_;
  }

 private:
  friend std::ostream& operator<<(std::ostream& os,
                                  const SlopBucket::Configuration& conf) {
    return os << "require_priority = "
              << kRequirePriorityParam.GetName(conf.require_priority())
              << "\nchunk_size = " << conf.chunk_size()
              << "\nmin_buffer_size = " << conf.min_buffer_size()
              << "\nmax_chunks_per_request = " << conf.max_chunks_per_request()
              << "\nmax_chunks_total = " << conf.max_chunks_total()
              << "\nmemory_pressure_disable_level = "
              << kMemoryPressureDisableLevelParam.GetName(
                     conf.memory_pressure_disable_level());
  }

  // Validate and set the configurable parameters from features. If the
  // configuration is invalid, disable the feature.
  void InitializeFromFeatures() {
    enabled_by_configuration_ = base::FeatureList::IsEnabled(kSlopBucket);
    if (!enabled_by_configuration_) {
      return;
    }
    require_priority_ = kRequirePriorityParam.Get();
    chunk_size_ = kChunkSizeParam.Get();
    if (chunk_size_ < 256) {
      DisableWithWarning("chunk_size is too small");
      return;
    }
    if (chunk_size_ >= INT_MAX) {
      DisableWithWarning("chunk_size is too big");
      return;
    }
    min_buffer_size_ = kMinBufferSizeParam.Get();
    if (min_buffer_size_ < 0) {
      DisableWithWarning("min_buffer_size is negative");
      return;
    }
    if (min_buffer_size_ >= chunk_size_) {
      DisableWithWarning("min_buffer_size is not less than chunk_size");
      return;
    }
    max_chunks_per_request_ = kMaxChunksPerRequestParam.Get();
    if (max_chunks_per_request_ < 1) {
      DisableWithWarning("max_chunks_per_request is less than 1");
      return;
    }
    max_chunks_total_ = kMaxChunksTotalParam.Get();
    if (max_chunks_total_ < max_chunks_per_request_) {
      DisableWithWarning(
          "max_chunks_total is less than max_chunks_per_request");
    }
    memory_pressure_disable_level_ = kMemoryPressureDisableLevelParam.Get();
  }

  void DisableWithWarning(const char* warning) {
    enabled_by_configuration_ = false;
    LOG(WARNING) << warning << ". SlopBucket disabled.";
  }

  // Configurable parameters. These only change during construction.
  bool enabled_by_configuration_ = false;

  net::RequestPriority require_priority_ = kDefaultRequirePriority;

  size_t chunk_size_ = size_t{kDefaultChunkSize};

  size_t min_buffer_size_ = size_t{kDefaultMinBufferSize};

  size_t max_chunks_per_request_ = size_t{kDefaultMaxChunksPerRequest};

  size_t max_chunks_total_ = size_t{kDefaultMaxChunksTotal};

  base::MemoryPressureListener::MemoryPressureLevel
      memory_pressure_disable_level_ = kDefaultMemoryPressureDisableLevel;
};

// SlopBucketManager tracks global state for SlopBuckets. It is thread-hostile.
// TODO(ricea): Make this be owned by the NetworkService so that it can function
// correctly if there are multiple NetworkServices on different threads.
class SlopBucket::Manager {
 public:
  // Prevent accidental use of the constructor.
  using PassKey = base::PassKey<Manager>;

  // Returns the global Manager object. May be called on any thread, but the
  // first call in the process must be on the IO thread.
  static Manager& Get() {
    static base::NoDestructor<Manager> manager{PassKey()};
    return *manager.get();
  }

  explicit Manager(PassKey) {
    if (!configuration_.enabled_by_configuration()) {
      disabled_.Set();
      DVLOG(1) << "SlopBucket disabled by configuration.";
      return;
    }

    // Don't try to use SlopBucket on devices with low RAM.
    if (base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()) {
      Disable(DisabledReason::kLowEnd);
      DVLOG(1) << "SlopBucket disabled due to low RAM.";
      return;
    }

    DVLOG(1) << "SlopBucket enabled. Configuration:\n" << configuration_;

    // This use of base::Unretained() is safe because the callback won't be
    // called after `memory_pressure_listener_` is destroyed.
    memory_pressure_listener_.emplace(
        FROM_HERE, base::BindRepeating(&Manager::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  // True if SlopBucket is disabled, either by configuration or lack of memory.
  bool disabled() const {
    // Checking enabed_by_default() first avoids a memory barrier on ARM in the
    // common case where SlopBucket is disabled by configuration. This is safe
    // because enabled_by_default() never changes after construction.
    return !configuration_.enabled_by_configuration() || disabled_.IsSet();
  }

  net::RequestPriority require_priority() const {
    return configuration_.require_priority();
  }

  size_t chunk_size() const { return configuration_.chunk_size(); }
  size_t min_buffer_size() const { return configuration_.min_buffer_size(); }

  scoped_refptr<ChunkIOBuffer> RequestChunk(size_t existing_chunks) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (disabled()) {
      DVLOG(1) << "Not allocating chunk because disabled";
      return nullptr;
    }

    if (existing_chunks == configuration_.max_chunks_per_request()) {
      DVLOG(1)
          << "Not allocating chunk because the request max has been reached";
      return nullptr;
    }

    base::HeapArray<char> chunk = GetChunk();
    if (!chunk.data()) {
      return nullptr;
    }

    DVLOG(1) << "Acquired chunk";

    return base::MakeRefCounted<ChunkIOBuffer>(std::move(chunk));
  }

  // Returns a chunk to the pool. This method may be called on background
  // threads. Despite the overhead of locking, it is still 10 times faster to
  // reuse the chunks than to delete and new them again.
  void ReleaseChunk(base::HeapArray<char> buffer) {
    if (disabled()) {
      return;  // `buffer` is freed.
    }
    base::AutoLock auto_lock(lock_);
    CHECK_GT(total_chunks_, 0u);
    --total_chunks_;
    free_pool_.push_back(std::move(buffer));
    CHECK_LE(total_chunks_ + free_pool_.size(),
             configuration_.max_chunks_total());
  }

  // Records histogram NetworkService.SlopBucket.DisabledReason if 24 hours has
  // passed since it was last recorded.
  void MaybeRecordDisabledReason() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!configuration_.enabled_by_configuration()) {
      return;
    }
    if (disabled_reason_last_recorded_.is_null() ||
        disabled_reason_last_recorded_ <
            base::TimeTicks::Now() - base::Hours(24)) {
      RecordDisabledReason();
    }
  }

 private:
  // The reason why SlopBucket is disabled, for metrics. We don't record when it
  // is disabled by configuration, as that case is common and uninteresting.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DisabledReason {
    kEnabled = 0,         // Not actually disabled.
    kLowEnd = 1,          // Disabled because device is low-end.
    kMemoryPressure = 2,  // Disabled at runtime due to memory pressure.
    kMaxValue = kMemoryPressure,
  };

  // Sets `disabled_` and `disabled_reason_`.
  void Disable(DisabledReason reason)
      VALID_CONTEXT_REQUIRED(sequence_checker_) {
    disabled_.Set();
    disabled_reason_ = reason;
  }

  // Records the histogram NetworkService.SlopBucket.DisabledReason and sets
  // `disabled_reason_last_recorded_`.
  void RecordDisabledReason() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    base::UmaHistogramEnumeration("NetworkService.SlopBucket.DisabledReason",
                                  disabled_reason_);
    disabled_reason_last_recorded_ = base::TimeTicks::Now();
  }

  // Encapsulates the locked section of RequestChunk().
  base::HeapArray<char> GetChunk() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    base::HeapArray<char> chunk;
    base::ReleasableAutoLock auto_lock(&lock_);
    if (total_chunks_ == configuration_.max_chunks_total()) {
      // Don't hold the lock while logging.
      auto_lock.Release();
      DVLOG(1)
          << "Not allocating chunk because the global max has been reached";
      return base::HeapArray<char>();
    }

    if (!free_pool_.empty()) {
      // We take the most recently freed chunk, as it is most likely to be in
      // cache.
      chunk = std::move(free_pool_.back());
      free_pool_.pop_back();
    } else {
      chunk = base::HeapArray<char>::Uninit(configuration_.chunk_size());
    }

    ++total_chunks_;
    CHECK_LE(total_chunks_ + free_pool_.size(),
             configuration_.max_chunks_total());
    return chunk;
  }

  // Responds to a memory pressure notification by emptying the free pool and
  // possibly disabling SlopBucket.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (disabled()) {
      return;
    }
    if (level >= configuration_.memory_pressure_disable_level()) {
      Disable(DisabledReason::kMemoryPressure);
      RecordDisabledReason();
      DVLOG(1) << "Memory pressure detected. Disabling SlopBucket.";
      memory_pressure_listener_ = std::nullopt;
    }
    // We swap the vector so we don't have to hold the lock while we free the
    // elements.
    std::vector<base::HeapArray<char>> old_free_pool;
    {
      base::AutoLock auto_lock(lock_);
      old_free_pool.swap(free_pool_);
    }
    // Elements are freed on return.
  }

  // SlopBucket configuration.
  const Configuration configuration_;

  // Is SlopBucket disabled? This can change to true during runtime. May be read
  // on any thread, but only set on the IO thread.
  base::AtomicFlag disabled_;

  // Lock for our runtime variables. They may be accessed on background threads.
  base::Lock lock_;

  // The total number of ChunkIOBuffers currently allocated, including any that
  // have been released by SlopBucket but are still referenced somewhere else.
  // This does not include buffers stored in `free_pool_`, but `total_chunks_` +
  // `free_pool_.size()` is <= max_chunks_total.
  size_t total_chunks_ GUARDED_BY(lock_) = 0;

  // Pool of free buffers.
  std::vector<base::HeapArray<char>> free_pool_ GUARDED_BY(lock_);

  // Reason why SlopBucket is disabled. Only accessed on the IO thread.
  DisabledReason disabled_reason_;

  // `disabled_reason_` is recorded the first time SlopBucket is attempted to be
  // used, and at most once every 24 hours after that. It is also recorded when
  // the state changes from enabled to disabled, otherwise we would miss
  // capturing that on Android, where the browser is frequently killed by the
  // OS. Only accessed on the IO thread.
  base::TimeTicks disabled_reason_last_recorded_;

  // Calls OnMemoryPressure() on the IO thread when there is memory pressure.
  // Only set when SlopBucket is enabled. Only accessed on the IO thread.
  std::optional<base::MemoryPressureListener> memory_pressure_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class SlopBucket::ChunkIOBuffer final : public net::IOBuffer {
 public:
  explicit ChunkIOBuffer(base::HeapArray<char> storage)
      : IOBuffer(storage), base_(std::move(storage)) {}

  // Notes that `bytes` bytes have been read from a URLRequest into this chunk.
  void MarkFilled(size_t bytes) {
    const size_t chunk_size = Manager::Get().chunk_size();
    CHECK_LE(bytes, chunk_size);
    // This addition can't overflow because both `bytes` and `filled_up_to_` are
    // always less than chunk_size, which is much less than half the max value
    // that can be stored in a size_t.
    filled_up_to_ += bytes;
    CHECK_LE(filled_up_to_, chunk_size);
    data_ += bytes;
    size_ -= bytes;
  }

  // Notes that `bytes` bytes have been written to the mojo data pipe and should
  // not be returned again.
  void MarkConsumed(size_t bytes) {
    CHECK_LE(bytes, Manager::Get().chunk_size());
    // This addition can't overflow because both `bytes` and `consumed_up_to_`
    // are always less than chunk_size, which is much less than half the max
    // value that can be stored in a size_t.
    CHECK_LE(consumed_up_to_ + bytes, filled_up_to_);
    consumed_up_to_ += bytes;
  }

  // The number of bytes that have been read into this chunk so far.
  size_t filled_up_to() const { return filled_up_to_; }

  // The number of bytes that have been read out of this chunk so far.
  size_t consumed_up_to() const { return consumed_up_to_; }

  // The number of bytes available to be read out of this chunk.
  size_t UnconsumedBytes() const { return filled_up_to_ - consumed_up_to_; }

  // True if there are no bytes available for reading out of this chunk.
  bool Empty() const { return consumed_up_to_ == filled_up_to_; }

  // The next byte of this chunk available for copying to the mojo data pipe.
  // Note that the data() method from the parent class provides the next byte
  // available for writing to by the URLRequest.
  const char* NextByteToConsume() const {
    return base_.subspan(consumed_up_to_).data();
  }

 private:
  ~ChunkIOBuffer() override {
    // Prevent the base class from trying to free `data_`.
    data_ = nullptr;
    // Return the chunk to the pool.
    Manager::Get().ReleaseChunk(std::move(base_));
  }

  base::HeapArray<char> base_;
  size_t filled_up_to_ = 0;
  size_t consumed_up_to_ = 0;
};

// static
std::unique_ptr<SlopBucket> SlopBucket::RequestSlopBucket(
    net::URLRequest* for_request) {
  Manager& manager = Manager::Get();
  manager.MaybeRecordDisabledReason();
  if (manager.disabled()) {
    return nullptr;
  }
  net::RequestPriority priority = for_request->priority();
  base::UmaHistogramEnumeration(
      "NetworkService.SlopBucket.RequestedPriority", priority,
      static_cast<net::RequestPriority>(net::MAXIMUM_PRIORITY + 1));
  if (priority < manager.require_priority()) {
    DVLOG(1) << "Refused to create SlopBucket for request to '"
             << for_request->url() << "' with priority "
             << net::RequestPriorityToString(priority);
    return nullptr;
  }

  return std::make_unique<SlopBucket>(PassKey(), for_request);
}

SlopBucket::SlopBucket(PassKey, net::URLRequest* request) : request_(request) {
  DVLOG(1) << "Created SlopBucket for request to '" << request->url() << "'";
}

SlopBucket::~SlopBucket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramCounts100("NetworkService.SlopBucket.PeakChunksAllocated",
                              peak_chunks_allocated_);
}

std::optional<int> SlopBucket::AttemptRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!read_in_progress_);
  CHECK(!completion_code_.has_value());
  const Manager& manager = Manager::Get();
  const size_t chunk_size = manager.chunk_size();
  const size_t min_buffer_size = manager.min_buffer_size();

  if (chunks_.empty() ||
      chunk_size - chunks_.back()->filled_up_to() < min_buffer_size) {
    if (!TryToAllocateChunk()) {
      return std::nullopt;
    }
  }
  ChunkIOBuffer& destination = *chunks_.back();

  read_in_progress_ = true;
  const size_t read_size = chunk_size - destination.filled_up_to();
  CHECK_GE(read_size, min_buffer_size);
  CHECK_LE(read_size, chunk_size);
  DVLOG(1) << "Starting Read(" << read_size << ")";
  // This cast is safe because `read_size` is always less than chunk_size, and
  // chunk_size is less than INT_MAX.
  const int rv = request_->Read(&destination, static_cast<int>(read_size));

  if (rv != net::ERR_IO_PENDING) {
    OnReadCompleted(rv);
  }

  return rv;
}

void SlopBucket::OnReadCompleted(int bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "OnReadCompleted(" << bytes_read << ")";
  read_in_progress_ = false;
  if (bytes_read <= 0) {
    completion_code_ = bytes_read;
    return;
  }

  CHECK(!chunks_.empty());
  chunks_.back()->MarkFilled(bytes_read);
}

size_t SlopBucket::Consume(void* buffer, size_t max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (chunks_.empty()) {
    return 0;
  }

  return ConsumeSlowPath(buffer, max);
}

size_t SlopBucket::ConsumeSlowPath(void* buffer, size_t max) {
  DVLOG(1) << "ConsumeSlowPath(" << max << ")";
  size_t consumed = 0;
  while (consumed < max && !chunks_.empty()) {
    ChunkIOBuffer& source = *chunks_.front();
    if (!source.Empty()) {
      const size_t bytes_available = source.UnconsumedBytes();
      const size_t bytes_to_consume = std::min(bytes_available, max - consumed);
      memcpy(static_cast<char*>(buffer) + consumed, source.NextByteToConsume(),
             bytes_to_consume);
      source.MarkConsumed(bytes_to_consume);
      consumed += bytes_to_consume;
    }
    if (source.Empty()) {
      if (chunks_.size() > 1 || !read_in_progress_) {
        DVLOG(1) << "Releasing chunk";
        chunks_.pop();
      } else {
        DVLOG(1)
            << "Cannot release the last chunk while we are reading into it.";
        break;
      }
    }
  }

  DVLOG(1) << "Consumed " << consumed << " bytes";
  CHECK_LE(consumed, max);
  return consumed;
}

bool SlopBucket::TryToAllocateChunk() {
  scoped_refptr<ChunkIOBuffer> new_chunk =
      Manager::Get().RequestChunk(chunks_.size());
  if (!new_chunk) {
    return false;
  }
  chunks_.push(std::move(new_chunk));
  if (chunks_.size() > peak_chunks_allocated_) {
    peak_chunks_allocated_ = chunks_.size();
  }
  return true;
}

}  // namespace network
