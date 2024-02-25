// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

BASE_FEATURE(kDelayParkingImages,
             "DelayParkingImages",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

void RecordReadStatistics(size_t size,
                          base::TimeDelta duration,
                          base::TimeDelta time_since_freeze) {
  int throughput_mb_s = duration.is_zero()
                            ? INT_MAX
                            : base::saturated_cast<int>(
                                  size / duration.InSecondsF() / (1024 * 1024));

  // Size is usually >1KiB, and at most ~10MiB, and throughput ranges from
  // single-digit MB/s to ~1000MiB/s depending on the CPU/disk, hence the
  // ranges.
  base::UmaHistogramCustomMicrosecondsTimes("Memory.ParkableImage.Read.Latency",
                                            duration, base::Microseconds(500),
                                            base::Seconds(1), 100);
  base::UmaHistogramCounts1000("Memory.ParkableImage.Read.Throughput",
                               throughput_mb_s);
}

void RecordWriteStatistics(size_t size, base::TimeDelta duration) {
  int size_kb = static_cast<int>(size / 1024);  // in KiB

  // Size should be <1MiB in most cases.
  base::UmaHistogramCounts10000("Memory.ParkableImage.Write.Size", size_kb);
  // Size is usually >1KiB, and at most ~10MiB, and throughput ranges from
  // single-digit MB/s to ~1000MiB/s depending on the CPU/disk, hence the
  // ranges.
  base::UmaHistogramCustomMicrosecondsTimes(
      "Memory.ParkableImage.Write.Latency", duration, base::Microseconds(500),
      base::Seconds(1), 100);
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

// This should be used to make sure that the last reference to the |this| is
// decremented on the main thread (since that's where the destructor must
// run), for example by posting a task with this to the main thread.
void NotifyWriteToDiskFinished(scoped_refptr<ParkableImageImpl>) {
  DCHECK(IsMainThread());
}

}  // namespace

// ParkableImageSegmentReader

class ParkableImageSegmentReader : public SegmentReader {
 public:
  explicit ParkableImageSegmentReader(scoped_refptr<ParkableImage> image);
  size_t size() const override;
  size_t GetSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> GetAsSkData() const override;
  void LockData() override;
  void UnlockData() override;

 private:
  ~ParkableImageSegmentReader() override = default;
  scoped_refptr<ParkableImage> parkable_image_;
  size_t available_;
};

ParkableImageSegmentReader::ParkableImageSegmentReader(
    scoped_refptr<ParkableImage> image)
    : parkable_image_(std::move(image)), available_(parkable_image_->size()) {}

size_t ParkableImageSegmentReader::size() const {
  return available_;
}

size_t ParkableImageSegmentReader::GetSomeData(const char*& data,
                                               size_t position) const {
  if (!parkable_image_) {
    return 0;
  }

  base::AutoLock lock(parkable_image_->impl_->lock_);
  DCHECK(parkable_image_->impl_->is_locked());

  RWBuffer::ROIter iter(parkable_image_->impl_->rw_buffer_.get(), available_);
  size_t position_of_block = 0;

  return RWBufferGetSomeData(iter, position_of_block, data, position);
}

sk_sp<SkData> ParkableImageSegmentReader::GetAsSkData() const {
  if (!parkable_image_) {
    return nullptr;
  }

  base::AutoLock lock(parkable_image_->impl_->lock_);
  parkable_image_->impl_->Unpark();

  RWBuffer::ROIter iter(parkable_image_->impl_->rw_buffer_.get(), available_);

  if (!iter.HasNext()) {  // No need to copy because the data is contiguous.
    // We lock here so that we don't get a use-after-free. ParkableImage can
    // not be parked while it is locked, so the buffer is valid for the whole
    // lifetime of the SkData. We add the ref so that the ParkableImage has a
    // longer limetime than the SkData.
    parkable_image_->AddRef();
    parkable_image_->LockData();
    return SkData::MakeWithProc(
        iter.data(), available_,
        [](const void* ptr, void* context) -> void {
          auto* parkable_image = static_cast<ParkableImage*>(context);
          {
            base::AutoLock lock(parkable_image->impl_->lock_);
            parkable_image->UnlockData();
          }
          // Don't hold the mutex while we call |Release|, since |Release| can
          // free the ParkableImage, if this is the last reference to it;
          // Freeing the ParkableImage while the mutex is held causes a UAF when
          // the dtor for base::AutoLock is called.
          parkable_image->Release();
        },
        parkable_image_.get());
  }

  // Data is not contiguous so we need to copy.
  return RWBufferCopyAsSkData(iter, available_);
}

void ParkableImageSegmentReader::LockData() {
  base::AutoLock lock(parkable_image_->impl_->lock_);
  parkable_image_->impl_->Unpark();

  parkable_image_->LockData();
}

void ParkableImageSegmentReader::UnlockData() {
  base::AutoLock lock(parkable_image_->impl_->lock_);

  parkable_image_->UnlockData();
}

BASE_FEATURE(kUseParkableImageSegmentReader,
             "UseParkableImageSegmentReader",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::TimeDelta ParkableImageImpl::kParkingDelay;

void ParkableImageImpl::Append(WTF::SharedBuffer* buffer, size_t offset) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(lock_);
  DCHECK(!is_frozen());
  DCHECK(!is_on_disk());
  DCHECK(rw_buffer_);

  for (auto it = buffer->GetIteratorAt(offset); it != buffer->cend(); ++it) {
    DCHECK_GE(buffer->size(), rw_buffer_->size() + it->size());
    const size_t remaining = buffer->size() - rw_buffer_->size() - it->size();
    rw_buffer_->Append(it->data(), it->size(), remaining);
  }
  size_ = rw_buffer_->size();
}

scoped_refptr<SharedBuffer> ParkableImageImpl::Data() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(lock_);
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

scoped_refptr<SegmentReader> ParkableImageImpl::GetROBufferSegmentReader() {
  base::AutoLock lock(lock_);
  Unpark();
  DCHECK(rw_buffer_);
  // The locking and unlocking here is only needed to make sure ASAN unpoisons
  // things correctly here.
  LockData();
  scoped_refptr<ROBuffer> ro_buffer(rw_buffer_->MakeROBufferSnapshot());
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromROBuffer(std::move(ro_buffer));
  UnlockData();
  return segment_reader;
}

bool ParkableImageImpl::CanParkNow() const {
  DCHECK(!is_on_disk());
  return !TransientlyUnableToPark() && !is_locked() &&
         rw_buffer_->HasNoSnapshots();
}

ParkableImageImpl::ParkableImageImpl(size_t initial_capacity)
    : rw_buffer_(std::make_unique<RWBuffer>(initial_capacity)) {}

ParkableImageImpl::~ParkableImageImpl() {
  DCHECK(IsMainThread());
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
scoped_refptr<ParkableImageImpl> ParkableImageImpl::Create(
    size_t initial_capacity) {
  return base::MakeRefCounted<ParkableImageImpl>(initial_capacity);
}

void ParkableImageImpl::Freeze() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(lock_);
  DCHECK(!is_frozen());
  frozen_time_ = base::TimeTicks::Now();

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

void ParkableImageImpl::LockData() {
  // Calling |Lock| only makes sense if the data is available.
  DCHECK(rw_buffer_);

  lock_depth_++;

  AsanUnpoisonBuffer(rw_buffer_.get());
}

void ParkableImageImpl::UnlockData() {
  // Check that we've locked it already.
  DCHECK_GT(lock_depth_, 0u);
  // While locked, we can never write the data to disk.
  DCHECK(!is_on_disk());

  lock_depth_--;

  // We only poison the buffer if we're able to park after unlocking.
  // This is to avoid issues when creating a ROBufferSegmentReader from the
  // ParkableImageImpl.
  if (CanParkNow())
    AsanPoisonBuffer(rw_buffer_.get());
}

// static
void ParkableImageImpl::WriteToDiskInBackground(
    scoped_refptr<ParkableImageImpl> parkable_image,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner) {
  DCHECK(!IsMainThread());
  base::AutoLock lock(parkable_image->lock_);

  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());
  DCHECK(parkable_image);
  DCHECK(parkable_image->reserved_chunk_);
  DCHECK(!parkable_image->on_disk_metadata_);

  AsanUnpoisonBuffer(parkable_image->rw_buffer_.get());

  scoped_refptr<ROBuffer> ro_buffer =
      parkable_image->rw_buffer_->MakeROBufferSnapshot();
  ROBuffer::Iter it(ro_buffer.get());

  Vector<char> vector;
  vector.ReserveInitialCapacity(
      base::checked_cast<wtf_size_t>(parkable_image->size()));

  do {
    vector.Append(reinterpret_cast<const char*>(it.data()),
                  base::checked_cast<wtf_size_t>(it.size()));
  } while (it.Next());

  auto reserved_chunk = std::move(parkable_image->reserved_chunk_);

  // Release the lock while writing, so we don't block for too long.
  parkable_image->lock_.Release();

  base::ElapsedTimer timer;
  auto metadata = ParkableImageManager::Instance().data_allocator().Write(
      std::move(reserved_chunk), vector.data());
  base::TimeDelta elapsed = timer.Elapsed();

  // Acquire the lock again after writing.
  parkable_image->lock_.Acquire();

  parkable_image->on_disk_metadata_ = std::move(metadata);

  // Nothing to do if the write failed except return. Notably, we need to
  // keep around the data for the ParkableImageImpl in this case.
  if (!parkable_image->on_disk_metadata_) {
    parkable_image->background_task_in_progress_ = false;
    // This ensures that we don't destroy |this| on the background thread at
    // the end of this function, if we happen to have the last reference to
    // |this|.
    //
    // We cannot simply check the reference count here, since it may be
    // changed racily on another thread, so posting a task is the only safe
    // way to proceed.
    PostCrossThreadTask(*callback_task_runner, FROM_HERE,
                        CrossThreadBindOnce(&NotifyWriteToDiskFinished,
                                            std::move(parkable_image)));
  } else {
    RecordWriteStatistics(parkable_image->on_disk_metadata_->size(), elapsed);
    ParkableImageManager::Instance().RecordDiskWriteTime(elapsed);
    PostCrossThreadTask(
        *callback_task_runner, FROM_HERE,
        CrossThreadBindOnce(&ParkableImageImpl::MaybeDiscardData,
                            std::move(parkable_image)));
  }
}

void ParkableImageImpl::MaybeDiscardData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_below_min_parking_size());

  base::AutoLock lock(lock_);
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

