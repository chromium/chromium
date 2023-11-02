// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPTIONS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPTIONS_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace storage {

// Provides runtime options that may change FileSystem API behavior.
// This object is copyable.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemOptions {
 public:
  enum ProfileMode { PROFILE_MODE_NORMAL = 0, PROFILE_MODE_INCOGNITO };

  // |profile_mode| specifies if the profile (for this filesystem)
  // is running in incognito mode (PROFILE_MODE_INCOGNITO) or no
  // (PROFILE_MODE_NORMAL).
  // A FileSystem will be created in-memory when |profile_mode| is incognito,
  // but it can be forced to be in-memory by setting |force_in_memory| to
  // true - this is only to support testing.
  // |additional_allowed_schemes| specifies schemes that are allowed
  // to access FileSystem API in addition to "http" and "https".
  FileSystemOptions(ProfileMode profile_mode,
                    bool force_in_memory,
                    const std::vector<std::string>& additional_allowed_schemes);
  FileSystemOptions(const FileSystemOptions& other);

  ~FileSystemOptions();

  // Returns true if it is running in the incognito mode.
  bool is_incognito() const { return profile_mode_ == PROFILE_MODE_INCOGNITO; }

  // Returns true if filesystem is in-memory.
  bool is_in_memory() const;

  // Returns the schemes that must be allowed to access FileSystem API
  // in addition to standard "http" and "https".
  // (e.g. If the --allow-file-access-from-files option is given in chrome
  // "file" scheme will also need to be allowed).
  const std::vector<std::string>& additional_allowed_schemes() const {
    return additional_allowed_schemes_;
  }

 private:
  const ProfileMode profile_mode_;
  const bool force_in_memory_;
  const std::vector<std::string> additional_allowed_schemes_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPTIONS_H_
