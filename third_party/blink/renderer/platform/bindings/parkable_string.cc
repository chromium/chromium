// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include <array>
#include <string_view>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/typed_macros.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/disk_data_allocator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
// "GN check" doesn't know that this file is only included when
// BUILDFLAG(HAS_ZSTD_COMPRESSION) is true. Disable it here.
#include "third_party/zstd/src/lib/zstd.h"  // nogncheck
#endif

namespace blink {

namespace {

ParkableStringImpl::Age MakeOlder(ParkableStringImpl::Age age) {
  switch (age) {
    case ParkableStringImpl::Age::kYoung:
      return ParkableStringImpl::Age::kOld;
    case ParkableStringImpl::Age::kOld:
    case ParkableStringImpl::Age::kVeryOld:
      return ParkableStringImpl::Age::kVeryOld;
  }
}

enum class ParkingAction { kParked, kUnparked, kWritten, kRead };

void RecordLatencyHistogram(const char* histogram_name,
                            base::TimeDelta duration) {
  // Size is at least 10kB, and at most ~10MB, and throughput ranges from
  // single-digit MB/s to ~1000MB/s depending on the CPU/disk, hence the ranges.
  base::UmaHistogramCustomMicrosecondsTimes(
      histogram_name, duration, base::Microseconds(500), base::Seconds(1), 100);
}

void RecordThroughputHistogram(const char* histogram_name,
                               int throughput_mb_s) {
  base::UmaHistogramCounts1000(histogram_name, throughput_mb_s);
}

void RecordStatistics(size_t size,
                      base::TimeDelta duration,
                      ParkingAction action) {
  int throughput_mb_s =
      base::ClampRound(size / duration.InSecondsF() / 1000000);
  int size_kb = static_cast<int>(size / 1000);

  switch (action) {
    case ParkingAction::kParked:
      // Size should be <1MiB in most cases.
      base::UmaHistogramCounts1000("Memory.ParkableString.Compression.SizeKb",
                                   size_kb);
      RecordLatencyHistogram("Memory.ParkableString.Compression.Latency",
                             duration);
      break;
    case ParkingAction::kUnparked:
      RecordLatencyHistogram("Memory.ParkableString.Decompression.Latency",
                             duration);
      RecordThroughputHistogram(
          "Memory.ParkableString.Decompression.ThroughputMBps",
          throughput_mb_s);
      break;
    case ParkingAction::kRead:
      RecordLatencyHistogram("Memory.ParkableString.Read.Latency", duration);
      break;
    case ParkingAction::kWritten:
      // No metric recorded.
      break;
  }
}

void AsanPoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  if (string.IsNull())
    return;
  // Since |string| is not deallocated, it remains in the AtomicStringTable,
  // where its content can be accessed for equality comparison for instance,
  // triggering a poisoned memory access. See crbug.com/883344 for an example.
  if (string.Impl()->IsAtomic())
    return;

  ASAN_POISON_MEMORY_REGION(string.Bytes(), string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

void AsanUnpoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  if (string.IsNull())
    return;

  ASAN_UNPOISON_MEMORY_REGION(string.Bytes(), string.CharactersSizeInBytes());
#endif  // defined(ADDRESS_SANITIZER)
}

// Char buffer allocated using PartitionAlloc, may be nullptr.
class NullableCharBuffer final {
  STACK_ALLOCATED();

 public:
  explicit NullableCharBuffer(size_t size) {
    data_ = reinterpret_cast<char*>(
        WTF::Partitions::BufferPartition()
            ->AllocInline<partition_alloc::AllocFlags::kReturnNull>(
                size, "NullableCharBuffer"));
    size_ = size;
  }

  NullableCharBuffer(const NullableCharBuffer&) = delete;
  NullableCharBuffer& operator=(const NullableCharBuffer&) = delete;

  ~NullableCharBuffer() {
    if (data_)
      WTF::Partitions::BufferPartition()->Free(data_);
  }

