// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_
#define STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_storage_constants.h"

namespace base {
class TaskRunner;
}

namespace content {
class ChromeBlobStorageContext;
}

namespace storage {
class ShareableBlobDataItem;
class ShareableFileReference;

// This class's main responsibility is deciding how blob data gets stored.
// This encompasses:
// * Keeping track of memory & file quota,
// * How to transport the blob data from the renderer (DetermineStrategy),
// * Allocating memory & file quota (ReserveMemoryQuota, ReserveFileQuota)
// * Paging memory quota to disk when we're nearing our memory limit, and
// * Maintaining an LRU of memory items to choose candidates to page to disk
//   (NotifyMemoryItemsUsed).
// This class can only be interacted with on the IO thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobMemoryController {
 public:
  enum class Strategy {
    // We don't have enough memory for this blob.
    TOO_LARGE,
    // There isn't any memory that needs transporting.
    NONE_NEEDED,
    // Transportation strategies.
    IPC,
    SHARED_MEMORY,
    FILE
  };

  struct COMPONENT_EXPORT(STORAGE_BROWSER) FileCreationInfo {
    FileCreationInfo();
    ~FileCreationInfo();
    FileCreationInfo(FileCreationInfo&& other);
    FileCreationInfo& operator=(FileCreationInfo&&);

    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::File file;
    scoped_refptr<base::TaskRunner> file_deletion_runner;
    base::FilePath path;
    scoped_refptr<ShareableFileReference> file_reference;
    base::Time last_modified;
  };

  struct MemoryAllocation {
    MemoryAllocation(base::WeakPtr<BlobMemoryController> controller,
                     uint64_t item_id,
                     size_t length);

    MemoryAllocation(const MemoryAllocation&) = delete;
    MemoryAllocation& operator=(const MemoryAllocation&) = delete;

    ~MemoryAllocation();

    size_t length() const { return length_; }

   private:
    friend class BlobMemoryController;

    base::WeakPtr<BlobMemoryController> controller_;
    uint64_t item_id_;
    size_t length_;
  };

  class QuotaAllocationTask {
   public:
    // Operation is cancelled and the callback will NOT be called. This object
    // will be destroyed by this call.
    virtual void Cancel() = 0;

   protected:
    virtual ~QuotaAllocationTask();
  };

  // The bool argument is true if we successfully received memory quota.
  using MemoryQuotaRequestCallback = base::OnceCallback<void(bool)>;
  // The bool argument is true if we successfully received file quota, and the
  // vector argument provides the file info.
  using FileQuotaRequestCallback =
      base::OnceCallback<void(std::vector<FileCreationInfo>, bool)>;

  // We enable file paging if |file_runner| isn't a nullptr.
  BlobMemoryController(const base::FilePath& storage_directory,
                       scoped_refptr<base::TaskRunner> file_runner);

  BlobMemoryController(const BlobMemoryController&) = delete;
  BlobMemoryController& operator=(const BlobMemoryController&) = delete;

  ~BlobMemoryController();

  // Disables file paging. This cancels all pending file creations and paging
  // operations. Reason is recorded in UMA.
  void DisableFilePaging(base::File::Error reason);

  bool file_paging_enabled() const { return file_paging_enabled_; }

  // Returns the strategy the transportation layer should use to transport the
  // given memory. |preemptive_transported_bytes| are the number of transport
  // bytes that are already populated for us, so we don't haved to request them
  // from the renderer.
  Strategy DetermineStrategy(size_t preemptive_transported_bytes,
                             uint64_t total_transportation_bytes) const;

  // Checks to see if we can reserve quota (disk or memory) for the given size.
  bool CanReserveQuota(uint64_t size) const;

  // Reserves quota for the given |unreserved_memory_items|. The items must be
  // bytes items in QUOTA_NEEDED state which we change to QUOTA_REQUESTED.
  // After we reserve memory quota we change their state to QUOTA_GRANTED, set
  // the 'memory_allocation' on the item, and  call |done_callback|. This can
  // happen synchronously.
  // Returns a task handle if the request is asynchronous for cancellation.
  // NOTE: We don't inspect quota limits and assume the user checked
  //       CanReserveQuota before calling this.
  base::WeakPtr<QuotaAllocationTask> ReserveMemoryQuota(
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
      MemoryQuotaRequestCallback done_callback);

  // Reserves quota for the given |unreserved_file_items|. The items must be
  // temporary file items (BlobDataBuilder::IsTemporaryFileItem returns true) in
  // QUOTA_NEEDED state, which we change to QUOTA_REQUESTED. After we reserve
  // file quota we change their state to QUOTA_GRANTED and call
  // |done_callback|.
  // Returns a task handle for cancellation.
  // NOTE: We don't inspect quota limits and assume the user checked
  //       CanReserveQuota before calling this.
  base::WeakPtr<QuotaAllocationTask> ReserveFileQuota(
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
      FileQuotaRequestCallback done_callback);

  // Called when initially populated or upon later access.
  void NotifyMemoryItemsUsed(
      const std::vector<scoped_refptr<ShareableBlobDataItem>>& items);

  size_t memory_usage() const { return blob_memory_used_; }
  uint64_t disk_usage() const { return disk_used_; }

  base::WeakPtr<BlobMemoryController> GetWeakPtr();

  const BlobStorageLimits& limits() const { return limits_; }
  void set_limits_for_testing(const BlobStorageLimits& limits) {
    OnStorageLimitsCalculated(limits);
    manual_limits_set_ = true;
  }

  void ShrinkMemoryAllocation(ShareableBlobDataItem* item);
  void ShrinkFileAllocation(ShareableFileReference* file_reference,
                            uint64_t old_length,
                            uint64_t new_length);
  void GrowFileAllocation(ShareableFileReference* file_reference,
                          uint64_t delta);

