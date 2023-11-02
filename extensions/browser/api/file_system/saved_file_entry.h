// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILE_ENTRY_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILE_ENTRY_H_

#include <string>

#include "base/files/file_path.h"

namespace extensions {

// Represents a file entry that a user has given an app permission to
// access. Must be serializable for persisting to disk.
struct SavedFileEntry {
  SavedFileEntry();

  SavedFileEntry(const std::string& id,
                 const base::FilePath& path,
                 bool is_directory,
                 int sequence_number);

  // The opaque id of this file entry.
  std::string id;

  // The path to a file entry that the app had permission to access.
  base::FilePath path;

  // Whether or not the entry refers to a directory.
  bool is_directory;

  // The sequence number in the LRU of the file entry. The value 0 indicates
  // that the entry is not in the LRU.
  int sequence_number;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_SAVED_FILE_ENTRY_H_