  // May return nullptr.
  char* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  char* data_;
  size_t size_;
};

}  // namespace

// Created and destroyed on the same thread, accessed on a background thread as
// well. |string|'s reference counting is *not* thread-safe, hence |string|'s
// reference count must *not* change on the background thread.
struct BackgroundTaskParams final {
  BackgroundTaskParams(
      scoped_refptr<ParkableStringImpl> string,
      const void* data,
      size_t size,
      std::unique_ptr<ReservedChunk> reserved_chunk,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
      : callback_task_runner(callback_task_runner),
        string(std::move(string)),
        data(data),
        size(size),
        reserved_chunk(std::move(reserved_chunk)) {}

  BackgroundTaskParams(const BackgroundTaskParams&) = delete;
  BackgroundTaskParams& operator=(const BackgroundTaskParams&) = delete;
  ~BackgroundTaskParams() { DCHECK(IsMainThread()); }

  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner;
  const scoped_refptr<ParkableStringImpl> string;
  raw_ptr<const void> data;
  const size_t size;
  std::unique_ptr<ReservedChunk> reserved_chunk;
};

// Valid transitions are:
//
// Compression:
// 1. kUnparked -> kParked: Parking completed normally
// 4. kParked -> kUnparked: String has been unparked.
//
// Disk:
// 1. kParked -> kOnDisk: Writing completed successfully
// 4. kOnDisk -> kUnParked: The string is requested, triggering a read and
//    decompression
//
// Since parking and disk writing are not synchronous operations the first time,
// when the asynchronous background task is posted,
// |background_task_in_progress_| is set to true. This prevents further string
// aging, and protects against concurrent background tasks.
//
// Each state can be combined with a string that is either old or
// young. Examples below:
// - kUnParked:
//   - (Very) Old: old strings are not necessarily parked
//   - Young: a string starts young and unparked.
// - kParked:
//   - (Very) Old: Parked, and not touched nor locked since then
//   - Young: Lock() makes a string young but doesn't unpark it.
// - kOnDisk:
//   - Very Old: On disk, and not touched nor locked since then
//   - Young: Lock() makes a string young but doesn't unpark it.
enum class ParkableStringImpl::State : uint8_t { kUnparked, kParked, kOnDisk };

// Current "ownership" status of the underlying data.
//
// - kUnreferencedExternally: |string_| is not referenced externally, and the
//   class is free to change it.
// - kTooManyReferences: |string_| has multiple references pointing to it,
//   cannot change it.
// - kLocked: |this| is locked.
enum class ParkableStringImpl::Status : uint8_t {
  kUnreferencedExternally,
  kTooManyReferences,
  kLocked
};

ParkableStringImpl::ParkableMetadata::ParkableMetadata(
    String string,
    std::unique_ptr<SecureDigest> digest)
    : lock_(),
      lock_depth_(0),
      state_(State::kUnparked),
      compression_failed_(false),
      compressed_(nullptr),
      digest_(*digest),
      age_(Age::kYoung),
      is_8bit_(string.Is8Bit()),
      length_(string.length()) {}

// static
std::unique_ptr<ParkableStringImpl::SecureDigest>
ParkableStringImpl::HashString(StringImpl* string) {
  DigestValue digest_result;

  Digestor digestor(kHashAlgorithmSha256);
  digestor.Update(string->RawByteSpan());
  // Also include encoding in the digest, otherwise two strings with identical
  // byte content but different encoding will be assumed equal, leading to
  // crashes when one is replaced by the other one.
  std::array<uint8_t, 1> is_8bit;
  is_8bit[0] = string->Is8Bit();
  digestor.Update(is_8bit);
  digestor.Finish(digest_result);

  // The only case where this can return false in BoringSSL is an allocation
  // failure of the temporary data required for hashing. In this case, there
  // is nothing better to do than crashing.
  if (digestor.has_failed()) {
    // Don't know the exact size, the SHA256 spec hints at ~64 (block size)
    // + 32 (digest) bytes.
    base::TerminateBecauseOutOfMemory(64 + kDigestSize);
  }
  // Unless SHA256 is... not 256 bits?
  DCHECK(digest_result.size() == kDigestSize);
  return std::make_unique<SecureDigest>(digest_result);
}

// static
scoped_refptr<ParkableStringImpl> ParkableStringImpl::MakeNonParkable(
    scoped_refptr<StringImpl>&& impl) {
  return base::AdoptRef(new ParkableStringImpl(std::move(impl), nullptr));
}

// static
scoped_refptr<ParkableStringImpl> ParkableStringImpl::MakeParkable(
    scoped_refptr<StringImpl>&& impl,
    std::unique_ptr<SecureDigest> digest) {
  DCHECK(!!digest);
  return base::AdoptRef(
      new ParkableStringImpl(std::move(impl), std::move(digest)));
}

// static
ParkableStringImpl::CompressionAlgorithm
ParkableStringImpl::GetCompressionAlgorithm() {
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
  if (base::FeatureList::IsEnabled(features::kUseZstdForParkableStrings)) {
    return CompressionAlgorithm::kZstd;
  }
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
  if (features::ParkableStringsUseSnappy()) {
    return CompressionAlgorithm::kSnappy;
  }
  return CompressionAlgorithm::kZlib;
}

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       std::unique_ptr<SecureDigest> digest)
    : string_(std::move(impl)),
      metadata_(digest ? std::make_unique<ParkableMetadata>(string_,
                                                            std::move(digest))
                       : nullptr)
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  if (!may_be_parked())
    return;
  // There is nothing thread-hostile in this method, but the current design
  // should only reach this path through the main thread.
  AssertOnValidThread();
  DCHECK_EQ(0, lock_depth_for_testing());
  AsanUnpoisonString(string_);
  // Cannot destroy while parking is in progress, as the object is kept alive by
  // the background task.
  DCHECK(!metadata_->background_task_in_progress_);
  DCHECK(!has_on_disk_data());
#if DCHECK_IS_ON()
  ParkableStringManager::Instance().AssertRemoved(this);
#endif
}

