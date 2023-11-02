// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_system/saved_file_entry.h"

namespace extensions {

SavedFileEntry::SavedFileEntry() : is_directory(false), sequence_number(0) {}

SavedFileEntry::SavedFileEntry(const std::string& id,
                               const base::FilePath& path,
                               bool is_directory,
                               int sequence_number)
    : id(id),
      path(path),
      is_directory(is_directory),
      sequence_number(sequence_number) {}

}  // namespace extensions
