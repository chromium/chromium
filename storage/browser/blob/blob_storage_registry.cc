// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
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

scoped_refptr<BlobEntry> BlobStorageRegistry::CreateEntry(
    const std::string& uuid,
    const std::string& content_type) {
  auto entry = base::MakeRefCounted<BlobEntry>(content_type);
  auto [_, inserted] = blob_map_.insert(std::make_pair(uuid, entry));
  CHECK(inserted);
  return entry;
}

void BlobStorageRegistry::ReleaseEntry(const std::string& uuid) {
  blob_map_.erase(uuid);
}

bool BlobStorageRegistry::HasEntry(const std::string& uuid) const {
  return blob_map_.contains(uuid);
}

scoped_refptr<BlobEntry> BlobStorageRegistry::GetEntry(
    const std::string& uuid) {
  auto found = blob_map_.find(uuid);
  if (found == blob_map_.end()) {
    return nullptr;
  }
  return found->second;
}

scoped_refptr<const BlobEntry> BlobStorageRegistry::GetEntry(
    const std::string& uuid) const {
  return const_cast<BlobStorageRegistry*>(this)->GetEntry(uuid);
}

}  // namespace storage
