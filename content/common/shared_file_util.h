// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SHARED_FILE_UTIL_H_
#define CONTENT_COMMON_SHARED_FILE_UTIL_H_

#include <map>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/component_export.h"

namespace content {

// Populates the global instance of base::FileDescriptorStore using the
// information from the command line, assuming base::GlobalDescriptors has been
// initialized to hold the dynamically-generated descriptors (e.g. as happens in
// the zygote).
void PopulateFileDescriptorStoreFromGlobalDescriptors();

// Similar to PopulateFileDescriptorStoreFromGlobalDescriptors(), this will
// populate the global instance of base::FileDescriptorStore, but takes the FDs
// directly from the FD table, using the default FD numbers (i.e. descriptor_id
// + base::GlobalDescriptors::kBaseDescriptor). On Posix systems, exec'd
// processes should use this instead of
// PopulateFileDescriptorStoreFromGlobalDescriptors().
void PopulateFileDescriptorStoreFromFdTable();

class SharedFileSwitchValueBuilder final {
 public:
  void AddEntry(const std::string& key_str, int key_id);
  const std::string& switch_value() const { return switch_value_; }

 private:
  std::string switch_value_;
};

std::optional<std::map<int, std::string>> ParseSharedFileSwitchValue(
    const std::string& value);

}  // namespace content

#endif  // CONTENT_COMMON_SHARED_FILE_UTIL_H_