void ParkableStringImpl::Lock() {
  if (!may_be_parked())
    return;

  base::AutoLock locker(metadata_->lock_);
  metadata_->lock_depth_ += 1;
  CHECK_NE(metadata_->lock_depth_, 0u);
  // Make young as this is a strong (but not certain) indication that the string
  // will be accessed soon.
  MakeYoung();
}

void ParkableStringImpl::Unlock() {
  if (!may_be_parked())
    return;

  base::AutoLock locker(metadata_->lock_);
  metadata_->lock_depth_ -= 1;
  CHECK_NE(metadata_->lock_depth_, std::numeric_limits<unsigned int>::max());

#if defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
  // There are no external references to the data, nobody should touch the data.
  //
  // Note: Only poison the memory if this is on the owning thread, as this is
  // otherwise racy. Indeed |Unlock()| may be called on any thread, and
  // the owning thread may concurrently call |ToString()|. It is then allowed
  // to use the string until the end of the current owning thread task.
  // Requires DCHECK_IS_ON() for the |owning_thread_| check.
  //
  // Checking the owning thread first as |CurrentStatus()| can only be called
  // from the owning thread.
  if (owning_thread_ == CurrentThread() &&
      CurrentStatus() == Status::kUnreferencedExternally) {
    AsanPoisonString(string_);
  }
#endif  // defined(ADDRESS_SANITIZER) && DCHECK_IS_ON()
}

const String& ParkableStringImpl::ToString() {
  if (!may_be_parked())
    return string_;

  base::AutoLock locker(metadata_->lock_);
  MakeYoung();
  AsanUnpoisonString(string_);
  Unpark();
  return string_;
}

size_t ParkableStringImpl::CharactersSizeInBytes() const {
  if (!may_be_parked())
    return string_.CharactersSizeInBytes();

  return metadata_->length_ * (is_8bit() ? sizeof(LChar) : sizeof(UChar));
}

size_t ParkableStringImpl::MemoryFootprintForDump() const {
  AssertOnValidThread();
  size_t size = sizeof(ParkableStringImpl);

  if (!may_be_parked())
    return size + string_.CharactersSizeInBytes();

  size += sizeof(ParkableMetadata);

  base::AutoLock locker(metadata_->lock_);
  if (!is_parked_no_lock()) {
    size += string_.CharactersSizeInBytes();
  }

  if (metadata_->compressed_)
    size += metadata_->compressed_->size();

  return size;
}