void ParkableImageImpl::DiscardData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_locked());
  AsanUnpoisonBuffer(rw_buffer_.get());

  rw_buffer_ = nullptr;
  ParkableImageManager::Instance().OnWrittenToDisk(this);
}

bool ParkableImageImpl::TransientlyUnableToPark() const {
  if (base::FeatureList::IsEnabled(kDelayParkingImages)) {
    // Most images are used only once, for the initial decode at render time.
    // Since rendering can happen multiple seconds after the image load (e.g.
    // if paint by a synchronous <script> earlier in the document), we instead
    // wait up to kParkingDelay before parking an unused image.
    return !is_frozen() ||
           (base::TimeTicks::Now() - frozen_time_ <= kParkingDelay && !used_);
  } else {
    return !is_frozen();
  }
}

bool ParkableImageImpl::MaybePark(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());
  DCHECK(IsMainThread());

  base::AutoLock lock(lock_);

  if (background_task_in_progress_)
    return true;

  if (!CanParkNow())
    return false;

  if (on_disk_metadata_) {
    DiscardData();
    return true;
  }

  auto reserved_chunk =
      ParkableImageManager::Instance().data_allocator().TryReserveChunk(size());
  if (!reserved_chunk) {
    return false;
  }
  reserved_chunk_ = std::move(reserved_chunk);

  background_task_in_progress_ = true;

  // The writing is done on a background thread. We pass a TaskRunner from the
  // current thread for when we have finished writing.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&ParkableImageImpl::WriteToDiskInBackground,
                          scoped_refptr<ParkableImageImpl>(this),
                          std::move(task_runner)));
  return true;
}

