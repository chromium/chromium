// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <inttypes.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/browser/blob/write_blob_to_file.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "url/gurl.h"

namespace storage {
namespace {
using ItemCopyEntry = BlobEntry::ItemCopyEntry;
using QuotaAllocationTask = BlobMemoryController::QuotaAllocationTask;
}  // namespace

BlobStorageContext::BlobStorageContext()
    : profile_directory_(base::FilePath()),
      memory_controller_(base::FilePath(), scoped_refptr<base::TaskRunner>()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "BlobStorageContext",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

BlobStorageContext::BlobStorageContext(
    const base::FilePath& profile_directory,
    const base::FilePath& storage_directory,
    scoped_refptr<base::TaskRunner> file_runner)
    : profile_directory_(profile_directory),
      memory_controller_(storage_directory, std::move(file_runner)) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "BlobStorageContext",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

BlobStorageContext::~BlobStorageContext() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return nullptr;
  return CreateHandle(uuid, entry);
}

void BlobStorageContext::GetBlobDataFromBlobRemote(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    base::OnceCallback<void(std::unique_ptr<BlobDataHandle>)> callback) {
  DCHECK(blob);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  blink::mojom::Blob* raw_blob = blob_remote.get();
  raw_blob->GetInternalUUID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(
          [](mojo::Remote<blink::mojom::Blob>,
             base::WeakPtr<BlobStorageContext> context,
             base::OnceCallback<void(std::unique_ptr<BlobDataHandle>)> callback,
             const std::string& uuid) {
            if (!context || uuid.empty()) {
              std::move(callback).Run(nullptr);
              return;
            }
            std::move(callback).Run(context->GetBlobDataFromUUID(uuid));
          },
          std::move(blob_remote), AsWeakPtr(), std::move(callback)),
      ""));
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    std::unique_ptr<BlobDataBuilder> external_builder) {
  TRACE_EVENT0("Blob", "Context::AddFinishedBlob");
  return BuildBlob(std::move(external_builder), TransportAllowedCallback());
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    std::vector<scoped_refptr<ShareableBlobDataItem>> items) {
  TRACE_EVENT0("Blob", "Context::AddFinishedBlobFromItems");
  BlobEntry* entry =
      registry_.CreateEntry(uuid, content_type, content_disposition);
  for (const auto& item : items) {
    DCHECK_EQ(item->state(), ShareableBlobDataItem::POPULATED_WITH_QUOTA);
    DCHECK_NE(BlobDataItem::Type::kBytesDescription, item->item()->type());
    DCHECK(!item->item()->IsFutureFileItem());
  }

  entry->SetSharedBlobItems(std::move(items));
  std::unique_ptr<BlobDataHandle> handle = CreateHandle(uuid, entry);
  entry->set_status(BlobStatus::DONE);
  memory_controller_.NotifyMemoryItemsUsed(entry->items());
  return handle;
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddBrokenBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    BlobStatus reason) {
  DCHECK(!registry_.HasEntry(uuid));
  DCHECK(BlobStatusIsError(reason));
  BlobEntry* entry =
      registry_.CreateEntry(uuid, content_type, content_disposition);
  entry->set_status(reason);
  FinishBuilding(entry);
  return CreateHandle(uuid, entry);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFutureBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    BuildAbortedCallback build_aborted_callback) {
  DCHECK(!registry_.HasEntry(uuid));

  BlobEntry* entry =
      registry_.CreateEntry(uuid, content_type, content_disposition);
  entry->set_size(blink::BlobUtils::kUnknownSize);
  entry->set_status(BlobStatus::PENDING_CONSTRUCTION);
  entry->set_building_state(std::make_unique<BlobEntry::BuildingState>(
      false, TransportAllowedCallback(), 0));
  entry->building_state_->build_aborted_callback =
      std::move(build_aborted_callback);
  return CreateHandle(uuid, entry);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildPreregisteredBlob(
    std::unique_ptr<BlobDataBuilder> content,
    TransportAllowedCallback transport_allowed_callback) {
  BlobEntry* entry = registry_.GetEntry(content->uuid());
  DCHECK(entry);
  DCHECK_EQ(BlobStatus::PENDING_CONSTRUCTION, entry->status());
  entry->set_size(0);

  return BuildBlobInternal(entry, std::move(content),
                           std::move(transport_allowed_callback));
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildBlob(
    std::unique_ptr<BlobDataBuilder> content,
    TransportAllowedCallback transport_allowed_callback) {
  DCHECK(!registry_.HasEntry(content->uuid_));

  BlobEntry* entry = registry_.CreateEntry(
      content->uuid(), content->content_type_, content->content_disposition_);

  return BuildBlobInternal(entry, std::move(content),
                           std::move(transport_allowed_callback));
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildBlobInternal(
    BlobEntry* entry,
    std::unique_ptr<BlobDataBuilder> content,
    TransportAllowedCallback transport_allowed_callback) {
#if DCHECK_IS_ON()
  bool contains_unpopulated_transport_items = false;
  for (const auto& item : content->pending_transport_items()) {
    if (item->item()->type() == BlobDataItem::Type::kBytesDescription ||
        item->item()->type() == BlobDataItem::Type::kFile)
      contains_unpopulated_transport_items = true;
  }

  DCHECK(!contains_unpopulated_transport_items || transport_allowed_callback)
      << "If we have pending unpopulated content then a callback is required";
#endif

  entry->SetSharedBlobItems(content->ReleaseItems());

  DCHECK((content->total_size() == 0 && !content->IsValid()) ||
         content->total_size() == entry->total_size())
      << content->total_size() << " vs " << entry->total_size();

  if (!content->IsValid()) {
    entry->set_size(0);
    entry->set_status(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  } else if (content->transport_quota_needed()) {
    entry->set_status(BlobStatus::PENDING_QUOTA);
  } else {
    entry->set_status(BlobStatus::PENDING_REFERENCED_BLOBS);
  }

  std::unique_ptr<BlobDataHandle> handle = CreateHandle(content->uuid_, entry);

  TransportQuotaType transport_quota_type = content->found_memory_transport()
                                                ? TransportQuotaType::MEMORY
                                                : TransportQuotaType::FILE;

  std::vector<scoped_refptr<BlobDataItem>> items_needing_timestamp;
  std::vector<base::FilePath> file_paths_needing_timestamp;
  for (auto& item : entry->items_) {
    if (item->item()->type() == BlobDataItem::Type::kFile &&
        !item->item()->IsFutureFileItem() &&
        item->item()->expected_modification_time().is_null()) {
      items_needing_timestamp.push_back(item->item());
      file_paths_needing_timestamp.push_back(item->item()->path());
    }
  }
  if (!items_needing_timestamp.empty()) {
    // Blob construction isn't blocked on getting these timestamps. The created
    // blob will be fully functional whether or not timestamps are set. When
    // the timestamp isn't set the blob just won't be able to detect the file
    // on disk changing after the blob is created.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](std::vector<base::FilePath> paths) {
              std::vector<base::Time> result;
              result.reserve(paths.size());
              for (const auto& path : paths) {
                base::File::Info info;
                if (!base::GetFileInfo(path, &info)) {
                  result.emplace_back();
                  continue;
                }
                result.push_back(info.last_modified);
              }
              return result;
            },
            std::move(file_paths_needing_timestamp)),
        base::BindOnce(&BlobDataItem::SetFileModificationTimes,
                       std::move(items_needing_timestamp)));
  }

  size_t num_building_dependent_blobs = 0;
  std::vector<std::unique_ptr<BlobDataHandle>> dependent_blobs;
  // We hold a handle to all blobs we're using. This is important, as our memory
  // accounting can be delayed until OnEnoughSizeForBlobData is called, and we
  // only free memory on canceling when we've done this accounting. If a
  // dependent blob is dereferenced, then we're the last blob holding onto that
  // data item, and we need to account for that. So we prevent that case by
  // holding onto all blobs.
  for (const std::string& dependent_blob_uuid : content->dependent_blobs()) {
    BlobEntry* blob_entry = registry_.GetEntry(dependent_blob_uuid);
    DCHECK(blob_entry);
    if (BlobStatusIsError(blob_entry->status())) {
      entry->set_status(BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
      break;
    }
    dependent_blobs.push_back(CreateHandle(dependent_blob_uuid, blob_entry));
    if (BlobStatusIsPending(blob_entry->status())) {
      blob_entry->building_state_->build_completion_callbacks.push_back(
          base::BindOnce(&BlobStorageContext::OnDependentBlobFinished,
                         ptr_factory_.GetWeakPtr(), content->uuid_));
      num_building_dependent_blobs++;
    }
  }

  std::vector<raw_ptr<ShareableBlobDataItem, VectorExperimental>>
      transport_items;
  transport_items.reserve(content->pending_transport_items().size());
  for (const auto& item : content->pending_transport_items())
    transport_items.emplace_back(item.get());

  std::vector<scoped_refptr<ShareableBlobDataItem>> pending_copy_items;
  pending_copy_items.reserve(content->copies().size());
  for (const auto& copy : content->copies())
    pending_copy_items.emplace_back(copy.dest_item);

  auto previous_building_state = std::move(entry->building_state_);
  entry->set_building_state(std::make_unique<BlobEntry::BuildingState>(
      !content->pending_transport_items().empty(),
      std::move(transport_allowed_callback), num_building_dependent_blobs));
  BlobEntry::BuildingState* building_state = entry->building_state_.get();
  building_state->copies = content->ReleaseCopies();
  std::swap(building_state->dependent_blobs, dependent_blobs);
  std::swap(building_state->transport_items, transport_items);
  if (previous_building_state) {
    DCHECK(!previous_building_state->transport_items_present);
    DCHECK(!previous_building_state->transport_allowed_callback);
    DCHECK(previous_building_state->transport_items.empty());
    DCHECK(previous_building_state->dependent_blobs.empty());
    DCHECK(previous_building_state->copies.empty());
    std::swap(building_state->build_completion_callbacks,
              previous_building_state->build_completion_callbacks);
    building_state->build_aborted_callback =
        std::move(previous_building_state->build_aborted_callback);
    auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    for (auto& callback : previous_building_state->build_started_callbacks)
      runner->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), entry->status()));
  }

  // Break ourselves if we have an error. BuildingState must be set first so the
  // callback is called correctly.
  if (BlobStatusIsError(entry->status())) {
    CancelBuildingBlobInternal(entry, entry->status());
    return handle;
  }

  // Avoid the state where we might grant only one quota.
  if (!memory_controller_.CanReserveQuota(content->copy_quota_needed() +
                                          content->transport_quota_needed())) {
    CancelBuildingBlobInternal(entry, BlobStatus::ERR_OUT_OF_MEMORY);
    return handle;
  }

  if (content->copy_quota_needed() > 0) {
    // The blob can complete during the execution of |ReserveMemoryQuota|.
    base::WeakPtr<QuotaAllocationTask> pending_request =
        memory_controller_.ReserveMemoryQuota(
            std::move(pending_copy_items),
            base::BindOnce(&BlobStorageContext::OnEnoughSpaceForCopies,
                           ptr_factory_.GetWeakPtr(), content->uuid_));
    // Building state will be null if the blob is already finished.
    if (entry->building_state_)
      entry->building_state_->copy_quota_request = std::move(pending_request);
  }

  if (content->transport_quota_needed() > 0) {
    base::WeakPtr<QuotaAllocationTask> pending_request;

    switch (transport_quota_type) {
      case TransportQuotaType::MEMORY: {
        // The blob can complete during the execution of |ReserveMemoryQuota|.
        std::vector<BlobMemoryController::FileCreationInfo> empty_files;
        pending_request = memory_controller_.ReserveMemoryQuota(
            content->ReleasePendingTransportItems(),
            base::BindOnce(&BlobStorageContext::OnEnoughSpaceForTransport,
                           ptr_factory_.GetWeakPtr(), content->uuid_,
                           std::move(empty_files)));
        break;
      }
      case TransportQuotaType::FILE:
        pending_request = memory_controller_.ReserveFileQuota(
            content->ReleasePendingTransportItems(),
            base::BindOnce(&BlobStorageContext::OnEnoughSpaceForTransport,
                           ptr_factory_.GetWeakPtr(), content->uuid_));
        break;
    }

    // Building state will be null if the blob is already finished.
    if (entry->building_state_) {
      entry->building_state_->transport_quota_request =
          std::move(pending_request);
    }
  }

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);

  return handle;
}

