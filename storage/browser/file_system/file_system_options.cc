// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_options.h"

namespace storage {

FileSystemOptions::FileSystemOptions(
    ProfileMode profile_mode,
    bool force_in_memory,
    const std::vector<std::string>& additional_allowed_schemes)
    : profile_mode_(profile_mode),
      force_in_memory_(force_in_memory),
      additional_allowed_schemes_(additional_allowed_schemes) {}

FileSystemOptions::FileSystemOptions(const FileSystemOptions& other) = default;

FileSystemOptions::~FileSystemOptions() = default;

bool FileSystemOptions::is_in_memory() const {
  return force_in_memory_ || is_incognito();
}

}  // namespace storage
