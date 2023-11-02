// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "storage/browser/blob/blob_storage_constants.h"

namespace storage {
class BlobEntry;

// This class stores the blob data in the various states of construction.
// Implementation notes:
// * When removing a uuid registration, we do not check for URL mappings to that
//   uuid. The user must keep track of these.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobStorageRegistry {
 public:
  BlobStorageRegistry();

  BlobStorageRegistry(const BlobStorageRegistry&) = delete;
  BlobStorageRegistry& operator=(const BlobStorageRegistry&) = delete;

  ~BlobStorageRegistry();

  // Creates the blob entry with a refcount of 1 and a state of PENDING. If
  // the blob is already in use, we return null.
  BlobEntry* CreateEntry(const std::string& uuid,
                         const std::string& content_type,
                         const std::string& content_disposition);

  // Removes the blob entry with the given uuid. This does not unmap any
  // URLs that are pointing to this uuid. Returns if the entry existed.
  bool DeleteEntry(const std::string& uuid);

  bool HasEntry(const std::string& uuid) const;

  // Gets the blob entry for the given uuid. Returns nullptr if the entry
  // does not exist.
  BlobEntry* GetEntry(const std::string& uuid);
  const BlobEntry* GetEntry(const std::string& uuid) const;

  size_t blob_count() const { return blob_map_.size(); }

 private:
  friend class ViewBlobInternalsJob;

  std::unordered_map<std::string, std::unique_ptr<BlobEntry>> blob_map_;
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
