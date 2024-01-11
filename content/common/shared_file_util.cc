// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/shared_file_util.h"

#include "base/file_descriptor_store.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {
void PopulateFDsFromCommandLine(bool use_global_descriptors) {
  const std::string& shared_file_param =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);
  if (shared_file_param.empty()) {
    return;
  }

  std::optional<std::map<int, std::string>> shared_file_descriptors =
      ParseSharedFileSwitchValue(shared_file_param);
  if (!shared_file_descriptors.has_value()) {
    return;
  }

  for (const auto& descriptor : *shared_file_descriptors) {
    base::MemoryMappedFile::Region region;
    const std::string& key = descriptor.second;
    base::ScopedFD fd;
    if (use_global_descriptors) {
      fd = base::GlobalDescriptors::GetInstance()->TakeFD(descriptor.first,
                                                          &region);
    } else {
      DCHECK_EQ(
          base::GlobalDescriptors::GetInstance()->MaybeGet(descriptor.first),
          -1);
      fd.reset(descriptor.first + base::GlobalDescriptors::kBaseDescriptor);
      region = base::MemoryMappedFile::Region::kWholeFile;
    }
    base::FileDescriptorStore::GetInstance().Set(key, std::move(fd), region);
  }
}
}  // namespace

void PopulateFileDescriptorStoreFromGlobalDescriptors() {
  PopulateFDsFromCommandLine(/*use_global_descriptors=*/true);
}

void PopulateFileDescriptorStoreFromFdTable() {
  PopulateFDsFromCommandLine(/*use_global_descriptors=*/false);
}

void SharedFileSwitchValueBuilder::AddEntry(const std::string& key_str,
                                            int key_id) {
  if (!switch_value_.empty()) {
    switch_value_ += ",";
  }
  switch_value_ += key_str;
  switch_value_ += ":";
  switch_value_ += base::NumberToString(key_id);
}

std::optional<std::map<int, std::string>> ParseSharedFileSwitchValue(
    const std::string& value) {
  std::map<int, std::string> values;
  std::vector<std::string> string_pairs = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& pair : string_pairs) {
    size_t colon_position = pair.find(":");
    if (colon_position == std::string::npos || colon_position == 0 ||
        colon_position == pair.size() - 1) {
      DLOG(ERROR) << "Found invalid entry parsing shared file string value:"
                  << pair;
      return std::nullopt;
    }
    std::string key = pair.substr(0, colon_position);
    std::string number_string =
        pair.substr(colon_position + 1, std::string::npos);
    int key_int;
    if (!base::StringToInt(number_string, &key_int)) {
      DLOG(ERROR) << "Found invalid entry parsing shared file string value:"
                  << number_string << " (not an int).";
      return std::nullopt;
    }

    values[key_int] = key;
  }
  return std::make_optional(std::move(values));
}

}  // namespace content
