// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

namespace {

void RecordReadStatistics(size_t size, base::TimeDelta duration) {
  size_t throughput_mb_s =
      static_cast<size_t>(size / duration.InSecondsF()) / (1024 * 1024);
  size_t size_kb = size / 1024;  // in KiB

  // Size should be <1MiB in most cases.
  base::UmaHistogramCounts10000("Memory.ParkableImage.Read.Size", size_kb);
  // Size is usually >1KiB, and at most ~10MiB, and throughput ranges from
  // single-digit MB/s to ~1000MiB/s depending on the CPU/disk, hence the
  // ranges.
  base::UmaHistogramCustomMicrosecondsTimes(
      "Memory.ParkableImage.Read.Latency", duration,
      base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
      100);
  base::UmaHistogramCounts1000("Memory.ParkableImage.Read.Throughput",
                               throughput_mb_s);
}

void RecordWriteStatistics(size_t size, base::TimeDelta duration) {
  size_t throughput_mb_s =
      static_cast<size_t>(size / duration.InSecondsF()) / (1024 * 1024);
  size_t size_kb = size / 1024;

  // Size should be <1MiB in most cases.
  base::UmaHistogramCounts10000("Memory.ParkableImage.Write.Size", size_kb);
  // Size is usually >1KiB, and at most ~10MiB, and throughput ranges from
  // single-digit MB/s to ~1000MiB/s depending on the CPU/disk, hence the
  // ranges.
  base::UmaHistogramCustomMicrosecondsTimes(
      "Memory.ParkableImage.Write.Latency", duration,
      base::TimeDelta::FromMicroseconds(500), base::TimeDelta::FromSeconds(1),
      100);
  base::UmaHistogramCounts1000("Memory.ParkableImage.Write.Throughput",
                               throughput_mb_s);
}

void AsanPoisonBuffer(RWBuffer* rw_buffer) {
#if defined(ADDRESS_SANITIZER)
  if (!rw_buffer || !rw_buffer->size())
    return;

  auto ro_buffer = rw_buffer->MakeROBufferSnapshot();
  ROBuffer::Iter iter(ro_buffer);
  do {
    ASAN_POISON_MEMORY_REGION(iter.data(), iter.size());
  } while (iter.Next());
#endif
}

void AsanUnpoisonBuffer(RWBuffer* rw_buffer) {
#if defined(ADDRESS_SANITIZER)
  if (!rw_buffer || !rw_buffer->size())
    return;

  auto ro_buffer = rw_buffer->MakeROBufferSnapshot();
  ROBuffer::Iter iter(ro_buffer);
  do {
    ASAN_UNPOISON_MEMORY_REGION(iter.data(), iter.size());
  } while (iter.Next());
#endif
}

}  // namespace

const base::Feature kUseParkableImageSegmentReader{
    "UseParkableImageSegmentReader", base::FEATURE_DISABLED_BY_DEFAULT};

void ParkableImage::Append(WTF::SharedBuffer* buffer, size_t offset) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MutexLocker lock(lock_);
  DCHECK(!frozen_);
  DCHECK(!is_on_disk());
  DCHECK(rw_buffer_);

  for (auto it = buffer->GetIteratorAt(offset); it != buffer->cend(); ++it) {
    DCHECK_GE(buffer->size(), rw_buffer_->size() + it->size());
    const size_t remaining = buffer->size() - rw_buffer_->size() - it->size();
    rw_buffer_->Append(it->data(), it->size(), remaining);
  }
  size_ = rw_buffer_->size();
}

scoped_refptr<SharedBuffer> ParkableImage::Data() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MutexLocker lock(lock_);
  Unpark();
  DCHECK(rw_buffer_);
  scoped_refptr<ROBuffer> ro_buffer(rw_buffer_->MakeROBufferSnapshot());
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  ROBuffer::Iter it(ro_buffer.get());
  do {
    shared_buffer->Append(static_cast<const char*>(it.data()), it.size());
  } while (it.Next());
  return shared_buffer;
}

scoped_refptr<SegmentReader> ParkableImage::GetSegmentReader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::FeatureList::IsEnabled(kUseParkableImageSegmentReader)) {
    return SegmentReader::CreateFromParkableImage(
        scoped_refptr<ParkableImage>(this));
  } else {
    MutexLocker lock(lock_);
    Unpark();
    DCHECK(rw_buffer_);
    // The locking and unlocking here is only needed to make sure ASAN unpoisons
    // things correctly here.
    Lock();
    scoped_refptr<ROBuffer> ro_buffer(rw_buffer_->MakeROBufferSnapshot());
    scoped_refptr<SegmentReader> segment_reader =
        SegmentReader::CreateFromROBuffer(std::move(ro_buffer));
    Unlock();
    return segment_reader;
  }
}

bool ParkableImage::CanParkNow() const {
  DCHECK(!is_on_disk());
  return is_frozen() && !is_locked() && rw_buffer_->HasNoSnapshots();
}

ParkableImage::ParkableImage(size_t initial_capacity)
    : rw_buffer_(std::make_unique<RWBuffer>(initial_capacity)) {
  ParkableImageManager::Instance().Add(this);
}

ParkableImage::~ParkableImage() {
  DCHECK(!is_locked());
  auto& manager = ParkableImageManager::Instance();
  if (!is_below_min_parking_size() || !is_frozen())
    manager.Remove(this);
  DCHECK(!manager.IsRegistered(this));
  if (on_disk_metadata_)
    manager.data_allocator().Discard(std::move(on_disk_metadata_));
  AsanUnpoisonBuffer(rw_buffer_.get());
}

// static
scoped_refptr<ParkableImage> ParkableImage::Create(size_t initial_capacity) {
  return base::MakeRefCounted<ParkableImage>(initial_capacity);
}