// static
size_t ParkableImageImpl::ReadFromDiskIntoBuffer(
    DiskDataMetadata* on_disk_metadata,
    void* buffer,
    size_t capacity) {
  size_t size = on_disk_metadata->size();
  DCHECK(size <= capacity);
  ParkableImageManager::Instance().data_allocator().Read(*on_disk_metadata,
                                                         buffer);
  return size;
}

void ParkableImageImpl::Unpark() {
  // We mark the ParkableImage as having been read here, since any access to
  // its data must first make sure it's not on disk.
  used_ = true;

  if (!is_on_disk()) {
    AsanUnpoisonBuffer(rw_buffer_.get());
    return;
  }

  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());

  TRACE_EVENT1("blink", "ParkableImageImpl::Unpark", "size", size());

  DCHECK(on_disk_metadata_);

  base::ElapsedTimer timer;

  DCHECK(!rw_buffer_);
  rw_buffer_ = std::make_unique<RWBuffer>(
      base::BindOnce(&ParkableImageImpl::ReadFromDiskIntoBuffer,
                     base::Unretained(on_disk_metadata_.get())),
      size());

  base::TimeDelta elapsed = timer.Elapsed();
  base::TimeDelta time_since_freeze = base::TimeTicks::Now() - frozen_time_;

  RecordReadStatistics(on_disk_metadata_->size(), elapsed, time_since_freeze);

  ParkableImageManager::Instance().RecordDiskReadTime(elapsed);
  ParkableImageManager::Instance().OnReadFromDisk(this);

  DCHECK(rw_buffer_);
}

