// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

const base::Feature kParkableImagesToDisk{"ParkableImagesToDisk",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

struct ParkableImageManager::Statistics {
  size_t unparked_size = 0;
  size_t on_disk_size = 0;
  size_t total_size = 0;
};

constexpr const char* ParkableImageManager::kAllocatorDumpName;

constexpr base::TimeDelta ParkableImageManager::kDelayedParkingInterval;

// static
ParkableImageManager& ParkableImageManager::Instance() {
  static base::NoDestructor<ParkableImageManager> instance;
  return *instance;
}

bool ParkableImageManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = pmd->CreateAllocatorDump(kAllocatorDumpName);

  MutexLocker lock(lock_);
  Statistics stats = ComputeStatistics();

  dump->AddScalar("total_size", "bytes", stats.total_size);
  dump->AddScalar("unparked_size", "bytes", stats.unparked_size);
  dump->AddScalar("on_disk_size", "bytes", stats.on_disk_size);

  return true;
}

ParkableImageManager::Statistics ParkableImageManager::ComputeStatistics()
    const {
  Statistics stats;

  for (auto* unparked : unparked_images_)
    stats.unparked_size += unparked->size();

  for (auto* on_disk : on_disk_images_)
    stats.on_disk_size += on_disk->size();

  stats.total_size = stats.on_disk_size + stats.unparked_size;

  return stats;
}

size_t ParkableImageManager::Size() const {
  MutexLocker lock(lock_);

  return on_disk_images_.size() + unparked_images_.size();
}

DiskDataAllocator& ParkableImageManager::data_allocator() const {
  if (allocator_for_testing_)
    return *allocator_for_testing_;

  return DiskDataAllocator::Instance();
}

void ParkableImageManager::ResetForTesting() {
  MutexLocker lock(lock_);

  has_pending_parking_task_ = false;
  has_posted_accounting_task_ = false;
  unparked_images_.clear();
  on_disk_images_.clear();
  allocator_for_testing_ = nullptr;
  total_disk_read_time_ = base::TimeDelta();
  total_disk_write_time_ = base::TimeDelta();
}

void ParkableImageManager::Add(ParkableImage* image) {
  DCHECK(IsMainThread());

  MutexLocker lock(lock_);

  ScheduleDelayedParkingTaskIfNeeded();

  if (!has_posted_accounting_task_) {
    auto task_runner = Thread::Current()->GetTaskRunner();
    DCHECK(task_runner);
    // |base::Unretained(this)| is fine because |this| is a NoDestructor
    // singleton.
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableImageManager::RecordStatisticsAfter5Minutes,
                       base::Unretained(this)),
        base::TimeDelta::FromMinutes(5));
    has_posted_accounting_task_ = true;
  }

  unparked_images_.insert(image);
}

void ParkableImageManager::RecordStatisticsAfter5Minutes() const {
  DCHECK(IsMainThread());

  MutexLocker lock(lock_);

  Statistics stats = ComputeStatistics();

  base::UmaHistogramCounts100000("Memory.ParkableImage.TotalSize.5min",
                                 stats.total_size / 1024);  // in KiB
  base::UmaHistogramCounts100000("Memory.ParkableImage.OnDiskSize.5min",
                                 stats.on_disk_size / 1024);  // in KiB
  base::UmaHistogramCounts100000("Memory.ParkableImage.UnparkedSize.5min",
                                 stats.unparked_size / 1024);  // in KiB

  // Metrics related to parking only should be recorded if the feature is
  // enabled.
  if (IsParkableImagesToDiskEnabled()) {
    base::UmaHistogramBoolean("Memory.ParkableImage.DiskIsUsable.5min",
                              data_allocator().may_write());
    // These metrics only make sense if the disk allocator is used.
    if (data_allocator().may_write()) {
      base::UmaHistogramTimes("Memory.ParkableImage.TotalWriteTime.5min",
                              total_disk_write_time_);
      base::UmaHistogramTimes("Memory.ParkableImage.TotalReadTime.5min",
                              total_disk_read_time_);
    }
  }
}

void ParkableImageManager::Remove(ParkableImage* image) {
  MutexLocker lock(lock_);

  // Image could be on disk or unparked. Remove it in either case.
  auto* map = image->is_on_disk() ? &on_disk_images_ : &unparked_images_;
  auto it = map->find(image);
  DCHECK(it != map->end());
  map->erase(it);
}

void ParkableImageManager::MoveImage(ParkableImage* image,
                                     WTF::HashSet<ParkableImage*>* from,
                                     WTF::HashSet<ParkableImage*>* to) {
  auto it = from->find(image);
  CHECK(it != from->end());
  CHECK(!to->Contains(image));
  from->erase(it);
  to->insert(image);
}

bool ParkableImageManager::IsRegistered(ParkableImage* image) {
  MutexLocker lock(lock_);

  auto* map = image->is_on_disk() ? &on_disk_images_ : &unparked_images_;
  auto it = map->find(image);

  return it != map->end();
}

void ParkableImageManager::OnWrittenToDisk(ParkableImage* image) {
  MutexLocker lock(lock_);
  MoveImage(image, &unparked_images_, &on_disk_images_);
}

void ParkableImageManager::OnReadFromDisk(ParkableImage* image) {
  MutexLocker lock(lock_);
  MoveImage(image, &on_disk_images_, &unparked_images_);
  ScheduleDelayedParkingTaskIfNeeded();
}

void ParkableImageManager::ScheduleDelayedParkingTaskIfNeeded() {
  if (!ParkableImageManager::IsParkableImagesToDiskEnabled())
    return;

  if (has_pending_parking_task_)
    return;

  auto* thread = Thread::MainThread();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      thread->GetTaskRunner();
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParkableImageManager::MaybeParkImages,
                     base::Unretained(this)),
      ParkableImageManager::kDelayedParkingInterval);
  has_pending_parking_task_ = true;
}

void ParkableImageManager::MaybeParkImages() {
  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());

  MutexLocker lock(lock_);

  // This makes a copy of the pointers stored in |unparked_images_|. We iterate
  // over this copy in |MaybeParkImages|, instead of |unparked_images_|
  // directly, for two reasons:
  // (1) Avoiding a deadlock when we need to park synchronously (i.e. if we have
  // already written to disk and don't need to post a background task), as
  // synchronous parking calls |ParkableImageManager::OnWrittenToDisk()|;
  // (2) Keeping the images alive until we are done iterating, without locking
  // (through use of scoped_refptr instead of a raw pointer).
  WTF::Vector<scoped_refptr<ParkableImage>> unparked_images;
  for (auto* image : unparked_images_)
    unparked_images.push_back(scoped_refptr<ParkableImage>(image));

  // We unlock here so that we can avoid a deadlock, since if the data for the
  // image is already written to disk, we can discard our copy of the data
  // synchronously, which calls back into the manager.
  lock_.unlock();

  bool unfrozen_images = false;
  for (auto image : unparked_images) {
    if (!image->is_frozen())
      unfrozen_images = true;
    image->MaybePark();
  }

  lock_.lock();

  has_pending_parking_task_ = false;

  if (unfrozen_images)
    ScheduleDelayedParkingTaskIfNeeded();
}

}  // namespace blink
