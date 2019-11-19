// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include <string>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/zlib/google/compression_utils.h"

namespace blink {

namespace {

enum class ParkingAction { kParked, kUnparked };

void RecordStatistics(size_t size,
                      base::TimeDelta duration,
                      ParkingAction action) {
  size_t throughput_mb_s =
      static_cast<size_t>(size / duration.InSecondsF()) / 1000000;
  size_t size_kb = size / 1000;
  if (action == ParkingAction::kParked) {
    UMA_HISTOGRAM_COUNTS_10000("Memory.ParkableString.Compression.SizeKb",
                               size_kb);
    // Size is at least 10kB, and at most ~1MB, and compression throughput
    // ranges from single-digit MB/s to ~40MB/s depending on the CPU, hence
    // the range.
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Memory.ParkableString.Compression.Latency", duration,
        base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
        100);
    UMA_HISTOGRAM_COUNTS_1000(
        "Memory.ParkableString.Compression.ThroughputMBps", throughput_mb_s);
  } else {
    UMA_HISTOGRAM_COUNTS_10000("Memory.ParkableString.Decompression.SizeKb",
                               size_kb);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Memory.ParkableString.Decompression.Latency", duration,
        base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
        100);
    // Decompression speed can go up to >500MB/s.
    UMA_HISTOGRAM_COUNTS_1000(
        "Memory.ParkableString.Decompression.ThroughputMBps", throughput_mb_s);
  }
}

void AsanPoisonString(const String& string) {
#if defined(ADDRESS_SANITIZER)
  if (string.IsNull())
    return;
  // Since |string| is not deallocated, it remains in the per-thread
  // AtomicStringTable, where its content can be accessed for equality
  // comparison for instance, triggering a poisoned memory access.
  // See crbug.com/883344 for an example.
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
    data_ =
        reinterpret_cast<char*>(WTF::Partitions::BufferPartition()->AllocFlags(
            base::PartitionAllocReturnNull, size, "NullableCharBuffer"));
    size_ = size;
  }

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

  DISALLOW_COPY_AND_ASSIGN(NullableCharBuffer);
};

}  // namespace