void BlobStorageContext::CancelBuildingBlob(const std::string& uuid,
                                            BlobStatus reason) {
  CancelBuildingBlobInternal(registry_.GetEntry(uuid), reason);
}

void BlobStorageContext::NotifyTransportComplete(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  CHECK(entry) << "There is no blob entry with uuid " << uuid;
  DCHECK(BlobStatusIsPending(entry->status()));
  NotifyTransportCompleteInternal(entry);
}

void BlobStorageContext::Bind(
    mojo::PendingReceiver<mojom::BlobStorageContext> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void BlobStorageContext::IncrementBlobRefCount(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  entry->IncrementRefCount();
}

void BlobStorageContext::DecrementBlobRefCount(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  DCHECK_GT(entry->refcount(), 0u);
  entry->DecrementRefCount();
  if (entry->refcount() == 0) {
    DVLOG(1) << "BlobStorageContext::DecrementBlobRefCount(" << uuid
             << "): Deleting blob.";
    ClearAndFreeMemory(entry);
    registry_.DeleteEntry(uuid);
  }
}

std::unique_ptr<BlobDataSnapshot> BlobStorageContext::CreateSnapshot(
    const std::string& uuid) {
  std::unique_ptr<BlobDataSnapshot> result;
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (entry->status() != BlobStatus::DONE)
    return result;

  std::unique_ptr<BlobDataSnapshot> snapshot(new BlobDataSnapshot(
      uuid, entry->content_type(), entry->content_disposition()));
  snapshot->items_.reserve(entry->items().size());
  for (const auto& shareable_item : entry->items()) {
    snapshot->items_.push_back(shareable_item->item());
  }
  memory_controller_.NotifyMemoryItemsUsed(entry->items());
  return snapshot;
}

BlobStatus BlobStorageContext::GetBlobStatus(const std::string& uuid) const {
  const BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  return entry->status();
}

void BlobStorageContext::RunOnConstructionComplete(const std::string& uuid,
                                                   BlobStatusCallback done) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  if (BlobStatusIsPending(entry->status())) {
    entry->building_state_->build_completion_callbacks.push_back(
        std::move(done));
    return;
  }
  std::move(done).Run(entry->status());
}

