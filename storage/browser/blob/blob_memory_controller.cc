// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_memory_controller.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/small_map.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/browser/blob/shareable_file_reference.h"

using base::File;
using base::FilePath;

namespace storage {
namespace {
constexpr int64_t kUnknownDiskAvailability = -1ll;
constexpr uint64_t kMegabyte = 1024ull * 1024;
const int64_t kMinSecondsForPressureEvictions = 30;

using FileCreationInfo = BlobMemoryController::FileCreationInfo;
using MemoryAllocation = BlobMemoryController::MemoryAllocation;
using QuotaAllocationTask = BlobMemoryController::QuotaAllocationTask;
using DiskSpaceFuncPtr = BlobMemoryController::DiskSpaceFuncPtr;

File::Error CreateBlobDirectory(const FilePath& blob_storage_dir) {
  File::Error error = File::FILE_OK;
  base::CreateDirectoryAndGetError(blob_storage_dir, &error);
  UMA_HISTOGRAM_ENUMERATION("Storage.Blob.CreateDirectoryResult", -error,
                            -File::FILE_ERROR_MAX);
  DLOG_IF(ERROR, error != File::FILE_OK)
      << "Error creating blob storage directory: " << error;
  return error;
}

// CrOS:
// * Ram -  20%
// * Disk - 50%
//   Note: The disk is the user partition, so the operating system can still
//   function if this is full.
// Android:
// * RAM -  1%
// * Disk -  6%
// Desktop:
// * Ram -  20%, or 2 GB if x64.
// * Disk - 10%
BlobStorageLimits CalculateBlobStorageLimitsImpl(
    const FilePath& storage_dir,
    bool disk_enabled,
    base::Optional<int64_t> optional_memory_size_for_testing) {
  int64_t disk_size = 0ull;
  int64_t memory_size = optional_memory_size_for_testing
                            ? optional_memory_size_for_testing.value()
                            : base::SysInfo::AmountOfPhysicalMemory();
  if (disk_enabled && CreateBlobDirectory(storage_dir) == base::File::FILE_OK)
    disk_size = base::SysInfo::AmountOfTotalDiskSpace(storage_dir);

  BlobStorageLimits limits;

  // Don't do specialty configuration for error size (-1).
  if (memory_size > 0) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && defined(ARCH_CPU_64_BITS)
    constexpr size_t kTwoGigabytes = 2ull * 1024 * 1024 * 1024;
    limits.max_blob_in_memory_space = kTwoGigabytes;
#elif defined(OS_ANDROID)
    limits.max_blob_in_memory_space = static_cast<size_t>(memory_size / 100ll);
#else
    limits.max_blob_in_memory_space = static_cast<size_t>(memory_size / 5ll);
#endif
  }
  // Devices just on the edge (RAM == 256MB) should not fail because
  // max_blob_in_memory_space turns out smaller than min_page_file_size
  // causing the CHECK below to fail.
  if (limits.max_blob_in_memory_space < limits.min_page_file_size)
    limits.max_blob_in_memory_space = limits.min_page_file_size;

  // Don't do specialty configuration for error size (-1). Allow no disk.
  if (disk_size >= 0) {
#if defined(OS_CHROMEOS)
    limits.desired_max_disk_space = static_cast<uint64_t>(disk_size / 2ll);
#elif defined(OS_ANDROID)
    limits.desired_max_disk_space = static_cast<uint64_t>(3ll * disk_size / 50);
#else
    limits.desired_max_disk_space = static_cast<uint64_t>(disk_size / 10ll);
#endif
  }
  if (disk_enabled) {
    UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.MaxDiskSpace2",
                            limits.desired_max_disk_space / kMegabyte);
  }
  limits.effective_max_disk_space = limits.desired_max_disk_space;

  CHECK(limits.IsValid());

  return limits;
}

void DestructFile(File infos_without_references) {}

void DeleteFiles(std::vector<FileCreationInfo> files) {
  for (FileCreationInfo& file_info : files) {
    file_info.file.Close();
    base::DeleteFile(file_info.path, false);
  }
}

struct EmptyFilesResult {
  EmptyFilesResult() = default;
  EmptyFilesResult(std::vector<FileCreationInfo> files,
                   File::Error file_error,
                   int64_t disk_availability)
      : files(std::move(files)),
        file_error(file_error),
        disk_availability(disk_availability) {}
  ~EmptyFilesResult() = default;
  EmptyFilesResult(EmptyFilesResult&& o) = default;
  EmptyFilesResult& operator=(EmptyFilesResult&& other) = default;

  std::vector<FileCreationInfo> files;
  File::Error file_error = File::FILE_ERROR_FAILED;
  int64_t disk_availability = 0;
};

// Used for new unpopulated file items. Caller must populate file reference in
// returned FileCreationInfos. Also returns the currently available disk space
// (without the future size of these files).
EmptyFilesResult CreateEmptyFiles(
    const FilePath& blob_storage_dir,
    DiskSpaceFuncPtr disk_space_function,
    scoped_refptr<base::TaskRunner> file_task_runner,
    std::vector<base::FilePath> file_paths) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  File::Error dir_create_status = CreateBlobDirectory(blob_storage_dir);
  if (dir_create_status != File::FILE_OK) {
    return EmptyFilesResult(std::vector<FileCreationInfo>(), dir_create_status,
                            kUnknownDiskAvailability);
  }

  int64_t free_disk_space = disk_space_function(blob_storage_dir);

  std::vector<FileCreationInfo> result;
  for (const base::FilePath& file_path : file_paths) {
    FileCreationInfo creation_info;
    // Try to open our file.
    File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
    creation_info.path = std::move(file_path);
    creation_info.file_deletion_runner = file_task_runner;
    creation_info.error = file.error_details();
    if (creation_info.error != File::FILE_OK) {
      return EmptyFilesResult(std::vector<FileCreationInfo>(),
                              creation_info.error, free_disk_space);
    }
    creation_info.file = std::move(file);

    result.push_back(std::move(creation_info));
  }
  return EmptyFilesResult(std::move(result), File::FILE_OK, free_disk_space);
}