ParkableStringImpl::AgeOrParkResult ParkableStringImpl::MaybeAgeOrParkString() {
  base::AutoLock locker(metadata_->lock_);
  AssertOnValidThread();
  DCHECK(may_be_parked());
  DCHECK(!is_on_disk_no_lock());

  // No concurrent background tasks.
  if (metadata_->background_task_in_progress_)
    return AgeOrParkResult::kSuccessOrTransientFailure;

  // TODO(lizeb): Simplify logic below.
  if (is_parked_no_lock()) {
    if (metadata_->age_ == Age::kVeryOld) {
      bool ok = ParkInternal(ParkingMode::kToDisk);
      if (!ok)
        return AgeOrParkResult::kNonTransientFailure;
    } else {
      metadata_->age_ = MakeOlder(metadata_->age_);
    }
    return AgeOrParkResult::kSuccessOrTransientFailure;
  }

  Status status = CurrentStatus();
  Age age = metadata_->age_;
  if (age == Age::kYoung) {
    if (status == Status::kUnreferencedExternally)
      metadata_->age_ = MakeOlder(age);
  } else if (age == Age::kOld) {
    if (!CanParkNow()) {
      return AgeOrParkResult::kNonTransientFailure;
    }
    bool ok = ParkInternal(ParkingMode::kCompress);
    DCHECK(ok);
    return AgeOrParkResult::kSuccessOrTransientFailure;
  }

  // External references to a string can be long-lived, cannot provide a
  // progress guarantee for this string.
  return status == Status::kTooManyReferences
             ? AgeOrParkResult::kNonTransientFailure
             : AgeOrParkResult::kSuccessOrTransientFailure;
}

bool ParkableStringImpl::Park(ParkingMode mode) {
  base::AutoLock locker(metadata_->lock_);
  AssertOnValidThread();
  DCHECK(may_be_parked());

  if (metadata_->state_ == State::kParked)
    return true;

  // Making the string old to cancel parking if it is accessed/locked before
  // parking is complete.
  metadata_->age_ = Age::kOld;
  if (!CanParkNow())
    return false;

  ParkInternal(mode);
  return true;
}

// Returns false if parking fails and will fail in the future (non-transient
// failure).
bool ParkableStringImpl::ParkInternal(ParkingMode mode) {
  DCHECK(metadata_->state_ == State::kUnparked ||
         metadata_->state_ == State::kParked);
  DCHECK(metadata_->age_ != Age::kYoung);
  DCHECK(CanParkNow());

  // No concurrent background tasks.
  if (metadata_->background_task_in_progress_)
    return true;

  switch (mode) {
    case ParkingMode::kSynchronousOnly:
      if (has_compressed_data())
        DiscardUncompressedData();
      break;
    case ParkingMode::kCompress:
      if (has_compressed_data())
        DiscardUncompressedData();
      else
        PostBackgroundCompressionTask();
      break;
    case ParkingMode::kToDisk:
      auto& manager = ParkableStringManager::Instance();
      if (has_on_disk_data()) {
        DiscardCompressedData();
      } else {
        // If the disk allocator doesn't accept writes, then the failure is not
        // transient, notify the caller. This is important so that
        // ParkableStringManager doesn't endlessly schedule aging tasks when
        // writing to disk is not possible.
        if (!manager.data_allocator().may_write())
          return false;

        auto reserved_chunk = manager.data_allocator().TryReserveChunk(
            metadata_->compressed_->size());
        if (!reserved_chunk) {
          return false;
        }
        PostBackgroundWritingTask(std::move(reserved_chunk));
      }
      break;
  }
  return true;
}

void ParkableStringImpl::DiscardUncompressedData() {
  // Must unpoison the memory before releasing it.
  AsanUnpoisonString(string_);
  string_ = String();

  metadata_->state_ = State::kParked;
  ParkableStringManager::Instance().OnParked(this);
}

void ParkableStringImpl::DiscardCompressedData() {
  metadata_->compressed_ = nullptr;
  metadata_->state_ = State::kOnDisk;
  metadata_->last_disk_parking_time_ = base::TimeTicks::Now();
  ParkableStringManager::Instance().OnWrittenToDisk(this);
}

bool ParkableStringImpl::is_parked_no_lock() const {
  return metadata_->state_ == State::kParked;
}

bool ParkableStringImpl::is_on_disk_no_lock() const {
  return metadata_->state_ == State::kOnDisk;
}

bool ParkableStringImpl::is_compression_failed_no_lock() const {
  return metadata_->compression_failed_;
}

bool ParkableStringImpl::is_parked() const {
  base::AutoLock locker(metadata_->lock_);
  return is_parked_no_lock();
}

bool ParkableStringImpl::is_on_disk() const {
  base::AutoLock locker(metadata_->lock_);
  return is_on_disk_no_lock();
}

ParkableStringImpl::Status ParkableStringImpl::CurrentStatus() const {
  AssertOnValidThread();
  DCHECK(may_be_parked());
  // Can park iff:
  // - |this| is not locked.
  // - There are no external reference to |string_|. Since |this| holds a
  //   reference to |string_|, it must the only one.
  if (metadata_->lock_depth_ != 0)
    return Status::kLocked;
  // Can be null if it is compressed or on disk.
  if (string_.IsNull())
    return Status::kUnreferencedExternally;

  if (!string_.Impl()->HasOneRef())
    return Status::kTooManyReferences;

  return Status::kUnreferencedExternally;
}

