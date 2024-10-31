// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/disk_data_allocator.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

struct ParkableStringManager::Statistics {
  size_t original_size;
  size_t uncompressed_size;
  size_t compressed_original_size;
  size_t compressed_size;
  size_t metadata_size;
  size_t overhead_size;
  size_t total_size;
  int64_t savings_size;
  size_t on_disk_size;
};

namespace {

bool CompressionEnabled() {
  return base::FeatureList::IsEnabled(features::kCompressParkableStrings);
}

class OnPurgeMemoryListener : public GarbageCollected<OnPurgeMemoryListener>,
                              public MemoryPressureListener {
  void OnPurgeMemory() override {
    if (!CompressionEnabled()) {
      return;
    }
    ParkableStringManager::Instance().PurgeMemory();
  }
};

Vector<ParkableStringImpl*> EnumerateStrings(
    const ParkableStringManager::StringMap& strings) {
  WTF::Vector<ParkableStringImpl*> all_strings;
  all_strings.reserve(strings.size());

  for (const auto& kv : strings)
    all_strings.push_back(kv.value);

  return all_strings;
}

void MoveString(ParkableStringImpl* string,
                ParkableStringManager::StringMap* from,
                ParkableStringManager::StringMap* to) {
  auto it = from->find(string->digest());
  CHECK(it != from->end(), base::NotFatalUntil::M130);
  DCHECK_EQ(it->value, string);
  from->erase(it);
  auto insert_result = to->insert(string->digest(), string);
  DCHECK(insert_result.is_new_entry);
}

}  // namespace

const char* ParkableStringManager::kAllocatorDumpName = "parkable_strings";
const base::TimeDelta ParkableStringManager::kFirstParkingDelay;

// static
ParkableStringManagerDumpProvider*
ParkableStringManagerDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(ParkableStringManagerDumpProvider, instance, ());
  return &instance;
}

bool ParkableStringManagerDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  return ParkableStringManager::Instance().OnMemoryDump(pmd);
}

ParkableStringManagerDumpProvider::~ParkableStringManagerDumpProvider() =
    default;
ParkableStringManagerDumpProvider::ParkableStringManagerDumpProvider() =
    default;

ParkableStringManager& ParkableStringManager::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ParkableStringManager, instance, ());
  return instance;
}

ParkableStringManager::~ParkableStringManager() = default;

void ParkableStringManager::SetRendererBackgrounded(bool backgrounded) {
  DCHECK(IsMainThread());
  bool was_paused = IsPaused();
  backgrounded_ = backgrounded;

  if (was_paused && !IsPaused() && HasPendingWork()) {
    ScheduleAgingTaskIfNeeded();
  }
}

void ParkableStringManager::OnRAILModeChanged(RAILMode rail_mode) {
  DCHECK(IsMainThread());
  bool was_paused = IsPaused();
  rail_mode_ = rail_mode;

  if (was_paused && !IsPaused() && HasPendingWork()) {
    ScheduleAgingTaskIfNeeded();
  }
}

bool ParkableStringManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(kAllocatorDumpName);

  Statistics stats = ComputeStatistics();

  dump->AddScalar("size", "bytes", stats.total_size);
  dump->AddScalar("original_size", "bytes", stats.original_size);
  dump->AddScalar("uncompressed_size", "bytes", stats.uncompressed_size);
  dump->AddScalar("compressed_size", "bytes", stats.compressed_size);
  dump->AddScalar("metadata_size", "bytes", stats.metadata_size);
  dump->AddScalar("overhead_size", "bytes", stats.overhead_size);
  // Has to be uint64_t.
  dump->AddScalar("savings_size", "bytes",
                  stats.savings_size > 0 ? stats.savings_size : 0);
  dump->AddScalar("on_disk_size", "bytes", stats.on_disk_size);
  dump->AddScalar("on_disk_footprint", "bytes",
                  data_allocator().disk_footprint());
  dump->AddScalar("on_disk_free_chunks", "bytes",
                  data_allocator().free_chunks_size());

  pmd->AddSuballocation(dump->guid(),
                        WTF::Partitions::kAllocatedObjectPoolName);
  return true;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread() &&
         CompressionEnabled();
}

// static
base::TimeDelta ParkableStringManager::AgingInterval() {
  return base::FeatureList::IsEnabled(features::kLessAggressiveParkableString)
             ? kLessAggressiveAgingInterval
             : kAgingInterval;
}

