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
#include "base/memory/scoped_refptr.h"
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

  // Creates the blob entry with a handle count of 0 and a state of PENDING.
  // It's an error to supply an already-used UUID.
  scoped_refptr<BlobEntry> CreateEntry(const std::string& uuid,
                                       const std::string& content_type);

  // Removes the blob entry with the given uuid. This does not unmap any
  // URLs that are pointing to this uuid.
  void ReleaseEntry(const std::string& uuid);

  bool HasEntry(const std::string& uuid) const;

  // Gets the blob entry for the given uuid. Returns nullptr if the entry
  // does not exist.
  scoped_refptr<BlobEntry> GetEntry(const std::string& uuid);
  scoped_refptr<const BlobEntry> GetEntry(const std::string& uuid) const;

  size_t blob_count() const { return blob_map_.size(); }

 private:
  friend class ViewBlobInternalsJob;

  // The `BlobEntry`s are ref counted because many different error conditions
  // can lead to dropping the last `BlobDataHandle` for an entry, leading to
  // `ReleaseEntry` being called. This frequently happens in the middle of
  // operating on said `BlobEntry`. See crbug.com/572193211
  std::unordered_map<std::string, scoped_refptr<BlobEntry>> blob_map_;
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
