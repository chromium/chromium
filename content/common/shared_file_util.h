// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SHARED_FILE_UTIL_H_
#define CONTENT_COMMON_SHARED_FILE_UTIL_H_

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class SharedFileSwitchValueBuilder final {
 public:
  void AddEntry(const std::string& key_str, int key_id);
  const std::string& switch_value() const { return switch_value_; }

 private:
  std::string switch_value_;
};

// Populates base::FileDescriptorStore using the information from the command
// line.
void PopulateFileDescriptorStoreFromGlobalDescriptors();

absl::optional<std::map<int, std::string>> ParseSharedFileSwitchValue(
    const std::string& value);

}  // namespace content

#endif  // CONTENT_COMMON_SHARED_FILE_UTIL_H_