void BlobStorageContext::RunOnConstructionBegin(const std::string& uuid,
                                                BlobStatusCallback done) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  if (entry->status() == BlobStatus::PENDING_CONSTRUCTION) {
    entry->building_state_->build_started_callbacks.push_back(std::move(done));
    return;
  }
  std::move(done).Run(entry->status());
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::CreateHandle(
    const std::string& uuid,
    BlobEntry* entry) {
  return base::WrapUnique(new BlobDataHandle(
      uuid, entry->content_type_, entry->content_disposition_, entry->size_,
      this, base::SingleThreadTaskRunner::GetCurrentDefault().get()));
}

void BlobStorageContext::NotifyTransportCompleteInternal(BlobEntry* entry) {
  DCHECK(entry);
  for (ShareableBlobDataItem* shareable_item :
       entry->building_state_->transport_items) {
    DCHECK(shareable_item->state() == ShareableBlobDataItem::QUOTA_GRANTED);
    shareable_item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  }
  entry->set_status(BlobStatus::PENDING_REFERENCED_BLOBS);
  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::CancelBuildingBlobInternal(BlobEntry* entry,
                                                    BlobStatus reason) {
  DCHECK(entry);
  DCHECK(BlobStatusIsError(reason));
  TransportAllowedCallback transport_allowed_callback;
  if (entry->building_state_ &&
      entry->building_state_->transport_allowed_callback) {
    transport_allowed_callback =
        std::move(entry->building_state_->transport_allowed_callback);
  }
  if (entry->building_state_ &&
      entry->status() == BlobStatus::PENDING_CONSTRUCTION) {
    auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    for (auto& callback : entry->building_state_->build_started_callbacks)
      runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), reason));
  }
  ClearAndFreeMemory(entry);
  entry->set_status(reason);
  if (transport_allowed_callback) {
    std::move(transport_allowed_callback)
        .Run(reason, std::vector<BlobMemoryController::FileCreationInfo>());
  }
  FinishBuilding(entry);
}

