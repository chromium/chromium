// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "storage/browser/blob/blob_data_snapshot.h"

namespace storage {

BlobDataSnapshot::BlobDataSnapshot(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    const std::vector<scoped_refptr<BlobDataItem>>& items)
    : uuid_(uuid),
      content_type_(content_type),
      content_disposition_(content_disposition),
      items_(items) {
}

BlobDataSnapshot::BlobDataSnapshot(const std::string& uuid,
                                   const std::string& content_type,
                                   const std::string& content_disposition)
    : uuid_(uuid),
      content_type_(content_type),
      content_disposition_(content_disposition) {
}

BlobDataSnapshot::BlobDataSnapshot(const BlobDataSnapshot& other)
    : uuid_(other.uuid_),
      content_type_(other.content_type_),
      content_disposition_(other.content_disposition_),
      items_(other.items_) {
}

BlobDataSnapshot::~BlobDataSnapshot() = default;

size_t BlobDataSnapshot::GetMemoryUsage() const {
  int64_t memory = 0;
  for (const auto& data_item : items_) {
    if (data_item->type() == BlobDataItem::Type::kBytes)
      memory += data_item->length();
  }
  return memory;
}

void PrintTo(const BlobDataSnapshot& x, std::ostream* os) {
  DCHECK(os);
  *os << "<BlobDataSnapshot>{uuid: " << x.uuid()
      << ", content_type: " << x.content_type_
      << ", content_disposition: " << x.content_disposition_ << ", items: [";
  for (const auto& item : x.items_) {
    PrintTo(*item, os);
    *os << ", ";
  }
  *os << "]}";
}

}  // namespace storage
