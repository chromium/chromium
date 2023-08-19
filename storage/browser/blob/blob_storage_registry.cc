// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "storage/browser/blob/blob_entry.h"

namespace storage {

BlobStorageRegistry::BlobStorageRegistry() = default;

BlobStorageRegistry::~BlobStorageRegistry() {
  // Note: We don't bother calling the construction complete callbacks, as we
  // are only being destructed at the end of the life of the browser process.
  // So it shouldn't matter.
}

BlobEntry* BlobStorageRegistry::CreateEntry(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition) {
  DCHECK(!base::Contains(blob_map_, uuid));
  std::unique_ptr<BlobEntry> entry =
      std::make_unique<BlobEntry>(content_type, content_disposition);
  BlobEntry* entry_ptr = entry.get();
  blob_map_[uuid] = std::move(entry);
  return entry_ptr;
}

bool BlobStorageRegistry::DeleteEntry(const std::string& uuid) {
  return blob_map_.erase(uuid) == 1;
}

bool BlobStorageRegistry::HasEntry(const std::string& uuid) const {
  return base::Contains(blob_map_, uuid);
}

BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) {
  auto found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return nullptr;
  return found->second.get();
}

const BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) const {
  return const_cast<BlobStorageRegistry*>(this)->GetEntry(uuid);
}

}  // namespace storage