void BlobStorageContext::FinishBuilding(BlobEntry* entry) {
  DCHECK(entry);
  BlobStatus status = entry->status_;
  DCHECK_NE(BlobStatus::DONE, status);

  if (BlobStatusIsPending(entry->status_)) {
    for (const ItemCopyEntry& copy : entry->building_state_->copies) {
      // Our source item can be a file if it was a slice of an unpopulated file,
      // or a slice of data that was then paged to disk.
      size_t dest_size = static_cast<size_t>(copy.dest_item->item()->length());
      BlobDataItem::Type dest_type = copy.dest_item->item()->type();
      switch (copy.source_item->item()->type()) {
        case BlobDataItem::Type::kBytes: {
          DCHECK_EQ(dest_type, BlobDataItem::Type::kBytesDescription);
          base::span<const uint8_t> src_data =
              copy.source_item->item()->bytes().subspan(copy.source_item_offset,
                                                        dest_size);
          copy.dest_item->item()->PopulateBytes(src_data);
          break;
        }
        case BlobDataItem::Type::kFile: {
          // If we expected a memory item (and our source was paged to disk) we
          // free that memory.
          if (dest_type == BlobDataItem::Type::kBytesDescription)
            copy.dest_item->set_memory_allocation(nullptr);

          const auto& source_item = copy.source_item->item();
          scoped_refptr<BlobDataItem> new_item = BlobDataItem::CreateFile(
              source_item->path(),
              source_item->offset() + copy.source_item_offset, dest_size,
              source_item->expected_modification_time(), source_item->file_ref_,
              source_item->file_access_);
          copy.dest_item->set_item(std::move(new_item));
          break;
        }
        case BlobDataItem::Type::kBytesDescription:
        case BlobDataItem::Type::kFileFilesystem:
        case BlobDataItem::Type::kReadableDataHandle:
          NOTREACHED();
      }
      copy.dest_item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
    }

    entry->set_status(BlobStatus::DONE);
  }

  std::vector<BlobStatusCallback> callbacks;
  if (entry->building_state_.get()) {
    std::swap(callbacks, entry->building_state_->build_completion_callbacks);
    entry->building_state_.reset();
  }

  memory_controller_.NotifyMemoryItemsUsed(entry->items());

  auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  for (auto& callback : callbacks)
    runner->PostTask(FROM_HERE,
                     base::BindOnce(std::move(callback), entry->status()));

  for (const auto& shareable_item : entry->items()) {
    DCHECK_NE(BlobDataItem::Type::kBytesDescription,
              shareable_item->item()->type());
    DCHECK(shareable_item->IsPopulated()) << shareable_item->state();
  }
}