bool ParkableStringImpl::CanParkNow() const {
  return CurrentStatus() == Status::kUnreferencedExternally &&
         metadata_->age_ != Age::kYoung && !is_compression_failed_no_lock();
}

void ParkableStringImpl::Unpark() {
  DCHECK(may_be_parked());

  if (metadata_->state_ == State::kUnparked)
    return;

  TRACE_EVENT(
      "blink", "ParkableStringImpl::Unpark", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_parkable_string_unpark();
        data->set_size_bytes(
            base::saturated_cast<int32_t>(CharactersSizeInBytes()));
        int32_t write_time = base::saturated_cast<int32_t>(
            metadata_->last_disk_parking_time_.is_null()
                ? -1
                : (base::TimeTicks::Now() - metadata_->last_disk_parking_time_)
                      .InSeconds());
        data->set_time_since_last_disk_write_sec(write_time);
      });

  DCHECK(metadata_->compressed_ || metadata_->on_disk_metadata_);
  string_ = UnparkInternal();
  if (metadata_->last_disk_parking_time_ != base::TimeTicks()) {
    // Can be quite short, can be multiple hours, hence long times, and 100
    // buckets.
    metadata_->last_disk_parking_time_ = base::TimeTicks();
  }
}

String ParkableStringImpl::UnparkInternal() {
  DCHECK(is_parked_no_lock() || is_on_disk_no_lock());

  base::ElapsedTimer timer;
  auto& manager = ParkableStringManager::Instance();

  base::TimeDelta disk_elapsed = base::TimeDelta::Min();
  if (is_on_disk_no_lock()) {
    TRACE_EVENT("blink", "ParkableStringImpl::ReadFromDisk");
    base::ElapsedTimer disk_read_timer;
    DCHECK(has_on_disk_data());
    metadata_->compressed_ = std::make_unique<Vector<uint8_t>>();
    metadata_->compressed_->Grow(
        base::checked_cast<wtf_size_t>(metadata_->on_disk_metadata_->size()));
    manager.data_allocator().Read(*metadata_->on_disk_metadata_,
                                  metadata_->compressed_->data());
    disk_elapsed = disk_read_timer.Elapsed();
    RecordStatistics(metadata_->on_disk_metadata_->size(), disk_elapsed,
                     ParkingAction::kRead);
  }

  TRACE_EVENT("blink", "ParkableStringImpl::Decompress");
  std::string_view compressed_string_piece(
      reinterpret_cast<const char*>(metadata_->compressed_->data()),
      metadata_->compressed_->size() * sizeof(uint8_t));
  String uncompressed;
  base::span<char> chars;
  if (is_8bit()) {
    base::span<LChar> data;
    uncompressed = String::CreateUninitialized(length(), data);
    chars = base::as_writable_chars(data);
  } else {
    base::span<UChar> data;
    uncompressed = String::CreateUninitialized(length(), data);
    chars = base::as_writable_chars(data);
  }

  switch (GetCompressionAlgorithm()) {
    case CompressionAlgorithm::kZlib: {
      const auto uncompressed_string_piece = base::as_string_view(chars);
      // If the buffer size is incorrect, then we have a corrupted data issue,
      // and in such case there is nothing else to do than crash.
      CHECK_EQ(compression::GetUncompressedSize(compressed_string_piece),
               uncompressed_string_piece.size());
      // If decompression fails, this is either because:
      // 1. Compressed data is corrupted
      // 2. Cannot allocate memory in zlib
      //
      // (1) is data corruption, and (2) is OOM. In all cases, we cannot
      // recover the string we need, nothing else to do than to abort.
      if (!compression::GzipUncompress(compressed_string_piece,
                                       uncompressed_string_piece)) {
        // Since this is almost always OOM, report it as such. We don't have
        // certainty, but memory corruption should be much rarer, and could make
        // us crash anywhere else.
        OOM_CRASH(uncompressed_string_piece.size());
      }
      break;
    }
    case CompressionAlgorithm::kSnappy: {
      size_t uncompressed_size;

      // As above, if size is incorrect, or if data is corrupted, prefer
      // crashing.
      CHECK(snappy::GetUncompressedLength(compressed_string_piece.data(),
                                          compressed_string_piece.size(),
                                          &uncompressed_size));
      CHECK_EQ(uncompressed_size, chars.size());
      CHECK(snappy::RawUncompress(compressed_string_piece.data(),
                                  compressed_string_piece.size(), chars.data()))
          << "Decompression failed, corrupted data?";
      break;
    }
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
    case CompressionAlgorithm::kZstd: {
      uint64_t content_size = ZSTD_getFrameContentSize(
          compressed_string_piece.data(), compressed_string_piece.size());
      // The CHECK()s below indicate memory corruption, terminate.
      CHECK_NE(content_size, ZSTD_CONTENTSIZE_UNKNOWN);
      CHECK_NE(content_size, ZSTD_CONTENTSIZE_ERROR);
      CHECK_EQ(content_size, static_cast<uint64_t>(chars.size()));

      size_t uncompressed_size = ZSTD_decompress(
          chars.data(), chars.size(), compressed_string_piece.data(),
          compressed_string_piece.size());
      CHECK(!ZSTD_isError(uncompressed_size));
      CHECK_EQ(uncompressed_size, chars.size());
      break;
    }
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
  }

  base::TimeDelta elapsed = timer.Elapsed();
  RecordStatistics(CharactersSizeInBytes(), elapsed, ParkingAction::kUnparked);
  metadata_->state_ = State::kUnparked;
  manager.CompleteUnpark(this, elapsed, disk_elapsed);
  return uncompressed;
}