size_t ParkableImageImpl::size() const {
  return size_;
}

bool ParkableImageImpl::is_below_min_parking_size() const {
  return size() < ParkableImageImpl::kMinSizeToPark;
}

bool ParkableImageImpl::is_locked() const {
  return lock_depth_ != 0;
}

ParkableImage::ParkableImage(size_t offset)
    : impl_(ParkableImageManager::Instance().CreateParkableImage(offset)) {
  ParkableImageManager::Instance().Add(impl_.get());
}

ParkableImage::~ParkableImage() {
  ParkableImageManager::Instance().DestroyParkableImage(std::move(impl_));
}

// static
scoped_refptr<ParkableImage> ParkableImage::Create(size_t initial_capacity) {
  return base::MakeRefCounted<ParkableImage>(initial_capacity);
}

size_t ParkableImage::size() const {
  DCHECK(impl_);
  return impl_->size();
}

bool ParkableImage::is_on_disk() const {
  DCHECK(impl_);
  return impl_->is_on_disk();
}

scoped_refptr<SegmentReader> ParkableImage::MakeROSnapshot() {
  DCHECK(impl_);
  DCHECK_CALLED_ON_VALID_THREAD(impl_->thread_checker_);

  if (base::FeatureList::IsEnabled(kUseParkableImageSegmentReader)) {
    return CreateSegmentReader();
  } else {
    return impl_->GetROBufferSegmentReader();
  }
}

void ParkableImage::Freeze() {
  DCHECK(impl_);
  impl_->Freeze();
}

scoped_refptr<SharedBuffer> ParkableImage::Data() {
  DCHECK(impl_);
  return impl_->Data();
}

void ParkableImage::Append(WTF::SharedBuffer* buffer, size_t offset) {
  DCHECK(impl_);
  impl_->Append(buffer, offset);
}

void ParkableImage::LockData() {
  DCHECK(impl_);
  impl_->LockData();
}

void ParkableImage::UnlockData() {
  DCHECK(impl_);
  impl_->UnlockData();
}

scoped_refptr<SegmentReader> ParkableImage::CreateSegmentReader() {
  return base::MakeRefCounted<ParkableImageSegmentReader>(this);
}

}  // namespace blink
