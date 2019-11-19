// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/component_export.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/blob_memory_controller.h"

namespace storage {
class BlobDataItem;

// This class allows blob items to be shared between blobs.  This class contains
// both the blob data item and the uuids of all the blobs using this item.
// The data in this class (the item) is immutable, but the item itself can be
// swapped out with an item with the same data but a different backing (think
// RAM vs file backed).
// We also allow the storage of a memory quota allocation object which is used
// for memory quota reclamation.
class COMPONENT_EXPORT(STORAGE_BROWSER) ShareableBlobDataItem
    : public base::RefCounted<ShareableBlobDataItem> {
 public:
  enum State {
    // We're an item that needs quota (either disk or memory).
    QUOTA_NEEDED,
    // We have requested quota from the BlobMemoryController.
    QUOTA_REQUESTED,
    // Space has been allocated for this item in the BlobMemoryController, but
    // it may not yet be populated.
    QUOTA_GRANTED,
    // We're a populated item that needed quota.
    POPULATED_WITH_QUOTA,
    // We're a populated item that didn't need quota.
    POPULATED_WITHOUT_QUOTA
  };

  ShareableBlobDataItem(scoped_refptr<BlobDataItem> item, State state);

  const scoped_refptr<BlobDataItem>& item() const { return item_; }

  void set_item(scoped_refptr<BlobDataItem> item);

  // This is a unique auto-incrementing id assigned to this item on
  // construction. It is used to keep track of this item in an LRU data
  // structure for eviction to disk.
  uint64_t item_id() const { return item_id_; }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  bool IsPopulated() const {
    return state_ == POPULATED_WITH_QUOTA || state_ == POPULATED_WITHOUT_QUOTA;
  }

  bool HasGrantedQuota() const {
    return state_ == POPULATED_WITH_QUOTA || state_ == QUOTA_GRANTED;
  }

 private:
  friend class BlobMemoryController;
  friend class BlobMemoryControllerTest;
  friend class BlobStorageContext;
  friend class base::RefCounted<ShareableBlobDataItem>;
  friend COMPONENT_EXPORT(STORAGE_BROWSER) void PrintTo(
      const ShareableBlobDataItem& x,
      ::std::ostream* os);

  ~ShareableBlobDataItem();

  void set_memory_allocation(
      std::unique_ptr<BlobMemoryController::MemoryAllocation> allocation) {
    memory_allocation_ = std::move(allocation);
  }

  bool has_memory_allocation() { return static_cast<bool>(memory_allocation_); }

  // This is a unique identifier for this ShareableBlobDataItem.
  const uint64_t item_id_;
  State state_;
  scoped_refptr<BlobDataItem> item_;
  std::unique_ptr<BlobMemoryController::MemoryAllocation> memory_allocation_;

  DISALLOW_COPY_AND_ASSIGN(ShareableBlobDataItem);
};

COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator==(const ShareableBlobDataItem& a, const ShareableBlobDataItem& b);
COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator!=(const ShareableBlobDataItem& a, const ShareableBlobDataItem& b);

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_