// Used to evict multiple memory items out to a single file. Caller must
// populate file reference in returned FileCreationInfo. Also returns the free
// disk space AFTER creating this file.
std::pair<FileCreationInfo, int64_t> CreateFileAndWriteItems(
    const FilePath& blob_storage_dir,
    DiskSpaceFuncPtr disk_space_function,
    const FilePath& file_path,
    scoped_refptr<base::TaskRunner> file_task_runner,
    std::vector<base::span<const uint8_t>> data,
    size_t total_size_bytes) {
  DCHECK_NE(0u, total_size_bytes);
  UMA_HISTOGRAM_MEMORY_KB("Storage.Blob.PageFileSize", total_size_bytes / 1024);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  FileCreationInfo creation_info;
  creation_info.file_deletion_runner = std::move(file_task_runner);
  creation_info.error = CreateBlobDirectory(blob_storage_dir);
  if (creation_info.error != File::FILE_OK)
    return std::make_pair(std::move(creation_info), kUnknownDiskAvailability);

  int64_t free_disk_space = disk_space_function(blob_storage_dir);

  // Fail early instead of creating the files if we fill the disk.
  if (free_disk_space != kUnknownDiskAvailability &&
      free_disk_space < static_cast<int64_t>(total_size_bytes)) {
    creation_info.error = File::FILE_ERROR_NO_SPACE;
    return std::make_pair(std::move(creation_info), free_disk_space);
  }
  int64_t disk_availability =
      free_disk_space == kUnknownDiskAvailability
          ? kUnknownDiskAvailability
          : free_disk_space - static_cast<int64_t>(total_size_bytes);

  // Create the page file.
  File file(file_path, File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  creation_info.path = file_path;
  creation_info.error = file.error_details();
  if (creation_info.error != File::FILE_OK)
    return std::make_pair(std::move(creation_info), free_disk_space);

  // Write data.
  file.SetLength(total_size_bytes);
  int bytes_written = 0;
  for (const auto& item : data) {
    size_t length = item.size();
    size_t bytes_left = length;
    while (bytes_left > 0) {
      bytes_written = file.WriteAtCurrentPos(
          reinterpret_cast<const char*>(item.data() + (length - bytes_left)),
          base::saturated_cast<int>(bytes_left));
      if (bytes_written < 0)
        break;
      DCHECK_LE(static_cast<size_t>(bytes_written), bytes_left);
      bytes_left -= bytes_written;
    }
    if (bytes_written < 0)
      break;
  }
  if (!file.Flush()) {
    file.Close();
    base::DeleteFile(file_path, false);
    creation_info.error = File::FILE_ERROR_FAILED;
    return std::make_pair(std::move(creation_info), free_disk_space);
  }

  File::Info info;
  bool success = file.GetInfo(&info);
  creation_info.error =
      bytes_written < 0 || !success ? File::FILE_ERROR_FAILED : File::FILE_OK;
  creation_info.last_modified = info.last_modified;
  return std::make_pair(std::move(creation_info), disk_availability);
}

uint64_t GetTotalSizeAndFileSizes(
    const std::vector<scoped_refptr<ShareableBlobDataItem>>&
        unreserved_file_items,
    std::vector<uint64_t>* file_sizes_output) {
  uint64_t total_size_output = 0;
  base::small_map<std::map<uint64_t, uint64_t>> file_id_to_sizes;
  for (const auto& item : unreserved_file_items) {
    uint64_t file_id = item->item()->GetFutureFileID();
    auto it = file_id_to_sizes.find(file_id);
    if (it != file_id_to_sizes.end())
      it->second =
          std::max(it->second, item->item()->offset() + item->item()->length());
    else
      file_id_to_sizes[file_id] =
          item->item()->offset() + item->item()->length();
    total_size_output += item->item()->length();
  }
  for (const auto& size_pair : file_id_to_sizes) {
    file_sizes_output->push_back(size_pair.second);
  }
  DCHECK_EQ(std::accumulate(file_sizes_output->begin(),
                            file_sizes_output->end(), 0ull),
            total_size_output)
      << "Illegal builder configuration, temporary files must be totally used.";
  return total_size_output;
}

}  // namespace

FileCreationInfo::FileCreationInfo() = default;
FileCreationInfo::~FileCreationInfo() {
  if (file.IsValid()) {
    DCHECK(file_deletion_runner);
    file_deletion_runner->PostTask(
        FROM_HERE, base::BindOnce(&DestructFile, std::move(file)));
  }
}
FileCreationInfo::FileCreationInfo(FileCreationInfo&&) = default;
FileCreationInfo& FileCreationInfo::operator=(FileCreationInfo&&) = default;

MemoryAllocation::MemoryAllocation(
    base::WeakPtr<BlobMemoryController> controller,
    uint64_t item_id,
    size_t length)
    : controller_(std::move(controller)), item_id_(item_id), length_(length) {}

MemoryAllocation::~MemoryAllocation() {
  if (controller_)
    controller_->RevokeMemoryAllocation(item_id_, length_);
}

BlobMemoryController::QuotaAllocationTask::~QuotaAllocationTask() = default;

