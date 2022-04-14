// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_
#define STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_

#include <set>

#include "base/component_export.h"
#include "base/files/file_path.h"

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

  // Returns path where WebStorage data is persisted to disk. Returns empty path
  // for incognito.
  const base::FilePath& path() const { return web_storage_path_; }

  std::set<base::FilePath> EnumerateDoomedDirectoriesForTesting() {
    return EnumerateDoomedDirectories();
  }

 private:
  std::set<base::FilePath> EnumerateDoomedDirectories();

  const base::FilePath web_storage_path_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_STORAGE_DIRECTORY_H_
