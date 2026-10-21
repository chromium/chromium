// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_entry.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram.h"
#include "base/task/sequenced_task_runner.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/shareable_blob_data_item.h"

namespace storage {

BlobEntry::ItemCopyEntry::ItemCopyEntry(
    scoped_refptr<ShareableBlobDataItem> source_item,
    size_t source_item_offset,
    scoped_refptr<ShareableBlobDataItem> dest_item)
    : source_item(std::move(source_item)),
      source_item_offset(source_item_offset),
      dest_item(std::move(dest_item)) {}

BlobEntry::ItemCopyEntry::ItemCopyEntry(ItemCopyEntry&& other) = default;
BlobEntry::ItemCopyEntry& BlobEntry::ItemCopyEntry::operator=(
    BlobEntry::ItemCopyEntry&& rhs) = default;
BlobEntry::ItemCopyEntry::~ItemCopyEntry() = default;

BlobEntry::BuildingState::BuildingState(
    bool transport_items_present,
    TransportAllowedCallback transport_allowed_callback,
    size_t num_building_dependent_blobs)
    : transport_items_present(transport_items_present),
      transport_allowed_callback(std::move(transport_allowed_callback)),
      num_building_dependent_blobs(num_building_dependent_blobs) {}

BlobEntry::BuildingState::~BuildingState() {
  CancelRequestsAndAbort();
}

void BlobEntry::BuildingState::CancelRequestsAndAbort() {
  if (copy_quota_request) {
    copy_quota_request->Cancel();
  }
  if (transport_quota_request) {
    transport_quota_request->Cancel();
  }
  if (build_aborted_callback) {
    std::move(build_aborted_callback).Run();
  }
}

BlobEntry::BlobEntry(const std::string& content_type)
    : content_type_(content_type) {}
BlobEntry::~BlobEntry() = default;

void BlobEntry::AppendSharedBlobItem(
    scoped_refptr<ShareableBlobDataItem> item) {
  DCHECK(item);
  if (!items_.empty()) {
    offsets_.push_back(size_);
  }
  size_ += item->item()->length();
  items_.push_back(std::move(item));
}

void BlobEntry::SetSharedBlobItems(
    std::vector<scoped_refptr<ShareableBlobDataItem>> items) {
  DCHECK(items_.empty());
  DCHECK(offsets_.empty());
  DCHECK_EQ(size_, 0u);

  items_ = std::move(items);
  offsets_.reserve(items_.size());
  for (const auto& item : items_) {
    size_ += item->item()->length();
    offsets_.emplace_back(size_);
  }
  // The loop above pushed one too many offset onto offsets_, so remove the
  // last one.
  if (!offsets_.empty())
    offsets_.pop_back();
}

const std::vector<scoped_refptr<ShareableBlobDataItem>>& BlobEntry::items()
    const {
  return items_;
}

void BlobEntry::OnCancelBuilding(BlobStatus reason) {
  TransportAllowedCallback transport_allowed;

  if (building_state_) {
    transport_allowed = std::move(building_state_->transport_allowed_callback);

    if (status_ == BlobStatus::PENDING_CONSTRUCTION) {
      auto runner = base::SequencedTaskRunner::GetCurrentDefault();
      for (auto& callback : building_state_->build_started_callbacks) {
        runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), reason));
      }
    }

    building_state_->CancelRequestsAndAbort();
    building_state_->transport_items.clear();
  }

  items_.clear();
  offsets_.clear();
  size_ = 0;
  status_ = reason;

  if (transport_allowed) {
    std::move(transport_allowed)
        .Run(reason, std::vector<BlobMemoryController::FileCreationInfo>());
  }
}

}  // namespace storage