class BlobMemoryController::MemoryQuotaAllocationTask
    : public BlobMemoryController::QuotaAllocationTask {
 public:
  MemoryQuotaAllocationTask(
      BlobMemoryController* controller,
      size_t quota_request_size,
      std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items,
      MemoryQuotaRequestCallback done_callback)
      : controller_(controller),
        pending_items_(std::move(pending_items)),
        done_callback_(std::move(done_callback)),
        allocation_size_(quota_request_size) {}

  ~MemoryQuotaAllocationTask() override = default;

  void RunDoneCallback(bool success) {
    // Make sure we clear the weak pointers we gave to the caller beforehand.
    weak_factory_.InvalidateWeakPtrs();
    if (success)
      controller_->GrantMemoryAllocations(&pending_items_, allocation_size_);
    std::move(done_callback_).Run(success);
  }

  base::WeakPtr<QuotaAllocationTask> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Cancel() override {
    DCHECK_GE(controller_->pending_memory_quota_total_size_, allocation_size_);
    controller_->pending_memory_quota_total_size_ -= allocation_size_;
    // This call destroys this object.
    controller_->pending_memory_quota_tasks_.erase(my_list_position_);
  }

  // The my_list_position_ iterator is stored so that we can remove ourself
  // from the task list when we are cancelled.
  void set_my_list_position(
      PendingMemoryQuotaTaskList::iterator my_list_position) {
    my_list_position_ = my_list_position;
  }

  size_t allocation_size() const { return allocation_size_; }

 private:
  BlobMemoryController* controller_;
  std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items_;
  MemoryQuotaRequestCallback done_callback_;

  size_t allocation_size_;
  PendingMemoryQuotaTaskList::iterator my_list_position_;

  base::WeakPtrFactory<MemoryQuotaAllocationTask> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MemoryQuotaAllocationTask);
};

class BlobMemoryController::FileQuotaAllocationTask
    : public BlobMemoryController::QuotaAllocationTask {
 public:
  // We post a task to create the file for the items right away.
  FileQuotaAllocationTask(
      BlobMemoryController* memory_controller,
      DiskSpaceFuncPtr disk_space_function,
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
      FileQuotaRequestCallback done_callback)
      : controller_(memory_controller),
        done_callback_(std::move(done_callback)) {
    // Get the file sizes and total size.
    uint64_t total_size =
        GetTotalSizeAndFileSizes(unreserved_file_items, &file_sizes_);

// When we do perf tests that force the file strategy, these often run
// before |CalculateBlobStorageLimitsImpl| is complete. The disk isn't
// enabled until after this call returns (|file_paging_enabled_| is false)
// and |GetAvailableFileSpaceForBlobs()| will thus return 0. So skip this
// check when we have a custom file transportation trigger.
#if DCHECK_IS_ON()
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (LIKELY(
            !command_line->HasSwitch(kBlobFileTransportByFileTriggerSwitch))) {
      DCHECK_LE(total_size, controller_->GetAvailableFileSpaceForBlobs());
    }
#endif
    allocation_size_ = total_size;

    // Check & set our item states.
    for (auto& shareable_item : unreserved_file_items) {
      DCHECK_EQ(ShareableBlobDataItem::QUOTA_NEEDED, shareable_item->state());
      DCHECK_EQ(BlobDataItem::Type::kFile, shareable_item->item()->type());
      shareable_item->set_state(ShareableBlobDataItem::QUOTA_REQUESTED);
    }
    pending_items_ = std::move(unreserved_file_items);

    // Increment disk usage and create our file references.
    controller_->disk_used_ += allocation_size_;
    std::vector<base::FilePath> file_paths;
    std::vector<scoped_refptr<ShareableFileReference>> references;
    for (size_t i = 0; i < file_sizes_.size(); i++) {
      file_paths.push_back(controller_->GenerateNextPageFileName());
      references.push_back(ShareableFileReference::GetOrCreate(
          file_paths.back(), ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          controller_->file_runner_.get()));
    }
    // Send file creation task to file thread.
    base::PostTaskAndReplyWithResult(
        controller_->file_runner_.get(), FROM_HERE,
        base::BindOnce(&CreateEmptyFiles, controller_->blob_storage_dir_,
                       disk_space_function, controller_->file_runner_,
                       std::move(file_paths)),
        base::BindOnce(&FileQuotaAllocationTask::OnCreateEmptyFiles,
                       weak_factory_.GetWeakPtr(), std::move(references),
                       allocation_size_));
    controller_->RecordTracingCounters();
  }
  ~FileQuotaAllocationTask() override = default;

  void RunDoneCallback(std::vector<FileCreationInfo> file_info, bool success) {
    // Make sure we clear the weak pointers we gave to the caller beforehand.
    weak_factory_.InvalidateWeakPtrs();

    // We want to destroy this object on the exit of this method if we were
    // successful.
    std::unique_ptr<FileQuotaAllocationTask> this_object;
    if (success) {
      // Register the disk space accounting callback.
      DCHECK_EQ(file_info.size(), file_sizes_.size());
      for (size_t i = 0; i < file_sizes_.size(); i++) {
        file_info[i].file_reference->AddFinalReleaseCallback(base::BindOnce(
            &BlobMemoryController::OnBlobFileDelete,
            controller_->weak_factory_.GetWeakPtr(), file_sizes_[i]));
      }
      for (auto& item : pending_items_) {
        item->set_state(ShareableBlobDataItem::QUOTA_GRANTED);
      }
      this_object = std::move(*my_list_position_);
      controller_->pending_file_quota_tasks_.erase(my_list_position_);
    }

    std::move(done_callback_).Run(std::move(file_info), success);
  }

  base::WeakPtr<QuotaAllocationTask> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Cancel() override {
    DCHECK_GE(controller_->disk_used_, allocation_size_);
    controller_->disk_used_ -= allocation_size_;
    // This call destroys this object.
    controller_->pending_file_quota_tasks_.erase(my_list_position_);
  }

  void OnCreateEmptyFiles(
      std::vector<scoped_refptr<ShareableFileReference>> references,
      uint64_t new_files_total_size,
      EmptyFilesResult result) {
    int64_t avail_disk_space = result.disk_availability;
    if (result.files.empty()) {
      DCHECK_NE(result.file_error, File::FILE_OK);
      DCHECK_GE(controller_->disk_used_, allocation_size_);
      controller_->disk_used_ -= allocation_size_;
      // This will call our callback and delete the object correctly.
      controller_->DisableFilePaging(result.file_error);
      return;
    }
    // The allocation won't fit at all. Cancel this request. The disk will be
    // decremented when the file is deleted through AddFinalReleaseCallback.
    if (avail_disk_space != kUnknownDiskAvailability &&
        base::checked_cast<uint64_t>(avail_disk_space) < new_files_total_size) {
      DCHECK_GE(controller_->disk_used_, allocation_size_);
      controller_->disk_used_ -= allocation_size_;
      controller_->AdjustDiskUsage(static_cast<uint64_t>(avail_disk_space));
      controller_->file_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DeleteFiles, std::move(result.files)));
      std::unique_ptr<FileQuotaAllocationTask> this_object =
          std::move(*my_list_position_);
      controller_->pending_file_quota_tasks_.erase(my_list_position_);
      RunDoneCallback(std::vector<FileCreationInfo>(), false);
      return;
    }
    if (avail_disk_space != kUnknownDiskAvailability) {
      controller_->AdjustDiskUsage(base::checked_cast<uint64_t>(
          avail_disk_space - new_files_total_size));
    }
    DCHECK_EQ(result.files.size(), references.size());
    for (size_t i = 0; i < result.files.size(); i++) {
      result.files[i].file_reference = std::move(references[i]);
    }
    RunDoneCallback(std::move(result.files), true);
  }

  // The my_list_position_ iterator is stored so that we can remove ourself
  // from the task list when we are cancelled.
  void set_my_list_position(
      PendingFileQuotaTaskList::iterator my_list_position) {
    my_list_position_ = my_list_position;
  }

  size_t allocation_size() const { return allocation_size_; }

 private:
  BlobMemoryController* controller_;
  std::vector<uint64_t> file_sizes_;
  std::vector<scoped_refptr<ShareableBlobDataItem>> pending_items_;
  FileQuotaRequestCallback done_callback_;

  uint64_t allocation_size_;
  PendingFileQuotaTaskList::iterator my_list_position_;

  base::WeakPtrFactory<FileQuotaAllocationTask> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FileQuotaAllocationTask);
};