scoped_refptr<SegmentReader> ParkableImage::MakeROSnapshot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetSegmentReader();
}

void ParkableImage::Freeze() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MutexLocker lock(lock_);
  DCHECK(!frozen_);
  frozen_ = true;

  if (is_below_min_parking_size()) {
    ParkableImageManager::Instance().Remove(this);
    return;
  }

  // If we don't have any snapshots of the current data, that means it could be
  // parked at any time.
  //
  // If we have snapshots, we don't want to poison the buffer, because the
  // snapshot is allowed to access the buffer's data freely.
  if (CanParkNow())
    AsanPoisonBuffer(rw_buffer_.get());
}

void ParkableImage::Lock() {
  // Calling |Lock| only makes sense if the data is available.
  DCHECK(rw_buffer_);

  lock_depth_++;

  AsanUnpoisonBuffer(rw_buffer_.get());
}

void ParkableImage::Unlock() {
  // Check that we've locked it already.
  DCHECK_GT(lock_depth_, 0u);
  // While locked, we can never write the data to disk.
  DCHECK(!is_on_disk());

  lock_depth_--;

  // We only poison the buffer if we're able to park after unlocking.
  // This is to avoid issues when creating a ROBufferSegmentReader from the
  // ParkableImage.
  if (CanParkNow())
    AsanPoisonBuffer(rw_buffer_.get());
}

// static
void ParkableImage::WriteToDiskInBackground(
    scoped_refptr<ParkableImage> parkable_image,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner) {
  DCHECK(!IsMainThread());
  MutexLocker lock(parkable_image->lock_);

  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());
  DCHECK(parkable_image);
  DCHECK(!parkable_image->on_disk_metadata_);

  AsanUnpoisonBuffer(parkable_image->rw_buffer_.get());

  scoped_refptr<ROBuffer> ro_buffer =
      parkable_image->rw_buffer_->MakeROBufferSnapshot();
  ROBuffer::Iter it(ro_buffer.get());

  Vector<char> vector;
  vector.ReserveInitialCapacity(parkable_image->size());

  do {
    vector.Append(reinterpret_cast<const char*>(it.data()), it.size());
  } while (it.Next());

  // Release the lock while writing, so we don't block for too long.
  parkable_image->lock_.unlock();

  base::ElapsedTimer timer;
  auto metadata = ParkableImageManager::Instance().data_allocator().Write(
      vector.data(), vector.size());
  base::TimeDelta elapsed = timer.Elapsed();

  // Acquire the lock again after writing.
  parkable_image->lock_.lock();

  parkable_image->on_disk_metadata_ = std::move(metadata);

  // Nothing to do if the write failed except return. Notably, we need to
  // keep around the data for the ParkableImage in this case.
  if (!parkable_image->on_disk_metadata_) {
    parkable_image->background_task_in_progress_ = false;
  } else {
    RecordWriteStatistics(parkable_image->on_disk_metadata_->size(), elapsed);
    ParkableImageManager::Instance().RecordDiskWriteTime(elapsed);
    PostCrossThreadTask(*callback_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&ParkableImage::MaybeDiscardData,
                                            std::move(parkable_image)));
  }
}

void ParkableImage::MaybeDiscardData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_below_min_parking_size());

  MutexLocker lock(lock_);
  DCHECK(on_disk_metadata_);

  background_task_in_progress_ = false;

  // If the image is now unparkable, we need to keep the data around.
  // This can happen if, for example, in between the time we posted the task to
  // discard the data and the time MaybeDiscardData is called, we've created a
  // SegmentReader from |rw_buffer_|, since discarding the data would leave us
  // with a dangling pointer in the SegmentReader.
  if (CanParkNow())
    DiscardData();
}

void ParkableImage::DiscardData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_locked());
  AsanUnpoisonBuffer(rw_buffer_.get());

  rw_buffer_ = nullptr;
  ParkableImageManager::Instance().OnWrittenToDisk(this);
}

bool ParkableImage::MaybePark() {
  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());

  MutexLocker lock(lock_);

  if (background_task_in_progress_)
    return true;

  if (!CanParkNow())
    return false;

  if (on_disk_metadata_) {
    DiscardData();
    return true;
  }

  background_task_in_progress_ = true;

  // The writing is done on a background thread. We pass a TaskRunner from the
  // current thread for when we have finished writing.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&ParkableImage::WriteToDiskInBackground,
                          scoped_refptr<ParkableImage>(this),
                          Thread::Current()->GetTaskRunner()));
  return true;
}

void ParkableImage::Unpark() {
  if (!is_on_disk()) {
    AsanUnpoisonBuffer(rw_buffer_.get());
    return;
  }

  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());

  TRACE_EVENT1("blink", "ParkableImage::Unpark", "size", size());

  DCHECK(on_disk_metadata_);
  WTF::Vector<uint8_t> vector(size());

  base::ElapsedTimer timer;
  ParkableImageManager::Instance().data_allocator().Read(*on_disk_metadata_,
                                                         vector.data());
  base::TimeDelta elapsed = timer.Elapsed();

  RecordReadStatistics(on_disk_metadata_->size(), elapsed);
  ParkableImageManager::Instance().RecordDiskReadTime(elapsed);

  ParkableImageManager::Instance().OnReadFromDisk(this);

  DCHECK(!rw_buffer_);

  rw_buffer_ = std::make_unique<RWBuffer>(size());
  rw_buffer_->Append(vector.data(), size());
}

size_t ParkableImage::size() const {
  return size_;
}

bool ParkableImage::is_below_min_parking_size() const {
  return size() < ParkableImage::kMinSizeToPark;
}

bool ParkableImage::is_locked() const {
  return lock_depth_ != 0;
}

}  // namespace blink
