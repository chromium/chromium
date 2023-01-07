// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_
#define STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_

#include <set>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"

namespace storage {

// Interface for handling the WebStorage directory for a profile where
// Storage Buckets data is stored.
class COMPONENT_EXPORT(STORAGE_BROWSER) StorageDirectory {
 public:
  explicit StorageDirectory(const base::FilePath& profile_path);
  StorageDirectory(const StorageDirectory&) = delete;
  StorageDirectory& operator=(const StorageDirectory&) = delete;
  ~StorageDirectory();

  // Creates storage directory and returns true if creation succeeds or
  // directory already exists.
  bool Create();

  // Marks the current storage directory for deletion and returns true on
  // success.
  bool Doom();

  // Deletes doomed storage directories.
  void ClearDoomed();

  // Create storage directory for `bucket` under `web_storage_path_`. Returns
  // true if creation succeeds or directory already exists.
  bool CreateBucket(const BucketLocator& bucket);

  // Finds and marks the storage directory for `bucket` and marks it for
  // deletion. Returns true on success.
  bool DoomBucket(const BucketLocator& bucket);

  // Deletes doomed bucket directories found under `web_storage_path_`.
  void ClearDoomedBuckets();

  // Returns path where WebStorage data is persisted to disk. Returns empty path
  // for incognito.
  const base::FilePath& path() const { return web_storage_path_; }

  std::set<base::FilePath> EnumerateDoomedDirectoriesForTesting() {
    return EnumerateDoomedDirectories();
  }

  std::set<base::FilePath> EnumerateDoomedBucketsForTesting() {
    return EnumerateDoomedBuckets();
  }

 private:
  std::set<base::FilePath> EnumerateDoomedDirectories();
  std::set<base::FilePath> EnumerateDoomedBuckets();

  bool DoomPath(const base::FilePath& path);

  const base::FilePath web_storage_path_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_