BlobMemoryController::BlobMemoryController(
    const base::FilePath& storage_directory,
    scoped_refptr<base::TaskRunner> file_runner)
    : file_paging_enabled_(file_runner.get() != nullptr),
      blob_storage_dir_(storage_directory),
      file_runner_(std::move(file_runner)),
      disk_space_function_(&base::SysInfo::AmountOfFreeDiskSpace),
      populated_memory_items_(
          base::MRUCache<uint64_t, ShareableBlobDataItem*>::NO_AUTO_EVICT),
      memory_pressure_listener_(
          base::BindRepeating(&BlobMemoryController::OnMemoryPressure,
                              base::Unretained(this))) {}

BlobMemoryController::~BlobMemoryController() = default;

void BlobMemoryController::DisableFilePaging(base::File::Error reason) {
  UMA_HISTOGRAM_ENUMERATION("Storage.Blob.PagingDisabled", -reason,
                            -File::FILE_ERROR_MAX);
  DLOG(ERROR) << "Blob storage paging disabled, reason: " << reason;
  file_paging_enabled_ = false;
  in_flight_memory_used_ = 0;
  items_paging_to_file_.clear();
  pending_evictions_ = 0;
  pending_memory_quota_total_size_ = 0;
  populated_memory_items_.Clear();
  populated_memory_items_bytes_ = 0;
  file_runner_ = nullptr;

  PendingMemoryQuotaTaskList old_memory_tasks;
  PendingFileQuotaTaskList old_file_tasks;
  std::swap(old_memory_tasks, pending_memory_quota_tasks_);
  std::swap(old_file_tasks, pending_file_quota_tasks_);

  // Don't call the callbacks until we have a consistent state.
  for (auto& memory_request : old_memory_tasks) {
    memory_request->RunDoneCallback(false);
  }
  for (auto& file_request : old_file_tasks) {
    // OnBlobFileDelete is registered when RunDoneCallback is called with
    // |true|, so manually do disk accounting.
    disk_used_ -= file_request->allocation_size();
    file_request->RunDoneCallback(std::vector<FileCreationInfo>(), false);
  }
}

BlobMemoryController::Strategy BlobMemoryController::DetermineStrategy(
    size_t preemptive_transported_bytes,
    uint64_t total_transportation_bytes) const {
  if (total_transportation_bytes == 0)
    return Strategy::NONE_NEEDED;
  if (!CanReserveQuota(total_transportation_bytes))
    return Strategy::TOO_LARGE;

  // Handle the case where we have all the bytes preemptively transported, and
  // we can also fit them.
  if (preemptive_transported_bytes == total_transportation_bytes &&
      pending_memory_quota_tasks_.empty() &&
      preemptive_transported_bytes <= GetAvailableMemoryForBlobs()) {
    return Strategy::NONE_NEEDED;
  }

  if (UNLIKELY(limits_.override_file_transport_min_size > 0) &&
      file_paging_enabled_ &&
      total_transportation_bytes >= limits_.override_file_transport_min_size) {
    return Strategy::FILE;
  }

  if (total_transportation_bytes <= limits_.max_ipc_memory_size)
    return Strategy::IPC;

  if (file_paging_enabled_ &&
      total_transportation_bytes <= GetAvailableFileSpaceForBlobs() &&
      total_transportation_bytes > limits_.memory_limit_before_paging()) {
    return Strategy::FILE;
  }
  return Strategy::SHARED_MEMORY;
}

