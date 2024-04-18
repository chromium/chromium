// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/tools/strings/grit_header_parsing.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

bool FillResourcesFromGritHeader(const base::FilePath& header,
                                 ResourceMap& resource_map) {
  std::string content;
  if (!base::ReadFileToString(header, &content)) {
    const base::File::Error error = base::File::GetLastFileError();
    fprintf(stderr, "ERROR: error loading header %s: %s\n",
            header.value().c_str(), base::File::ErrorToString(error).c_str());
    return false;
  }

  std::vector<std::string_view> lines = base::SplitStringPiece(
      content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (std::string_view line : lines) {
    if (!base::StartsWith(line, "#define "))
      continue;

    std::vector<std::string_view> items = base::SplitStringPiece(
        line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (items.size() != 3) {
      fprintf(stderr, "ERROR: header %s contains invalid entry: %s\n",
              header.value().c_str(), std::string(line).c_str());
      return false;
    }

    const std::string_view key = items[1];
    if (base::Contains(resource_map, key)) {
      fprintf(stderr, "ERROR: entry duplicated in parsed headers: %s\n",
              std::string(key).c_str());
      return false;
    }

    int value = 0;
    const std::string_view val = items[2];
    if (!base::StringToInt(val, &value)) {
      fprintf(stderr, "ERROR: header %s contains invalid entry: %s\n",
              header.value().c_str(), std::string(line).c_str());
      return false;
    }

    resource_map.emplace(key, value);
  }

  return true;
}

}  // namespace

std::optional<ResourceMap> LoadResourcesFromGritHeaders(
    const std::vector<base::FilePath>& headers) {
  ResourceMap resource_map;
  for (const base::FilePath& header : headers) {
    if (!FillResourcesFromGritHeader(header, resource_map)) {
      return std::nullopt;
    }
  }
  return resource_map;
}
