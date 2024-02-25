// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_memory_controller.h"

namespace storage {
class BlobDataHandle;
class ShareableBlobDataItem;

// Represents a blob in BlobStorageRegistry. Exported only for unit tests.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobEntry {
 public:
  using TransportAllowedCallback = base::OnceCallback<
      void(BlobStatus, std::vector<BlobMemoryController::FileCreationInfo>)>;
  using BuildAbortedCallback = base::OnceClosure;

  // Records a copy from a referenced blob. Copies happen after referenced blobs
  // are complete & quota for the copies is granted.
  struct COMPONENT_EXPORT(STORAGE_BROWSER) ItemCopyEntry {
    ItemCopyEntry(scoped_refptr<ShareableBlobDataItem> source_item,
                  size_t source_item_offset,
                  scoped_refptr<ShareableBlobDataItem> dest_item);

    ItemCopyEntry(const ItemCopyEntry&) = delete;
    ItemCopyEntry& operator=(const ItemCopyEntry&) = delete;

    ~ItemCopyEntry();
    ItemCopyEntry(ItemCopyEntry&& other);
    BlobEntry::ItemCopyEntry& operator=(BlobEntry::ItemCopyEntry&& rhs);

    scoped_refptr<ShareableBlobDataItem> source_item;
    size_t source_item_offset = 0;
    scoped_refptr<ShareableBlobDataItem> dest_item;
  };

  // Building state for pending blobs. State can include:
  // 1. Waiting for quota to be granted for transport data (PENDING_QUOTA)
  // 2. Waiting for user population of data after quota (PENDING_TRANSPORT)
  // 3. Waiting for blobs we reference to complete & quota granted for possible
  //    copies. (PENDING_REFERENCED_BLOBS)
  struct COMPONENT_EXPORT(STORAGE_BROWSER) BuildingState {
    // |transport_allowed_callback| is not null when data needs population. See
    // BlobStorageContext::BuildBlob for when the callback is called.
    BuildingState(bool transport_items_present,
                  TransportAllowedCallback transport_allowed_callback,
                  size_t num_building_dependent_blobs);

    BuildingState(const BuildingState&) = delete;
    BuildingState& operator=(const BuildingState&) = delete;

    ~BuildingState();

    // Cancels pending memory or file requests, and calls aborted callback if it
    // is set.
    void CancelRequestsAndAbort();

    const bool transport_items_present;
    // We can have trasnport data that's either populated or unpopulated. If we
    // need population, this is populated.
    TransportAllowedCallback transport_allowed_callback;
    std::vector<raw_ptr<ShareableBlobDataItem, VectorExperimental>>
        transport_items;

    BuildAbortedCallback build_aborted_callback;

    // Stores all blobs that we're depending on for building. This keeps the
    // blobs alive while we build our blob.
    std::vector<std::unique_ptr<BlobDataHandle>> dependent_blobs;
    size_t num_building_dependent_blobs;

    base::WeakPtr<BlobMemoryController::QuotaAllocationTask>
        transport_quota_request;

    // Copy quota is always memory.
    base::WeakPtr<BlobMemoryController::QuotaAllocationTask> copy_quota_request;

    // These are copies from a referenced blob item to our blob items. Some of
    // these entries may have changed from bytes to files if they were paged.
    std::vector<ItemCopyEntry> copies;

    // When our blob finishes building these callbacks are called.
    std::vector<BlobStatusCallback> build_completion_callbacks;

    // When our blob is no longer in PENDING_CONSTRUCTION state these callbacks
    // are called.
    std::vector<BlobStatusCallback> build_started_callbacks;
  };

  BlobEntry(const std::string& content_type,
            const std::string& content_disposition);

  BlobEntry(const BlobEntry&) = delete;
  BlobEntry& operator=(const BlobEntry&) = delete;

  ~BlobEntry();

  // Appends the given shared blob data item to this object.
  void AppendSharedBlobItem(scoped_refptr<ShareableBlobDataItem> item);
  void SetSharedBlobItems(
      std::vector<scoped_refptr<ShareableBlobDataItem>> items);

  // Returns if we're a pending blob that can finish building.
  bool CanFinishBuilding() const {
    // PENDING_REFERENCED_BLOBS means transport is finished.
    return status_ == BlobStatus::PENDING_REFERENCED_BLOBS && building_state_ &&
           !building_state_->copy_quota_request &&
           building_state_->num_building_dependent_blobs == 0;
  }

  BlobStatus status() const { return status_; }

  size_t refcount() const { return refcount_; }

  const std::string& content_type() const { return content_type_; }

  const std::string& content_disposition() const {
    return content_disposition_;
  }

  // Total size of this blob in bytes.
  uint64_t total_size() const { return size_; }

  // We record the cumulative offsets of each blob item (except for the first,
  // which would always be 0) to enable binary searching for an item at a
  // specific byte offset.
  const std::vector<uint64_t>& offsets() const { return offsets_; }

  const std::vector<scoped_refptr<ShareableBlobDataItem>>& items() const;

 protected:
  friend class BlobStorageContext;

  void IncrementRefCount() { ++refcount_; }
  void DecrementRefCount() { --refcount_; }

  void set_status(BlobStatus status) { status_ = status; }
  void set_size(uint64_t size) { size_ = size; }

  void ClearItems();
  void ClearOffsets();

  void set_building_state(std::unique_ptr<BuildingState> building_state) {
    building_state_ = std::move(building_state);
  }

 private:
  BlobStatus status_ = BlobStatus::PENDING_QUOTA;
  size_t refcount_ = 0;

  // Metadata.
  std::string content_type_;
  std::string content_disposition_;

  std::vector<scoped_refptr<ShareableBlobDataItem>> items_;

  // Size in bytes. IFF we're a single file then this can be uint64_max.
  uint64_t size_ = 0;

  // Only populated if len(items_) > 1.  Used for binary search.
  // Since the offset of the first item is always 0, we exclude this.
  std::vector<uint64_t> offsets_;

  // Only populated if our status is PENDING_*.
  std::unique_ptr<BuildingState> building_state_;
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_