bool BlobMemoryController::CanReserveQuota(uint64_t size) const {
  // We check each size independently as a blob can't be constructed in both
  // disk and memory.
  return size <= GetAvailableMemoryForBlobs() ||
         size <= GetAvailableFileSpaceForBlobs();
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::ReserveMemoryQuota(
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
    MemoryQuotaRequestCallback done_callback) {
  if (unreserved_memory_items.empty()) {
    std::move(done_callback).Run(true);
    return base::WeakPtr<QuotaAllocationTask>();
  }

  base::CheckedNumeric<uint64_t> unsafe_total_bytes_needed = 0;
  for (auto& item : unreserved_memory_items) {
    DCHECK_EQ(ShareableBlobDataItem::QUOTA_NEEDED, item->state());
    DCHECK(item->item()->type() == BlobDataItem::Type::kBytesDescription ||
           item->item()->type() == BlobDataItem::Type::kBytes);
    DCHECK(item->item()->length() > 0);
    unsafe_total_bytes_needed += item->item()->length();
    item->set_state(ShareableBlobDataItem::QUOTA_REQUESTED);
  }

  uint64_t total_bytes_needed = unsafe_total_bytes_needed.ValueOrDie();
  DCHECK_GT(total_bytes_needed, 0ull);

  // If we're currently waiting for blobs to page already, then we add
  // ourselves to the end of the queue. Once paging is complete, we'll schedule
  // more paging for any more pending blobs.
  if (!pending_memory_quota_tasks_.empty()) {
    return AppendMemoryTask(total_bytes_needed,
                            std::move(unreserved_memory_items),
                            std::move(done_callback));
  }

  // Store right away if we can.
  if (total_bytes_needed <= GetAvailableMemoryForBlobs()) {
    GrantMemoryAllocations(&unreserved_memory_items,
                           static_cast<size_t>(total_bytes_needed));
    MaybeScheduleEvictionUntilSystemHealthy(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
    std::move(done_callback).Run(true);
    return base::WeakPtr<QuotaAllocationTask>();
  }

  // Size is larger than available memory.
  DCHECK(pending_memory_quota_tasks_.empty());
  DCHECK_EQ(0u, pending_memory_quota_total_size_);

  auto weak_ptr =
      AppendMemoryTask(total_bytes_needed, std::move(unreserved_memory_items),
                       std::move(done_callback));
  MaybeScheduleEvictionUntilSystemHealthy(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  return weak_ptr;
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::ReserveFileQuota(
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
    FileQuotaRequestCallback done_callback) {
  pending_file_quota_tasks_.push_back(std::make_unique<FileQuotaAllocationTask>(
      this, disk_space_function_, std::move(unreserved_file_items),
      std::move(done_callback)));
  pending_file_quota_tasks_.back()->set_my_list_position(
      --pending_file_quota_tasks_.end());
  return pending_file_quota_tasks_.back()->GetWeakPtr();
}

void BlobMemoryController::ShrinkMemoryAllocation(ShareableBlobDataItem* item) {
  DCHECK(item->HasGrantedQuota());
  DCHECK_EQ(item->item()->type(), BlobDataItem::Type::kBytes);
  DCHECK_GE(item->memory_allocation_->length(), item->item()->length());
  DCHECK_EQ(item->memory_allocation_->controller_.get(), this);

  // Setting a new MemoryAllocation will delete and free the existing memory
  // allocation, so here we only have to account for the new allocation.
  blob_memory_used_ += item->item()->length();
  item->set_memory_allocation(std::make_unique<MemoryAllocation>(
      weak_factory_.GetWeakPtr(), item->item_id(),
      base::checked_cast<size_t>(item->item()->length())));
  MaybeGrantPendingMemoryRequests();
}

void BlobMemoryController::ShrinkFileAllocation(
    ShareableFileReference* file_reference,
    uint64_t old_length,
    uint64_t new_length) {
  DCHECK_GE(old_length, new_length);

  DCHECK_GE(disk_used_, old_length - new_length);
  disk_used_ -= old_length - new_length;
  file_reference->AddFinalReleaseCallback(
      base::BindOnce(&BlobMemoryController::OnShrunkenBlobFileDelete,
                     weak_factory_.GetWeakPtr(), old_length - new_length));
}

void BlobMemoryController::GrowFileAllocation(
    ShareableFileReference* file_reference,
    uint64_t delta) {
  DCHECK_LE(delta, GetAvailableFileSpaceForBlobs());
  disk_used_ += delta;
  file_reference->AddFinalReleaseCallback(
      base::BindOnce(&BlobMemoryController::OnBlobFileDelete,
                     weak_factory_.GetWeakPtr(), delta));
}

void BlobMemoryController::NotifyMemoryItemsUsed(
    const std::vector<scoped_refptr<ShareableBlobDataItem>>& items) {
  for (const auto& item : items) {
    if (item->item()->type() != BlobDataItem::Type::kBytes ||
        item->state() != ShareableBlobDataItem::POPULATED_WITH_QUOTA) {
      continue;
    }
    // We don't want to re-add the item if we're currently paging it to disk.
    if (items_paging_to_file_.find(item->item_id()) !=
        items_paging_to_file_.end()) {
      return;
    }
    auto iterator = populated_memory_items_.Get(item->item_id());
    if (iterator == populated_memory_items_.end()) {
      populated_memory_items_bytes_ +=
          static_cast<size_t>(item->item()->length());
      populated_memory_items_.Put(item->item_id(), item.get());
    }
  }
  MaybeScheduleEvictionUntilSystemHealthy(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
}

void BlobMemoryController::CallWhenStorageLimitsAreKnown(
    base::OnceClosure callback) {
  if (did_calculate_storage_limits_) {
    std::move(callback).Run();
    return;
  }
  on_calculate_limits_callbacks_.push_back(std::move(callback));
  CalculateBlobStorageLimits();
}

void BlobMemoryController::CalculateBlobStorageLimits() {
  if (did_schedule_limit_calculation_)
    return;
  did_schedule_limit_calculation_ = true;
  if (file_runner_) {
    PostTaskAndReplyWithResult(
        file_runner_.get(), FROM_HERE,
        base::BindOnce(&CalculateBlobStorageLimitsImpl, blob_storage_dir_,
                       true, amount_of_memory_for_testing_),
        base::BindOnce(&BlobMemoryController::OnStorageLimitsCalculated,
                       weak_factory_.GetWeakPtr()));
  } else {
    OnStorageLimitsCalculated(CalculateBlobStorageLimitsImpl(
        blob_storage_dir_, false, amount_of_memory_for_testing_));
  }
}

base::WeakPtr<BlobMemoryController> BlobMemoryController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BlobMemoryController::OnStorageLimitsCalculated(BlobStorageLimits limits) {
  DCHECK(limits.IsValid());
  if (manual_limits_set_)
    return;
  limits_ = limits;
  did_calculate_storage_limits_ = true;
  for (auto& callback : on_calculate_limits_callbacks_)
    std::move(callback).Run();
  on_calculate_limits_callbacks_.clear();
}

namespace {
// Used in UMA metrics, do not change values.
enum DiskSpaceAdjustmentType {
  FREEZE_HIT_MIN_AVAILABLE = 0,
  LOWERED_NEAR_MIN_AVAILABLE = 1,
  RAISED_NEAR_MIN_AVAILABLE = 2,
  RESTORED = 3,
  MAX_ADJUSTMENT_TYPE
};

enum DiskSpaceAdjustmentStatus { FROZEN, ADJUSTED, NORMAL };
}  // namespace

void BlobMemoryController::AdjustDiskUsage(uint64_t avail_disk) {
  DCHECK_LE(disk_used_, limits_.desired_max_disk_space +
                            limits_.min_available_external_disk_space());

  DiskSpaceAdjustmentStatus curr_status;
  if (limits_.effective_max_disk_space == limits_.desired_max_disk_space) {
    curr_status = NORMAL;
  } else if (limits_.effective_max_disk_space == disk_used_) {
    curr_status = FROZEN;
  } else {
    curr_status = ADJUSTED;
  }
  uint64_t old_effective_max_disk_space = limits_.effective_max_disk_space;
  uint64_t avail_disk_without_blobs = avail_disk + disk_used_;

  // Note: The UMA metrics here intended to record state change between frozen,
  // adjusted, and normal states.

  if (avail_disk <= limits_.min_available_external_disk_space()) {
    limits_.effective_max_disk_space = disk_used_;
    if (curr_status != FROZEN &&
        limits_.effective_max_disk_space != old_effective_max_disk_space) {
      UMA_HISTOGRAM_ENUMERATION("Storage.Blob.MaxDiskSpaceAdjustment",
                                FREEZE_HIT_MIN_AVAILABLE, MAX_ADJUSTMENT_TYPE);
    }
  } else if (avail_disk_without_blobs <
             limits_.min_available_external_disk_space() +
                 limits_.desired_max_disk_space) {
    // |effective_max_disk_space| is guaranteed to be less than
    // |desired_max_disk_space| by the if statement.
    limits_.effective_max_disk_space =
        avail_disk_without_blobs - limits_.min_available_external_disk_space();
    if (curr_status != ADJUSTED &&
        limits_.effective_max_disk_space != old_effective_max_disk_space) {
      UMA_HISTOGRAM_ENUMERATION("Storage.Blob.MaxDiskSpaceAdjustment",
                                curr_status == NORMAL
                                    ? LOWERED_NEAR_MIN_AVAILABLE
                                    : RAISED_NEAR_MIN_AVAILABLE,
                                MAX_ADJUSTMENT_TYPE);
    }
  } else {
    limits_.effective_max_disk_space = limits_.desired_max_disk_space;
    if (curr_status != NORMAL &&
        limits_.effective_max_disk_space != old_effective_max_disk_space) {
      UMA_HISTOGRAM_ENUMERATION("Storage.Blob.MaxDiskSpaceAdjustment", RESTORED,
                                MAX_ADJUSTMENT_TYPE);
    }
  }
}

base::WeakPtr<QuotaAllocationTask> BlobMemoryController::AppendMemoryTask(
    uint64_t total_bytes_needed,
    std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
    MemoryQuotaRequestCallback done_callback) {
  DCHECK(file_paging_enabled_)
      << "Caller tried to reserve memory when CanReserveQuota("
      << total_bytes_needed << ") would have returned false.";

  pending_memory_quota_total_size_ += total_bytes_needed;
  pending_memory_quota_tasks_.push_back(
      std::make_unique<MemoryQuotaAllocationTask>(
          this, total_bytes_needed, std::move(unreserved_memory_items),
          std::move(done_callback)));
  pending_memory_quota_tasks_.back()->set_my_list_position(
      --pending_memory_quota_tasks_.end());

  return pending_memory_quota_tasks_.back()->GetWeakPtr();
}

void BlobMemoryController::MaybeGrantPendingMemoryRequests() {
  while (!pending_memory_quota_tasks_.empty() &&
         limits_.max_blob_in_memory_space - blob_memory_used_ >=
             pending_memory_quota_tasks_.front()->allocation_size()) {
    std::unique_ptr<MemoryQuotaAllocationTask> memory_task =
        std::move(pending_memory_quota_tasks_.front());
    pending_memory_quota_tasks_.pop_front();
    pending_memory_quota_total_size_ -= memory_task->allocation_size();
    memory_task->RunDoneCallback(true);
  }
  RecordTracingCounters();
}

size_t BlobMemoryController::CollectItemsForEviction(
    std::vector<scoped_refptr<ShareableBlobDataItem>>* output,
    uint64_t min_page_file_size) {
  base::CheckedNumeric<size_t> total_items_size = 0;
  // Process the recent item list and remove items until we have at least a
  // minimum file size or we're at the end of our items to page to disk.
  while (total_items_size.ValueOrDie() < min_page_file_size &&
         !populated_memory_items_.empty()) {
    auto iterator = --populated_memory_items_.end();
    ShareableBlobDataItem* item = iterator->second;
    DCHECK_EQ(item->item()->type(), BlobDataItem::Type::kBytes);
    populated_memory_items_.Erase(iterator);
    size_t size = base::checked_cast<size_t>(item->item()->length());
    populated_memory_items_bytes_ -= size;
    total_items_size += size;
    output->push_back(base::WrapRefCounted(item));
  }
  return total_items_size.ValueOrDie();
}

void BlobMemoryController::MaybeScheduleEvictionUntilSystemHealthy(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // Don't do eviction when others are happening, as we don't change our
  // pending_memory_quota_total_size_ value until after the paging files have
  // been written.
  if (pending_evictions_ != 0 || !file_paging_enabled_)
    return;

  uint64_t total_memory_usage =
      static_cast<uint64_t>(pending_memory_quota_total_size_) +
      blob_memory_used_;

  size_t in_memory_limit = limits_.memory_limit_before_paging();
  uint64_t min_page_file_size = limits_.min_page_file_size;
  if (memory_pressure_level !=
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    in_memory_limit = 0;
    // Use lower page file size to reduce using more memory for writing under
    // pressure.
    min_page_file_size = limits_.max_blob_in_memory_space *
                         limits_.max_blob_in_memory_space_under_pressure_ratio;
  }

  // We try to page items to disk until our current system size + requested
  // memory is below our size limit.
  // Size limit is a lower |memory_limit_before_paging()| if we have disk space.
  while (disk_used_ < limits_.effective_max_disk_space &&
         total_memory_usage > in_memory_limit) {
    const char* reason = nullptr;
    if (memory_pressure_level !=
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
      reason = "OnMemoryPressure";
    } else {
      reason = "SizeExceededInMemoryLimit";
    }

    // We only page when we have enough items to fill a whole page file.
    if (populated_memory_items_bytes_ < min_page_file_size)
      break;
    DCHECK_LE(min_page_file_size, static_cast<uint64_t>(blob_memory_used_));

    std::vector<scoped_refptr<ShareableBlobDataItem>> items_to_swap;

    size_t total_items_size =
        CollectItemsForEviction(&items_to_swap, min_page_file_size);
    if (total_items_size == 0)
      break;

    std::vector<base::span<const uint8_t>> data_for_paging;
    for (auto& shared_blob_item : items_to_swap) {
      items_paging_to_file_.insert(shared_blob_item->item_id());
      data_for_paging.push_back(shared_blob_item->item()->bytes());
    }

    // Update our bookkeeping.
    pending_evictions_++;
    disk_used_ += total_items_size;
    in_flight_memory_used_ += total_items_size;

    // Create our file reference.
    FilePath page_file_path = GenerateNextPageFileName();
    scoped_refptr<ShareableFileReference> file_reference =
        ShareableFileReference::GetOrCreate(
            page_file_path,
            ShareableFileReference::DELETE_ON_FINAL_RELEASE,
            file_runner_.get());
    // Add the release callback so we decrement our disk usage on file deletion.
    file_reference->AddFinalReleaseCallback(
        base::BindOnce(&BlobMemoryController::OnBlobFileDelete,
                       weak_factory_.GetWeakPtr(), total_items_size));

    // Post the file writing task.
    base::PostTaskAndReplyWithResult(
        file_runner_.get(), FROM_HERE,
        base::BindOnce(&CreateFileAndWriteItems, blob_storage_dir_,
                       disk_space_function_, std::move(page_file_path),
                       file_runner_, std::move(data_for_paging),
                       total_items_size),
        base::BindOnce(&BlobMemoryController::OnEvictionComplete,
                       weak_factory_.GetWeakPtr(), std::move(file_reference),
                       std::move(items_to_swap), total_items_size, reason,
                       total_memory_usage));

    last_eviction_time_ = base::TimeTicks::Now();
  }
  RecordTracingCounters();
}

void BlobMemoryController::OnEvictionComplete(
    scoped_refptr<ShareableFileReference> file_reference,
    std::vector<scoped_refptr<ShareableBlobDataItem>> items,
    size_t total_items_size,
    const char* evict_reason,
    size_t memory_usage_before_eviction,
    std::pair<FileCreationInfo, int64_t /* avail_disk */> result) {
  if (!file_paging_enabled_)
    return;

  FileCreationInfo& file_info = std::get<0>(result);
  int64_t avail_disk_space = std::get<1>(result);

  if (file_info.error != File::FILE_OK) {
    DisableFilePaging(file_info.error);
    return;
  }

  if (avail_disk_space != kUnknownDiskAvailability) {
    AdjustDiskUsage(static_cast<uint64_t>(avail_disk_space));
  }

  DCHECK_LT(0, pending_evictions_);
  pending_evictions_--;

  // Switch item from memory to the new file.
  uint64_t offset = 0;
  for (const scoped_refptr<ShareableBlobDataItem>& shareable_item : items) {
    scoped_refptr<BlobDataItem> new_item = BlobDataItem::CreateFile(
        file_reference->path(), offset, shareable_item->item()->length(),
        file_info.last_modified, file_reference);
    DCHECK(shareable_item->memory_allocation_);
    shareable_item->set_memory_allocation(nullptr);
    shareable_item->set_item(new_item);
    items_paging_to_file_.erase(shareable_item->item_id());
    offset += shareable_item->item()->length();
  }
  in_flight_memory_used_ -= total_items_size;

  // Record change in memory usage at the last eviction reply.
  size_t total_usage = blob_memory_used_ + pending_memory_quota_total_size_;
  if (!pending_evictions_ && memory_usage_before_eviction >= total_usage) {
    std::string full_histogram_name =
        std::string("Storage.Blob.SizeEvictedToDiskInKB.") + evict_reason;
    base::UmaHistogramCounts100000(
        full_histogram_name,
        (memory_usage_before_eviction - total_usage) / 1024);
  }

  // We want callback on blobs up to the amount we've freed.
  MaybeGrantPendingMemoryRequests();

  // If we still have more blobs waiting and we're not waiting on more paging
  // operations, schedule more.
  MaybeScheduleEvictionUntilSystemHealthy(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
}

void BlobMemoryController::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  auto time_from_last_evicion = base::TimeTicks::Now() - last_eviction_time_;
  if (last_eviction_time_ != base::TimeTicks() &&
      time_from_last_evicion.InSeconds() < kMinSecondsForPressureEvictions) {
    return;
  }

  MaybeScheduleEvictionUntilSystemHealthy(memory_pressure_level);
}

FilePath BlobMemoryController::GenerateNextPageFileName() {
  std::string file_name = base::NumberToString(current_file_num_++);
  return blob_storage_dir_.Append(FilePath::FromUTF8Unsafe(file_name));
}

void BlobMemoryController::RecordTracingCounters() const {
  TRACE_COUNTER2("Blob", "MemoryUsage", "TotalStorage", blob_memory_used_,
                 "InFlightToDisk", in_flight_memory_used_);
  TRACE_COUNTER1("Blob", "DiskUsage", disk_used_);
  TRACE_COUNTER1("Blob", "TransfersPendingOnDisk",
                 pending_memory_quota_tasks_.size());
  TRACE_COUNTER1("Blob", "TransfersBytesPendingOnDisk",
                 pending_memory_quota_total_size_);
}

size_t BlobMemoryController::GetAvailableMemoryForBlobs() const {
  if (limits_.max_blob_in_memory_space < memory_usage())
    return 0;
  return limits_.max_blob_in_memory_space - memory_usage();
}

uint64_t BlobMemoryController::GetAvailableFileSpaceForBlobs() const {
  if (!file_paging_enabled_)
    return 0;
  // Sometimes we're only paging part of what we need for the new blob, so add
  // the rest of the size we need into our disk usage if this is the case.
  uint64_t total_disk_used = disk_used_;
  if (in_flight_memory_used_ < pending_memory_quota_total_size_) {
    total_disk_used +=
        pending_memory_quota_total_size_ - in_flight_memory_used_;
  }
  if (limits_.effective_max_disk_space < total_disk_used)
    return 0;
  return limits_.effective_max_disk_space - total_disk_used;
}

void BlobMemoryController::GrantMemoryAllocations(
    std::vector<scoped_refptr<ShareableBlobDataItem>>* items,
    size_t total_bytes) {
  // These metrics let us calculate the global distribution of blob storage by
  // subtracting the histograms.
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.StorageSizeBeforeAppend",
                          blob_memory_used_ / 1024);
  blob_memory_used_ += total_bytes;
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.StorageSizeAfterAppend",
                          blob_memory_used_ / 1024);

  for (auto& item : *items) {
    item->set_state(ShareableBlobDataItem::QUOTA_GRANTED);
    item->set_memory_allocation(std::make_unique<MemoryAllocation>(
        weak_factory_.GetWeakPtr(), item->item_id(),
        base::checked_cast<size_t>(item->item()->length())));
  }
}

void BlobMemoryController::RevokeMemoryAllocation(uint64_t item_id,
                                                  size_t length) {
  DCHECK_LE(length, blob_memory_used_);

  // These metrics let us calculate the global distribution of blob storage by
  // subtracting the histograms.
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.StorageSizeBeforeAppend",
                          blob_memory_used_ / 1024);
  blob_memory_used_ -= length;
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.StorageSizeAfterAppend",
                          blob_memory_used_ / 1024);

  auto iterator = populated_memory_items_.Get(item_id);
  if (iterator != populated_memory_items_.end()) {
    DCHECK_GE(populated_memory_items_bytes_, length);
    populated_memory_items_bytes_ -= length;
    populated_memory_items_.Erase(iterator);
  }
  MaybeGrantPendingMemoryRequests();
}

void BlobMemoryController::OnBlobFileDelete(uint64_t size,
                                            const FilePath& path) {
  DCHECK_LE(size, disk_used_);
  disk_used_ -= size;
}

void BlobMemoryController::OnShrunkenBlobFileDelete(uint64_t shrink_delta,
                                                    const FilePath& path) {
  disk_used_ += shrink_delta;
}

}  // namespace storage