void ParkableStringImpl::ReleaseAndRemoveIfNeeded() const {
  ParkableStringManager::Instance().Remove(
      const_cast<ParkableStringImpl*>(this));
}

void ParkableStringImpl::PostBackgroundCompressionTask() {
  DCHECK(!metadata_->background_task_in_progress_);
  // |string_|'s data should not be touched except in the compression task.
  AsanPoisonString(string_);
  metadata_->background_task_in_progress_ = true;
  auto& manager = ParkableStringManager::Instance();
  DCHECK(manager.task_runner()->BelongsToCurrentThread());
  // |params| keeps |this| alive until |OnParkingCompleteOnMainThread()|.
  auto params = std::make_unique<BackgroundTaskParams>(
      this, string_.Bytes(), string_.CharactersSizeInBytes(),
      /* reserved_chunk */ nullptr, manager.task_runner());
  worker_pool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      CrossThreadBindOnce(&ParkableStringImpl::CompressInBackground,
                          std::move(params)));
}

// static
void ParkableStringImpl::CompressInBackground(
    std::unique_ptr<BackgroundTaskParams> params) {
  TRACE_EVENT(
      "blink", "ParkableStringImpl::CompressInBackground",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_parkable_string_compress_in_background();
        data->set_size_bytes(base::saturated_cast<int32_t>(params->size));
      });

  base::ElapsedTimer timer;
#if defined(ADDRESS_SANITIZER)
  // Lock the string to prevent a concurrent |Unlock()| on the main thread from
  // poisoning the string in the meantime.
  //
  // Don't make the string young at the same time, otherwise parking would
  // always be cancelled on the main thread with address sanitizer, since the
  // |OnParkingCompleteOnMainThread()| callback would be executed on a young
  // string.
  params->string->LockWithoutMakingYoung();