scoped_refptr<ParkableStringImpl> ParkableStringManager::Add(
    scoped_refptr<StringImpl>&& string,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest) {
  DCHECK(IsMainThread());

  ScheduleAgingTaskIfNeeded();

  auto string_impl = string;
  if (!digest)
    digest = ParkableStringImpl::HashString(string_impl.get());
  DCHECK(digest.get());

  auto it = unparked_strings_.find(digest.get());
  if (it != unparked_strings_.end())
    return it->value;

  it = parked_strings_.find(digest.get());
  if (it != parked_strings_.end())
    return it->value;

  it = on_disk_strings_.find(digest.get());
  if (it != on_disk_strings_.end())
    return it->value;

  // No hit, new unparked string.
  auto new_parkable = ParkableStringImpl::MakeParkable(std::move(string_impl),
                                                       std::move(digest));
  auto insert_result =
      unparked_strings_.insert(new_parkable->digest(), new_parkable.get());
  DCHECK(insert_result.is_new_entry);

  // Lazy registration because registering too early can cause crashes on Linux,
  // see crbug.com/930117, and registering without any strings is pointless
  // anyway.
  if (!did_register_memory_pressure_listener_) {
    // No need to ever unregister, as the only ParkableStringManager instance
    // lives forever.
    MemoryPressureListenerRegistry::Instance().RegisterClient(
        MakeGarbageCollected<OnPurgeMemoryListener>());
    did_register_memory_pressure_listener_ = true;
  }

  if (!has_posted_unparking_time_accounting_task_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringManager::RecordStatisticsAfter5Minutes,
                       base::Unretained(this)),
        base::Minutes(5));
    has_posted_unparking_time_accounting_task_ = true;
  }

  return new_parkable;
}

void ParkableStringManager::RemoveOnMainThread(ParkableStringImpl* string) {
  DCHECK(IsMainThread());
  DCHECK(string->may_be_parked());
  DCHECK(string->digest());

  {
    base::AutoLock locker(string->metadata_->lock_);
    // `RefCountedThreadSafeBase::Release()` may return false if the Main
    // Thread took a new reference to the string between the moment this task
    // was posted from a background thread and its execution.
    if (!string->RefCountedThreadSafeBase::Release()) {
      return;
    }

    StringMap* map = nullptr;
    if (string->is_on_disk_no_lock()) {
      map = &on_disk_strings_;
    } else if (string->is_parked_no_lock()) {
      map = &parked_strings_;
    } else {
      map = &unparked_strings_;
    }

    auto it = map->find(string->digest());
    CHECK(it != map->end(), base::NotFatalUntil::M130);
    map->erase(it);
  }

  if (string->has_on_disk_data()) {
    data_allocator().Discard(std::move(string->metadata_->on_disk_metadata_));
    // Now data_allocator may have enough free space for pending compressed
    // strings. Schedule for them.
    ScheduleAgingTaskIfNeeded();
  }

  delete string;
}

void ParkableStringManager::Remove(ParkableStringImpl* string) {
  if (task_runner_->BelongsToCurrentThread()) {
    RemoveOnMainThread(string);
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ParkableStringManager::RemoveOnMainThread,
                     base::Unretained(this), base::Unretained(string)));
}

void ParkableStringManager::CompleteUnparkOnMainThread(
    ParkableStringImpl* string,
    base::TimeDelta elapsed,
    base::TimeDelta disk_elapsed) {
  DCHECK(IsMainThread());
  bool was_on_disk = !disk_elapsed.is_min();
  RecordUnparkingTime(elapsed);
  OnUnparked(string, was_on_disk);
  if (was_on_disk) {
    RecordDiskReadTime(disk_elapsed);
  }
}

void ParkableStringManager::CompleteUnpark(ParkableStringImpl* string,
                                           base::TimeDelta elapsed,
                                           base::TimeDelta disk_elapsed) {
  // The task runner is bound to the main thread.
  if (task_runner_->BelongsToCurrentThread()) {
    CompleteUnparkOnMainThread(string, elapsed, disk_elapsed);
    return;
  }
  // Use a retained reference to prevent `string` from being deleted before
  // `CompleteUnpark()` is executed in the main thread.
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ParkableStringManager::CompleteUnparkOnMainThread,
                          base::Unretained(this), base::RetainedRef(string),
                          elapsed, disk_elapsed));
}

void ParkableStringManager::OnParked(ParkableStringImpl* newly_parked_string) {
  DCHECK(IsMainThread());
  DCHECK(newly_parked_string->may_be_parked());
  MoveString(newly_parked_string, &unparked_strings_, &parked_strings_);
}

void ParkableStringManager::OnWrittenToDisk(
    ParkableStringImpl* newly_written_string) {
  DCHECK(IsMainThread());
  DCHECK(newly_written_string->may_be_parked());
  MoveString(newly_written_string, &parked_strings_, &on_disk_strings_);
}

