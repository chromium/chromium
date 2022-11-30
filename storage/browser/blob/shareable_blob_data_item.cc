// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/shareable_blob_data_item.h"

#include "storage/browser/blob/blob_data_item.h"

namespace storage {
namespace {

uint64_t GetAndIncrementItemId() {
  static uint64_t sNextItemId = 0;
  return sNextItemId++;
}

}  // namespace

ShareableBlobDataItem::ShareableBlobDataItem(
    scoped_refptr<BlobDataItem> item,
    ShareableBlobDataItem::State state)
    : item_id_(GetAndIncrementItemId()), state_(state), item_(std::move(item)) {
}

ShareableBlobDataItem::~ShareableBlobDataItem() = default;

void ShareableBlobDataItem::set_item(scoped_refptr<BlobDataItem> item) {
  item_ = std::move(item);
}

void PrintTo(const ShareableBlobDataItem& x, ::std::ostream* os) {
  *os << "<ShareableBlobDataItem>{ item_id: " << x.item_id_
      << ", state: " << x.state_ << ", item: ";
  PrintTo(*x.item_, os);
  *os << "]}";
}

bool operator==(const ShareableBlobDataItem& a,
                const ShareableBlobDataItem& b) {
  return a.item_id() == b.item_id() && *a.item() == *b.item();
}

bool operator!=(const ShareableBlobDataItem& a,
                const ShareableBlobDataItem& b) {
  return !(a == b);
}

}  // namespace storage