#endif  // defined(ADDRESS_SANITIZER)
  // Compression touches the string.
  AsanUnpoisonString(params->string->string_);
  bool ok;
  std::string_view data(reinterpret_cast<const char*>(params->data.get()),
                        params->size);
  std::unique_ptr<Vector<uint8_t>> compressed;

  // This runs in background, making CPU starvation likely, and not an issue.
  // Hence, report thread time instead of wall clock time.
  base::ElapsedThreadTimer thread_timer;
  {
    // Create a temporary buffer for compressed data. After compression the
    // output bytes are _copied_ to a new vector sized according to the newly
    // discovered compressed size. This is done as a memory saving measure
    // because Vector::Shrink() does not resize the memory allocation.
    //
    // For zlib: the temporary buffer has the same size as the initial data.
    // Compression will fail if this is not large enough.
    // For snappy: the temporary buffer has size
    // GetMaxCompressedLength(inital_data_size). If the compression does not
    // compress, the result is discarded.
    //
    // This is not using:
    // - malloc() or any STL container: this is discouraged in blink, and there
    //   is a suspected memory regression caused by using it (crbug.com/920194).
    // - WTF::Vector<> as allocation failures result in an OOM crash, whereas
    //   we can fail gracefully. See crbug.com/905777 for an example of OOM
    //   triggered from there.

    size_t buffer_size;
    switch (GetCompressionAlgorithm()) {
      case CompressionAlgorithm::kZlib:
        buffer_size = params->size;
        break;
      case CompressionAlgorithm::kSnappy:
        // Contrary to other compression algorithms, snappy requires the buffer
        // to be at least this size, rather than aborting if the provided buffer
        // is too small.
        buffer_size = snappy::MaxCompressedLength(params->size);
        break;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
      case CompressionAlgorithm::kZstd:
        buffer_size = ZSTD_compressBound(params->size);
        break;
#endif
    }

    NullableCharBuffer buffer(buffer_size);
    ok = buffer.data();
    size_t compressed_size;
    if (ok) {
      switch (GetCompressionAlgorithm()) {
        case CompressionAlgorithm::kZlib:
          ok = compression::GzipCompress(data, buffer.data(), buffer.size(),
                                         &compressed_size, nullptr, nullptr);
          break;
        case CompressionAlgorithm::kSnappy:
          snappy::RawCompress(data.data(), params->size, buffer.data(),
                              &compressed_size);
          if (compressed_size > params->size) {
            ok = false;
          }
          break;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
        case CompressionAlgorithm::kZstd:
          compressed_size = ZSTD_compress(
              buffer.data(), buffer.size(), data.data(), params->size,
              features::kZstdCompressionLevel.Get());
          ok = !ZSTD_isError(compressed_size) &&
               (compressed_size < params->size);
          break;
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
      }
    }

#if defined(ADDRESS_SANITIZER)
    params->string->Unlock();
#endif  // defined(ADDRESS_SANITIZER)

    if (ok) {
      compressed = std::make_unique<Vector<uint8_t>>();
      // Not using realloc() as we want the compressed data to be a regular
      // WTF::Vector.
      compressed->AppendSpan(base::as_byte_span(buffer).first(compressed_size));
    }
  }
  base::TimeDelta thread_elapsed = thread_timer.Elapsed();

  auto* task_runner = params->callback_task_runner.get();
  size_t size = params->size;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BackgroundTaskParams> params,
             std::unique_ptr<Vector<uint8_t>> compressed,
             base::TimeDelta parking_thread_time) {
            auto* string = params->string.get();
            string->OnParkingCompleteOnMainThread(
                std::move(params), std::move(compressed), parking_thread_time);
          },
          std::move(params), std::move(compressed), thread_elapsed));
  RecordStatistics(size, timer.Elapsed(), ParkingAction::kParked);
}

void ParkableStringImpl::OnParkingCompleteOnMainThread(
    std::unique_ptr<BackgroundTaskParams> params,
    std::unique_ptr<Vector<uint8_t>> compressed,
    base::TimeDelta parking_thread_time) {
  DCHECK(metadata_->background_task_in_progress_);
  base::AutoLock locker(metadata_->lock_);
  DCHECK_EQ(State::kUnparked, metadata_->state_);
  metadata_->background_task_in_progress_ = false;

  // Always keep the compressed data. Compression is expensive, so even if the
  // uncompressed representation cannot be discarded now, avoid compressing
  // multiple times. This will allow synchronous parking next time.
  DCHECK(!metadata_->compressed_);
  if (compressed) {
    metadata_->compressed_ = std::move(compressed);
  } else {
    metadata_->compression_failed_ = true;
  }

  // Between |Park()| and now, things may have happened:
  // 1. |ToString()| or
  // 2. |Lock()| may have been called.
  //
  // Both of these will make the string young again, and if so we don't
  // discard the compressed representation yet.
  if (CanParkNow() && metadata_->compressed_) {
    // Prevent `data` from dangling, since it points to the uncompressed data
    // freed below.
    params->data = nullptr;
    DiscardUncompressedData();
  } else {
    metadata_->state_ = State::kUnparked;
  }
  // Record the time no matter whether the string was parked or not, as the
  // parking cost was paid.
  ParkableStringManager::Instance().RecordParkingThreadTime(
      parking_thread_time);
}