void ParkableStringManager::OnUnparked(ParkableStringImpl* was_parked_string,
                                       bool was_on_disk) {
  DCHECK(IsMainThread());
  DCHECK(was_parked_string->may_be_parked());
  StringMap* from_map = was_on_disk ? &on_disk_strings_ : &parked_strings_;
  MoveString(was_parked_string, from_map, &unparked_strings_);
  ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::ParkAll(ParkableStringImpl::ParkingMode mode) {
  DCHECK(IsMainThread());
  DCHECK(CompressionEnabled());

  // Parking may be synchronous, need to copy values first.
  // In case of synchronous parking, |ParkableStringImpl::Park()| calls
  // |OnParked()|, which moves the string from |unparked_strings_|
  // to |parked_strings_|, hence the need to copy values first.
  //
  // Efficiency: In practice, either we are parking strings for the first time,
  // and |unparked_strings_| can contain a few 10s of strings (and we will
  // trigger expensive compression), or this is a subsequent one, and
  // |unparked_strings_| will have few entries.
  auto unparked = EnumerateStrings(unparked_strings_);

  for (ParkableStringImpl* str : unparked) {
    str->Park(mode);
  }
}

size_t ParkableStringManager::Size() const {
  DCHECK(IsMainThread());

  return parked_strings_.size() + unparked_strings_.size();
}

void ParkableStringManager::RecordStatisticsAfter5Minutes() const {
  if (!CompressionEnabled()) {
    return;
  }

  base::UmaHistogramTimes("Memory.ParkableString.TotalParkingThreadTime.5min",
                          total_parking_thread_time_);
  base::UmaHistogramTimes("Memory.ParkableString.TotalUnparkingTime.5min",
                          total_unparking_time_);

  // These metrics only make sense if the disk allocator is used.
  if (data_allocator().may_write()) {
    Statistics stats = ComputeStatistics();
    base::UmaHistogramTimes("Memory.ParkableString.DiskWriteTime.5min",
                            total_disk_write_time_);
    base::UmaHistogramTimes("Memory.ParkableString.DiskReadTime.5min",
                            total_disk_read_time_);
    base::UmaHistogramCounts100000("Memory.ParkableString.OnDiskSizeKb.5min",
                                   static_cast<int>(stats.on_disk_size / 1000));
  }
}

void ParkableStringManager::AgeStringsAndPark() {
  DCHECK(CompressionEnabled());
  has_pending_aging_task_ = false;

  if (IsPaused()) {
    return;
  }

  TRACE_EVENT0("blink", "ParkableStringManager::AgeStringsAndPark");
  auto unparked = EnumerateStrings(unparked_strings_);
  auto parked = EnumerateStrings(parked_strings_);

  bool can_make_progress = false;
  for (ParkableStringImpl* str : unparked) {
    if (str->MaybeAgeOrParkString() ==
        ParkableStringImpl::AgeOrParkResult::kSuccessOrTransientFailure) {
      can_make_progress = true;
    }
  }

  for (ParkableStringImpl* str : parked) {
    if (str->MaybeAgeOrParkString() ==
        ParkableStringImpl::AgeOrParkResult::kSuccessOrTransientFailure) {
      can_make_progress = true;
    }
  }

  // Some strings will never be parkable because there are lasting external
  // references to them. Don't endlessely reschedule the aging task if we are
  // not making progress (that is, no new string was either aged or parked).
  //
  // This ensures that the tasks will stop getting scheduled, assuming that
  // the renderer is otherwise idle. Note that we cannot use "idle" tasks as
  // we need to age and park strings after the renderer becomes idle, meaning
  // that this has to run when the idle tasks are not. As a consequence, it
  // is important to make sure that this will not reschedule tasks forever.
  bool reschedule = HasPendingWork() && can_make_progress;
  if (reschedule)
    ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::ScheduleAgingTaskIfNeeded() {
  if (IsPaused()) {
    return;
  }

  if (has_pending_aging_task_)
    return;

  base::TimeDelta delay = AgingInterval();
  // Delay the first aging tick, since this renderer may be short-lived, we do
  // not want to waste CPU time compressing memory that is going away soon.
  if (!first_string_aging_was_delayed_) {
    delay = kFirstParkingDelay;
    first_string_aging_was_delayed_ = true;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParkableStringManager::AgeStringsAndPark,
                     base::Unretained(this)),
      delay);
  has_pending_aging_task_ = true;
}

void ParkableStringManager::PurgeMemory() {
  DCHECK(IsMainThread());
  DCHECK(CompressionEnabled());

  ParkAll(ParkableStringImpl::ParkingMode::kCompress);
}

ParkableStringManager::Statistics ParkableStringManager::ComputeStatistics()
    const {
  ParkableStringManager::Statistics stats = {};
  // The digest has an inline capacity set to the digest size, hence sizeof() is
  // accurate.
  constexpr size_t kParkableStringImplActualSize =
      sizeof(ParkableStringImpl) + sizeof(ParkableStringImpl::ParkableMetadata);

  for (const auto& kv : unparked_strings_) {
    ParkableStringImpl* str = kv.value;
    size_t size = str->CharactersSizeInBytes();
    stats.original_size += size;
    stats.uncompressed_size += size;
    stats.metadata_size += kParkableStringImplActualSize;

    if (str->has_compressed_data())
      stats.overhead_size += str->compressed_size();

    if (str->has_on_disk_data())
      stats.on_disk_size += str->on_disk_size();

    // Since ParkableStringManager wants to have a finer breakdown of memory
    // footprint, this doesn't directly use
    // |ParkableStringImpl::MemoryFootprintForDump()|. However we want the two
    // computations to be consistent, hence the DCHECK().
    size_t memory_footprint =
        (str->has_compressed_data() ? str->compressed_size() : 0) + size +
        kParkableStringImplActualSize;
    DCHECK_EQ(memory_footprint, str->MemoryFootprintForDump());
  }

  for (const auto& kv : parked_strings_) {
    ParkableStringImpl* str = kv.value;
    size_t size = str->CharactersSizeInBytes();
    stats.compressed_original_size += size;
    stats.original_size += size;
    stats.compressed_size += str->compressed_size();
    stats.metadata_size += kParkableStringImplActualSize;

    if (str->has_on_disk_data())
      stats.on_disk_size += str->on_disk_size();

    // See comment above.
    size_t memory_footprint =
        str->compressed_size() + kParkableStringImplActualSize;
    DCHECK_EQ(memory_footprint, str->MemoryFootprintForDump());
  }

  for (const auto& kv : on_disk_strings_) {
    ParkableStringImpl* str = kv.value;
    size_t size = str->CharactersSizeInBytes();
    stats.original_size += size;
    stats.metadata_size += kParkableStringImplActualSize;
    stats.on_disk_size += str->on_disk_size();
  }

  stats.total_size = stats.uncompressed_size + stats.compressed_size +
                     stats.metadata_size + stats.overhead_size;
  size_t memory_footprint = stats.compressed_size + stats.uncompressed_size +
                            stats.metadata_size + stats.overhead_size;
  stats.savings_size =
      stats.original_size - static_cast<int64_t>(memory_footprint);

  return stats;
}

void ParkableStringManager::AssertRemoved(ParkableStringImpl* string) {
#if DCHECK_IS_ON()
  auto it = on_disk_strings_.find(string->digest());
  DCHECK_EQ(it, on_disk_strings_.end());

  it = parked_strings_.find(string->digest());
  DCHECK_EQ(it, parked_strings_.end());

  it = unparked_strings_.find(string->digest());
  DCHECK_EQ(it, unparked_strings_.end());
#endif
}

void ParkableStringManager::ResetForTesting() {
  has_pending_aging_task_ = false;
  has_posted_unparking_time_accounting_task_ = false;
  did_register_memory_pressure_listener_ = false;
  total_unparking_time_ = base::TimeDelta();
  total_parking_thread_time_ = base::TimeDelta();
  total_disk_read_time_ = base::TimeDelta();
  total_disk_write_time_ = base::TimeDelta();
  unparked_strings_.clear();
  parked_strings_.clear();
  on_disk_strings_.clear();
  allocator_for_testing_ = nullptr;
  first_string_aging_was_delayed_ = false;
}

bool ParkableStringManager::IsPaused() const {
  DCHECK(IsMainThread());
  if (!CompressionEnabled()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kLessAggressiveParkableString)) {
    return false;
  }

  return !(backgrounded_ && (rail_mode_ != RAILMode::kLoad));
}

bool ParkableStringManager::HasPendingWork() const {
  return !unparked_strings_.empty() || !parked_strings_.empty();
}

bool ParkableStringManager::IsOnParkedMapForTesting(
    ParkableStringImpl* string) {
  auto it = parked_strings_.find(string->digest());
  return it != parked_strings_.end();
}

bool ParkableStringManager::IsOnDiskMapForTesting(ParkableStringImpl* string) {
  auto it = on_disk_strings_.find(string->digest());
  return it != on_disk_strings_.end();
}

ParkableStringManager::ParkableStringManager()
    : task_runner_(Thread::MainThread()->GetTaskRunner(
          MainThreadTaskRunnerRestricted())) {
  // Should unregister in the destructor, but `this` is a NoDestructor static
  // local.
  ThreadScheduler::Current()->ToMainThreadScheduler()->AddRAILModeObserver(
      this);
}

}  // namespace blink