void BlobStorageContext::RequestTransport(
    BlobEntry* entry,
    std::vector<BlobMemoryController::FileCreationInfo> files) {
  BlobEntry::BuildingState* building_state = entry->building_state_.get();
  if (building_state->transport_allowed_callback) {
    std::move(building_state->transport_allowed_callback)
        .Run(BlobStatus::PENDING_TRANSPORT, std::move(files));
    return;
  }
  DCHECK(files.empty());
  NotifyTransportCompleteInternal(entry);
}

void BlobStorageContext::OnEnoughSpaceForTransport(
    const std::string& uuid,
    std::vector<BlobMemoryController::FileCreationInfo> files,
    bool success) {
  if (!success) {
    CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry || !entry->building_state_.get())
    return;
  BlobEntry::BuildingState& building_state = *entry->building_state_;
  DCHECK(!building_state.transport_quota_request);
  DCHECK(building_state.transport_items_present);

  entry->set_status(BlobStatus::PENDING_TRANSPORT);
  RequestTransport(entry, std::move(files));

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::OnEnoughSpaceForCopies(const std::string& uuid,
                                                bool success) {
  if (!success) {
    CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return;

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::OnDependentBlobFinished(
    const std::string& owning_blob_uuid,
    BlobStatus status) {
  BlobEntry* entry = registry_.GetEntry(owning_blob_uuid);
  if (!entry || !entry->building_state_)
    return;

  if (BlobStatusIsError(status)) {
    DCHECK_NE(BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING, status)
        << "Referenced blob should never be dereferenced while we "
        << "are depending on it, as our system holds a handle.";
    CancelBuildingBlobInternal(entry, BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
    return;
  }
  DCHECK_GT(entry->building_state_->num_building_dependent_blobs, 0u);
  --entry->building_state_->num_building_dependent_blobs;

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::ClearAndFreeMemory(BlobEntry* entry) {
  if (entry->building_state_)
    entry->building_state_->CancelRequestsAndAbort();
  entry->ClearItems();
  entry->ClearOffsets();
  entry->set_size(0);
}

bool BlobStorageContext::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();

  auto* mad = pmd->CreateAllocatorDump(
      base::StringPrintf("site_storage/blob_storage/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                 base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                 memory_controller().memory_usage());
  mad->AddScalar("disk_usage",
                 base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                 memory_controller().disk_usage());
  mad->AddScalar("blob_count",
                 base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                 blob_count());
  if (system_allocator_name)
    pmd->AddSuballocation(mad->guid(), system_allocator_name);
  return true;
}

void BlobStorageContext::RegisterFromDataItem(
    mojo::PendingReceiver<::blink::mojom::Blob> blob,
    const std::string& uuid,
    mojom::BlobDataItemPtr item) {
  if (registry_.HasEntry(uuid)) {
    receivers_.ReportBadMessage("duplicate uuid");
    return;
  }
  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>(uuid);
  if (!item->content_type.empty())
    builder->set_content_type(item->content_type);
  builder->AppendMojoDataItem(std::move(item));
  std::unique_ptr<BlobDataHandle> handle = AddFinishedBlob(std::move(builder));
  BlobImpl::Create(std::move(handle), std::move(blob));
}

void BlobStorageContext::RegisterFromMemory(
    mojo::PendingReceiver<::blink::mojom::Blob> blob,
    const std::string& uuid,
    mojo_base::BigBuffer data) {
  if (registry_.HasEntry(uuid)) {
    receivers_.ReportBadMessage("duplicate uuid");
    return;
  }

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>(uuid);
  builder->AppendData(data);
  std::unique_ptr<BlobDataHandle> handle = AddFinishedBlob(std::move(builder));
  BlobImpl::Create(std::move(handle), std::move(blob));
}

void BlobStorageContext::WriteBlobToFile(
    mojo::PendingRemote<::blink::mojom::Blob> pending_blob,
    const base::FilePath& file_path,
    bool flush_on_write,
    std::optional<base::Time> last_modified,
    BlobStorageContext::WriteBlobToFileCallback callback) {
  DCHECK(!last_modified || !last_modified.value().is_null());
  if (profile_directory_.empty()) {
    std::move(callback).Run(mojom::WriteBlobToFileResult::kBadPath);
    return;
  }
  if (file_path.ReferencesParent()) {
    std::move(callback).Run(mojom::WriteBlobToFileResult::kBadPath);
    return;
  }
  if (!profile_directory_.IsParent(file_path)) {
    std::move(callback).Run(mojom::WriteBlobToFileResult::kBadPath);
    return;
  }

  GetBlobDataFromBlobRemote(
      std::move(pending_blob),
      base::BindOnce(
          [](base::WeakPtr<BlobStorageContext> blob_context,
             const base::FilePath& file_path, bool flush_on_write,
             std::optional<base::Time> last_modified,
             BlobStorageContext::WriteBlobToFileCallback callback,
             std::unique_ptr<BlobDataHandle> handle) {
            if (!handle || !blob_context) {
              std::move(callback).Run(
                  mojom::WriteBlobToFileResult::kInvalidBlob);
              return;
            }
            storage::WriteBlobToFile(std::move(handle), file_path,
                                     flush_on_write, last_modified,
                                     std::move(callback));
          },
          AsWeakPtr(), file_path, flush_on_write, last_modified,
          std::move(callback)));
}

void BlobStorageContext::Clone(
    mojo::PendingReceiver<mojom::BlobStorageContext> receiver) {
  Bind(std::move(receiver));
}

}  // namespace storage