void ParkableStringImpl::PostBackgroundWritingTask(
    std::unique_ptr<ReservedChunk> reserved_chunk) {
  DCHECK(!metadata_->background_task_in_progress_);
  DCHECK_EQ(State::kParked, metadata_->state_);
  auto& manager = ParkableStringManager::Instance();
  DCHECK(manager.task_runner()->BelongsToCurrentThread());
  auto& data_allocator = manager.data_allocator();
  if (!has_on_disk_data() && data_allocator.may_write()) {
    metadata_->background_task_in_progress_ = true;
    auto params = std::make_unique<BackgroundTaskParams>(
        this, metadata_->compressed_->data(), metadata_->compressed_->size(),
        std::move(reserved_chunk), manager.task_runner());
    worker_pool::PostTask(
        FROM_HERE, {base::MayBlock()},
        CrossThreadBindOnce(&ParkableStringImpl::WriteToDiskInBackground,
                            std::move(params),
                            WTF::CrossThreadUnretained(&data_allocator)));
  }
}

// static
void ParkableStringImpl::WriteToDiskInBackground(
    std::unique_ptr<BackgroundTaskParams> params,
    DiskDataAllocator* data_allocator) {
  base::ElapsedTimer timer;
  auto metadata =
      data_allocator->Write(std::move(params->reserved_chunk), params->data);
  base::TimeDelta elapsed = timer.Elapsed();
  RecordStatistics(params->size, elapsed, ParkingAction::kWritten);

  auto* task_runner = params->callback_task_runner.get();
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BackgroundTaskParams> params,
             std::unique_ptr<DiskDataMetadata> metadata,
             base::TimeDelta elapsed) {
            auto* string = params->string.get();
            string->OnWritingCompleteOnMainThread(std::move(params),
                                                  std::move(metadata), elapsed);
          },
          std::move(params), std::move(metadata), elapsed));
}

void ParkableStringImpl::OnWritingCompleteOnMainThread(
    std::unique_ptr<BackgroundTaskParams> params,
    std::unique_ptr<DiskDataMetadata> on_disk_metadata,
    base::TimeDelta writing_time) {
  base::AutoLock locker(metadata_->lock_);
  DCHECK(metadata_->background_task_in_progress_);
  DCHECK(!metadata_->on_disk_metadata_);

  metadata_->background_task_in_progress_ = false;

  // Writing failed.
  if (!on_disk_metadata)
    return;

  metadata_->on_disk_metadata_ = std::move(on_disk_metadata);
  // State can be:
  // - kParked: unparking didn't happen in the meantime.
  // - Unparked: unparking happened in the meantime.
  DCHECK(metadata_->state_ == State::kUnparked ||
         metadata_->state_ == State::kParked);
  if (metadata_->state_ == State::kParked) {
    // Prevent `data` from dangling, since it points to the compressed data
    // freed below.
    params->data = nullptr;
    DiscardCompressedData();
    DCHECK_EQ(metadata_->state_, State::kOnDisk);
  }

  // Record the time no matter whether the string was discarded or not, as the
  // writing cost was paid.
  ParkableStringManager::Instance().RecordDiskWriteTime(writing_time);
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl)
    : ParkableString(std::move(impl), nullptr) {}

ParkableString::ParkableString(
    scoped_refptr<StringImpl>&& impl,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest) {
  if (!impl) {
    impl_ = nullptr;
    return;
  }

  bool is_parkable = ParkableStringManager::ShouldPark(*impl);
  if (is_parkable) {
    impl_ = ParkableStringManager::Instance().Add(std::move(impl),
                                                  std::move(digest));
  } else {
    impl_ = ParkableStringImpl::MakeNonParkable(std::move(impl));
  }
}

ParkableString::~ParkableString() = default;

void ParkableString::Lock() const {
  if (impl_)
    impl_->Lock();
}

void ParkableString::Unlock() const {
  if (impl_)
    impl_->Unlock();
}

void ParkableString::OnMemoryDump(WebProcessMemoryDump* pmd,
                                  const String& name) const {
  if (!impl_)
    return;

  auto* dump = pmd->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", impl_->MemoryFootprintForDump());

  const char* parent_allocation =
      may_be_parked() ? ParkableStringManager::kAllocatorDumpName
                      : WTF::Partitions::kAllocatedObjectPoolName;
  pmd->AddSuballocation(dump->Guid(), parent_allocation);
}

bool ParkableString::Is8Bit() const {
  return impl_->is_8bit();
}

const String& ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : g_empty_string;
}

size_t ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