// Created and destroyed on the same thread, accessed on a background thread as
// well. |string|'s reference counting is *not* thread-safe, hence |string|'s
// reference count must *not* change on the background thread.
struct CompressionTaskParams final {
  CompressionTaskParams(
      scoped_refptr<ParkableStringImpl> string,
      const void* data,
      size_t size,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
      : string(string),
        data(data),
        size(size),
        callback_task_runner(std::move(callback_task_runner)) {
    DCHECK(IsMainThread());
  }

  ~CompressionTaskParams() { DCHECK(IsMainThread()); }

  const scoped_refptr<ParkableStringImpl> string;
  const void* data;
  const size_t size;
  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner;

  CompressionTaskParams(CompressionTaskParams&&) = delete;
  DISALLOW_COPY_AND_ASSIGN(CompressionTaskParams);
};

// Valid transitions are:
// 1. kUnparked -> kParkingInProgress: Parking started asynchronously
// 2. kParkingInProgress -> kUnparked: Parking did not complete
// 3. kParkingInProgress -> kParked: Parking completed normally
// 4. kParked -> kUnparked: String has been unparked.
//
// See |Park()| for (1), |OnParkingCompleteOnMainThread()| for 2-3, and
// |Unpark()| for (4).
//
// Each state can be combined with a string that is either old or
// young. Examples below:
// - kUnParked:
//   - Old: old strings are not necessarily parked
//   - Young: a string starts young and unparked.
// - kParkingInProgress:
//   - Old: The string wasn't touched since parking started
//   - Young: The string was either touched or locked since parking started
// - kParked:
//   - Old: Parked, and not touched nor locked since then
//   - Young: Lock() makes a string young but doesn't unpark it.
enum class ParkableStringImpl::State : uint8_t {
  kUnparked,
  kParkingInProgress,
  kParked
};

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

ParkableStringImpl::ParkableStringImpl(scoped_refptr<StringImpl>&& impl,
                                       ParkableState parkable)
    : mutex_(),
      lock_depth_(0),
      state_(State::kUnparked),
      string_(std::move(impl)),
      compressed_(nullptr),
      is_young_(true),
      may_be_parked_(parkable == ParkableState::kParkable),
      is_8bit_(string_.Is8Bit()),
      length_(string_.length()),
      hash_(string_.Impl()->GetHash())
#if DCHECK_IS_ON()
      ,
      owning_thread_(CurrentThread())
#endif
{
  DCHECK(!string_.IsNull());
}

ParkableStringImpl::~ParkableStringImpl() {
  AssertOnValidThread();
  if (!may_be_parked())
    return;

  DCHECK_EQ(0, lock_depth_for_testing());
  AsanUnpoisonString(string_);
  DCHECK(state_ == State::kParked || state_ == State::kUnparked);

  ParkableStringManager::Instance().Remove(this);
}

void ParkableStringImpl::Lock() {
  if (!may_be_parked_)
    return;

  MutexLocker locker(mutex_);
  lock_depth_ += 1;
  // Make young as this is a strong (but not certain) indication that the string
  // will be accessed soon.
  MakeYoung();
}

#if defined(ADDRESS_SANITIZER)

void ParkableStringImpl::LockWithoutMakingYoung() {
  DCHECK(may_be_parked());
  MutexLocker locker(mutex_);
  lock_depth_ += 1;
}

#endif  // defined(ADDRESS_SANITIZER)

void ParkableStringImpl::Unlock() {
  if (!may_be_parked())
    return;

  MutexLocker locker(mutex_);
  DCHECK_GT(lock_depth_, 0);
  lock_depth_ -= 1;

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

void ParkableStringImpl::PurgeMemory() {
  AssertOnValidThread();
  if (state_ == State::kUnparked)
    compressed_ = nullptr;
}

bool ParkableStringImpl::Equal(const ParkableStringImpl& rhs) const {
  // This is called when two strings share the same bucket. Either string can
  // be parked.
  //
  // As a consequence, for a hash collision or true equality, a string can be
  // unparked in this function, making it expensive (see below).
  AssertOnValidThread();

  if (this == &rhs)
    return true;

  // The hash is actually only 24 bits. If collisions become an issue, replace
  // it either with a stronger one (murmur2 for instance), or switch to SHA256
  // and don't check for equality.
  if (GetHash() != rhs.GetHash() || length() != rhs.length())
    return false;

  // Can be expensive.
  //
  // Using the transient version because otherwise a lot of code in
  // ParkableStringManager becomes more reentrant than it already is, and many
  // parts of ParkableStringImpl would become mutable.
  // For instance, ToString() can unpark a string, and that would call into
  // ParkableStringManager::OnUnparked(), from ParkableStringManager::Add().
  //
  // Note that we only get here in two cases:
  // - Hash collision
  // - True equality
  //
  // Assuming that collisions are rare, unparking for true equality is not too
  // bad, as the obvious alternative is to compare a cryptographic hash. On
  // low-end ARM devices, unparking is roughly as expensive as SHA256, but we
  // only do it for one of the two strings, so this is still expected to be
  // faster than SHA256, at the cost of more memory (since both strings are held
  // in memory temporarily).
  return ToStringTransient() == rhs.ToStringTransient();
}

bool ParkableStringImpl::Equal(scoped_refptr<StringImpl> string) const {
  // See above for comments about cost of this function. This is called from
  // ParkableStringManager::Add() through ParkableStringImplTranslator.
  AssertOnValidThread();

  if (GetHash() != string->GetHash() || length() != string->length())
    return false;

  return ToStringTransient() == String(string);
}

void ParkableStringImpl::MakeYoung() {
  mutex_.AssertAcquired();
  is_young_ = true;
}

const String& ParkableStringImpl::ToString() {
  AssertOnValidThread();
  if (!may_be_parked())
    return string_;

  MutexLocker locker(mutex_);
  MakeYoung();
  AsanUnpoisonString(string_);
  Unpark();
  return string_;
}

String ParkableStringImpl::ToStringTransient() const {
  AssertOnValidThread();
  if (!is_parked()) {
    AsanUnpoisonString(string_);
    return string_;
  }

  return UnparkInternal();
}

unsigned ParkableStringImpl::CharactersSizeInBytes() const {
  AssertOnValidThread();
  return length_ * (is_8bit() ? sizeof(LChar) : sizeof(UChar));
}

ParkableStringImpl::AgeOrParkResult ParkableStringImpl::MaybeAgeOrParkString() {
  MutexLocker locker(mutex_);
  AssertOnValidThread();
  DCHECK(may_be_parked());
  DCHECK(!is_parked());

  Status status = CurrentStatus();
  if (is_young_) {
    if (status == Status::kUnreferencedExternally)
      is_young_ = false;
  } else {
    if (state_ == State::kParkingInProgress)
      return AgeOrParkResult::kSuccessOrTransientFailure;

    if (CanParkNow()) {
      ParkInternal(ParkingMode::kAlways);
      return AgeOrParkResult::kSuccessOrTransientFailure;
    }
  }

  // External references to a string can be long-lived, cannot provide a
  // progress guarantee for this string.
  return status == Status::kTooManyReferences
             ? AgeOrParkResult::kNonTransientFailure
             : AgeOrParkResult::kSuccessOrTransientFailure;
}

bool ParkableStringImpl::Park(ParkingMode mode) {
  MutexLocker locker(mutex_);
  AssertOnValidThread();
  DCHECK(may_be_parked());

  if (state_ == State::kParkingInProgress || state_ == State::kParked)
    return true;

  // Making the string old to cancel parking if it is accessed/locked before
  // parking is complete.
  is_young_ = false;
  if (!CanParkNow())
    return false;

  ParkInternal(mode);
  return true;
}

void ParkableStringImpl::ParkInternal(ParkingMode mode) {
  mutex_.AssertAcquired();
  DCHECK_EQ(State::kUnparked, state_);
  DCHECK(!is_young_);
  DCHECK(CanParkNow());

  // Parking can proceed synchronously.
  if (has_compressed_data()) {
    state_ = State::kParked;
    ParkableStringManager::Instance().OnParked(this);

    // Must unpoison the memory before releasing it.
    AsanUnpoisonString(string_);
    string_ = String();
  } else if (mode == ParkingMode::kAlways) {
    // |string_|'s data should not be touched except in the compression task.
    AsanPoisonString(string_);
    // |params| keeps |this| alive until |OnParkingCompleteOnMainThread()|.
    auto params = std::make_unique<CompressionTaskParams>(
        this, string_.Bytes(), string_.CharactersSizeInBytes(),
        Thread::Current()->GetTaskRunner());
    worker_pool::PostTask(
        FROM_HERE,
        CrossThreadBindOnce(&ParkableStringImpl::CompressInBackground,
                            WTF::Passed(std::move(params))));
    state_ = State::kParkingInProgress;
  }
}

bool ParkableStringImpl::is_parked() const {
  return state_ == State::kParked;
}

ParkableStringImpl::Status ParkableStringImpl::CurrentStatus() const {
  AssertOnValidThread();
  mutex_.AssertAcquired();
  DCHECK(may_be_parked());
  // Can park iff:
  // - |this| is not locked.
  // - There are no external reference to |string_|. Since |this| holds a
  //   reference to |string_|, it must the only one.
  if (lock_depth_ != 0)
    return Status::kLocked;
  if (!string_.Impl()->HasOneRef())
    return Status::kTooManyReferences;
  return Status::kUnreferencedExternally;
}

bool ParkableStringImpl::CanParkNow() const {
  return CurrentStatus() == Status::kUnreferencedExternally && !is_young_;
}

void ParkableStringImpl::Unpark() {
  AssertOnValidThread();
  DCHECK(may_be_parked());
  mutex_.AssertAcquired();
  if (state_ != State::kParked)
    return;

  TRACE_EVENT1("blink", "ParkableStringImpl::Unpark", "size",
               CharactersSizeInBytes());
  DCHECK(compressed_);
  string_ = UnparkInternal();
  state_ = State::kUnparked;
  ParkableStringManager::Instance().OnUnparked(this);
}

String ParkableStringImpl::UnparkInternal() const {
  AssertOnValidThread();
  DCHECK(is_parked());
  // Note: No need for |mutex_| to be held, this doesn't touch any member
  // variable protected by it.

  base::ElapsedTimer timer;
  base::StringPiece compressed_string_piece(
      reinterpret_cast<const char*>(compressed_->data()),
      compressed_->size() * sizeof(uint8_t));
  String uncompressed;
  base::StringPiece uncompressed_string_piece;
  size_t size = CharactersSizeInBytes();
  if (is_8bit()) {
    LChar* data;
    uncompressed = String::CreateUninitialized(length(), data);
    uncompressed_string_piece =
        base::StringPiece(reinterpret_cast<const char*>(data), size);
  } else {
    UChar* data;
    uncompressed = String::CreateUninitialized(length(), data);
    uncompressed_string_piece =
        base::StringPiece(reinterpret_cast<const char*>(data), size);
  }

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
  //
  // Stability sheriffs: If you see this, this is likely an OOM.
  CHECK(compression::GzipUncompress(compressed_string_piece,
                                    uncompressed_string_piece));

  base::TimeDelta elapsed = timer.Elapsed();
  ParkableStringManager::Instance().RecordUnparkingTime(elapsed);
  RecordStatistics(CharactersSizeInBytes(), elapsed, ParkingAction::kUnparked);

  return uncompressed;
}

void ParkableStringImpl::OnParkingCompleteOnMainThread(
    std::unique_ptr<CompressionTaskParams> params,
    std::unique_ptr<Vector<uint8_t>> compressed,
    base::TimeDelta parking_thread_time) {
  MutexLocker locker(mutex_);
  DCHECK_EQ(State::kParkingInProgress, state_);

  // Always keep the compressed data. Compression is expensive, so even if the
  // uncompressed representation cannot be discarded now, avoid compressing
  // multiple times. This will allow synchronous parking next time.
  DCHECK(!compressed_);
  if (compressed)
    compressed_ = std::move(compressed);

  // Between |Park()| and now, things may have happened:
  // 1. |ToString()| or
  // 2. |Lock()| may have been called.
  //
  // Both of these will make the string young again, and if so we don't
  // discard the compressed representation yet.
  if (CanParkNow() && compressed_) {
    state_ = State::kParked;
    ParkableStringManager::Instance().OnParked(this);

    // Must unpoison the memory before releasing it.
    AsanUnpoisonString(string_);
    string_ = String();
  } else {
    state_ = State::kUnparked;
  }
  // Record the time no matter whether the string was parked or not, as the
  // parking cost was paid.
  ParkableStringManager::Instance().RecordParkingThreadTime(
      parking_thread_time);
}

// static
void ParkableStringImpl::CompressInBackground(
    std::unique_ptr<CompressionTaskParams> params) {
  TRACE_EVENT1("blink", "ParkableStringImpl::CompressInBackground", "size",
               params->size);

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
  base::StringPiece data(reinterpret_cast<const char*>(params->data),
                         params->size);
  std::unique_ptr<Vector<uint8_t>> compressed = nullptr;

  // This runs in background, making CPU starvation likely, and not an issue.
  // Hence, report thread time instead of wall clock time.
  base::ElapsedThreadTimer thread_timer;
  {
    // Temporary vector. As we don't want to waste memory, the temporary buffer
    // has the same size as the initial data. Compression will fail if this is
    // not large enough.
    //
    // This is not using:
    // - malloc() or any STL container: this is discouraged in blink, and there
    //   is a suspected memory regression caused by using it (crbug.com/920194).
    // - WTF::Vector<> as allocation failures result in an OOM crash, whereas
    //   we can fail gracefully. See crbug.com/905777 for an example of OOM
    //   triggered from there.
    NullableCharBuffer buffer(params->size);
    ok = buffer.data();
    size_t compressed_size;
    if (ok) {
      // Use partition alloc for zlib's temporary data. This is crucial to avoid
      // leaking memory on Android, see the details in crbug.com/931553.
      auto fast_malloc = [](size_t size) {
        return WTF::Partitions::FastMalloc(size, "ZlibTemporaryData");
      };
      ok = compression::GzipCompress(data, buffer.data(), buffer.size(),
                                     &compressed_size, fast_malloc,
                                     WTF::Partitions::FastFree);
    }

#if defined(ADDRESS_SANITIZER)
    params->string->Unlock();
#endif  // defined(ADDRESS_SANITIZER)

    if (ok) {
      compressed = std::make_unique<Vector<uint8_t>>();
      // Not using realloc() as we want the compressed data to be a regular
      // WTF::Vector.
      compressed->Append(reinterpret_cast<const uint8_t*>(buffer.data()),
                         compressed_size);
    }
  }
  base::TimeDelta thread_elapsed = thread_timer.Elapsed();

  auto* task_runner = params->callback_task_runner.get();
  size_t size = params->size;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<CompressionTaskParams> params,
             std::unique_ptr<Vector<uint8_t>> compressed,
             base::TimeDelta parking_thread_time) {
            auto* string = params->string.get();
            string->OnParkingCompleteOnMainThread(
                std::move(params), std::move(compressed), parking_thread_time);
          },
          WTF::Passed(std::move(params)), WTF::Passed(std::move(compressed)),
          thread_elapsed));
  RecordStatistics(size, timer.Elapsed(), ParkingAction::kParked);
}

ParkableString::ParkableString(scoped_refptr<StringImpl>&& impl) {
  if (!impl) {
    impl_ = nullptr;
    return;
  }

  bool is_parkable = ParkableStringManager::ShouldPark(*impl);
  if (is_parkable) {
    impl_ = ParkableStringManager::Instance().Add(std::move(impl));
  } else {
    impl_ = base::MakeRefCounted<ParkableStringImpl>(
        std::move(impl), ParkableStringImpl::ParkableState::kNotParkable);
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
  // Parkable strings are reported by ParkableStringManager.
  if (!impl_ || may_be_parked())
    return;

  auto* dump = pmd->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", CharactersSizeInBytes());
  pmd->AddSuballocation(dump->Guid(),
                        String(WTF::Partitions::kAllocatedObjectPoolName));
}

bool ParkableString::Is8Bit() const {
  return impl_->is_8bit();
}

const String& ParkableString::ToString() const {
  return impl_ ? impl_->ToString() : g_empty_string;
}

wtf_size_t ParkableString::CharactersSizeInBytes() const {
  return impl_ ? impl_->CharactersSizeInBytes() : 0;
}

}  // namespace blink