  using DiskSpaceFuncPtr = int64_t (*)(const base::FilePath&);

  void set_testing_disk_space(DiskSpaceFuncPtr disk_space_function) {
    disk_space_function_ = disk_space_function;
  }

  size_t GetAvailableMemoryForBlobs() const;
  uint64_t GetAvailableFileSpaceForBlobs() const;

  // The given callback will be called when we've finished calculating blob
  // storage limits. Usually limits are calculated at some point after startup,
  // but calling this method may cause them to be calculated sooner.
  // If limits have already been calculated |callback| will be called
  // synchronously.
  void CallWhenStorageLimitsAreKnown(base::OnceClosure callback);

  void set_amount_of_physical_memory_for_testing(uint64_t amount_of_memory) {
    amount_of_memory_for_testing_ = amount_of_memory;
  }

 private:
  class FileQuotaAllocationTask;
  class MemoryQuotaAllocationTask;

  FRIEND_TEST_ALL_PREFIXES(BlobMemoryControllerTest, OnMemoryPressure);
  // So this (and only this) class can call CalculateBlobStorageLimits().
  friend class content::ChromeBlobStorageContext;

  // Schedules a task on the file runner to calculate blob storage quota limits.
  // This should only be called once per storage partition initialization as we
  // emit UMA stats with that expectation.
  void CalculateBlobStorageLimits();

  using PendingMemoryQuotaTaskList =
      std::list<std::unique_ptr<MemoryQuotaAllocationTask>>;
  using PendingFileQuotaTaskList =
      std::list<std::unique_ptr<FileQuotaAllocationTask>>;

  void OnStorageLimitsCalculated(BlobStorageLimits limits);

  // Adjusts the effective disk usage based on the available space. We try to
  // keep at least BlobSorageLimits::min_available_disk_space() free.
  void AdjustDiskUsage(uint64_t avail_disk_space);

  base::WeakPtr<QuotaAllocationTask> AppendMemoryTask(
      uint64_t total_bytes_needed,
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
      MemoryQuotaRequestCallback done_callback);

  void MaybeGrantPendingMemoryRequests();

  size_t CollectItemsForEviction(
      std::vector<scoped_refptr<ShareableBlobDataItem>>* output,
      uint64_t min_page_file_size);

  // Schedule paging until our memory usage is below our memory limit.
  void MaybeScheduleEvictionUntilSystemHealthy(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Called when we've completed evicting a list of items to disk. This is where
  // we swap the bytes items for file items, and update our bookkeeping.
  void OnEvictionComplete(
      scoped_refptr<ShareableFileReference> file_reference,
      std::vector<scoped_refptr<ShareableBlobDataItem>> items,
      size_t total_items_size,
      std::pair<FileCreationInfo, int64_t /* avail_disk */> result);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void GrantMemoryAllocations(
      std::vector<scoped_refptr<ShareableBlobDataItem>>* items,
      size_t total_bytes);
  void RevokeMemoryAllocation(uint64_t item_id, size_t length);

  // This is registered as a callback for file deletions on the file reference
  // of our paging files. We decrement the disk space used.
  void OnBlobFileDelete(uint64_t size, const base::FilePath& path);
  void OnShrunkenBlobFileDelete(uint64_t shrink_delta,
                                const base::FilePath& path);

  base::FilePath GenerateNextPageFileName();

  // This records diagnostic counters of our memory quotas. Called when usage
  // changes.
  void RecordTracingCounters() const;

  // Store that we set manual limits so we don't accidentally override them with
  // our configuration task.
  bool manual_limits_set_ = false;
  BlobStorageLimits limits_;
  bool did_schedule_limit_calculation_ = false;
  bool did_calculate_storage_limits_ = false;
  std::vector<base::OnceClosure> on_calculate_limits_callbacks_;

  std::optional<uint64_t> amount_of_memory_for_testing_;

  // Memory bookkeeping. These numbers are all disjoint.
  // This is the amount of memory we're using for blobs in RAM, including the
  // in_flight_memory_used_.
  size_t blob_memory_used_ = 0;
  // This is memory we're temporarily using while we try to write blob items to
  // disk.
  size_t in_flight_memory_used_ = 0;
  // This is the amount of memory we're using on disk.
  uint64_t disk_used_ = 0;

  // State for GenerateNextPageFileName.
  uint64_t current_file_num_ = 0;

  size_t pending_memory_quota_total_size_ = 0;
  PendingMemoryQuotaTaskList pending_memory_quota_tasks_;
  PendingFileQuotaTaskList pending_file_quota_tasks_;

  int pending_evictions_ = 0;

  bool file_paging_enabled_ = false;
  base::FilePath blob_storage_dir_;
  scoped_refptr<base::TaskRunner> file_runner_;
  // This defaults to calling base::SysInfo::AmountOfFreeDiskSpace.
  DiskSpaceFuncPtr disk_space_function_;
  base::TimeTicks last_eviction_time_;

  // Lifetime of the ShareableBlobDataItem objects is handled externally in the
  // BlobStorageContext class.
  base::LRUCache<uint64_t, ShareableBlobDataItem*> populated_memory_items_;
  size_t populated_memory_items_bytes_ = 0;
  // We need to keep track of items currently being paged to disk so that if
  // another blob successfully grabs a ref, we can prevent it from adding the
  // item to the recent_item_cache_ above.
  std::unordered_set<uint64_t> items_paging_to_file_;

  base::MemoryPressureListener memory_pressure_listener_;

  base::WeakPtrFactory<BlobMemoryController> weak_factory_{this};
};
}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_
