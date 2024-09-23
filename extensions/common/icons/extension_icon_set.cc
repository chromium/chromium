// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_set.h"

#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"

ExtensionIconSet::ExtensionIconSet() = default;

ExtensionIconSet::ExtensionIconSet(const ExtensionIconSet& other) = default;

ExtensionIconSet::~ExtensionIconSet() = default;

void ExtensionIconSet::Clear() {
  map_.clear();
}

void ExtensionIconSet::Add(int size_in_px, const std::string& path) {
  DCHECK(!path.empty() && path[0] != '/');
  map_[size_in_px] = path;
}

const std::string& ExtensionIconSet::Get(int size_in_px,
                                         Match match_type) const {
  // The searches for kMatchBigger and kSmaller below rely on the fact that
  // std::map is sorted. This is per the spec, so it should be safe to rely on.
  if (match_type == Match::kExactly) {
    auto result = map_.find(size_in_px);
    return result == map_.end() ? base::EmptyString() : result->second;
  }
  if (match_type == Match::kSmaller) {
    auto result = map_.rend();
    for (auto iter = map_.rbegin(); iter != map_.rend(); ++iter) {
      if (iter->first <= size_in_px) {
        result = iter;
        break;
      }
    }
    return result == map_.rend() ? base::EmptyString() : result->second;
  }

  DCHECK(match_type == Match::kBigger);
  auto result = map_.cend();
  for (auto iter = map_.cbegin(); iter != map_.cend(); ++iter) {
    if (iter->first >= size_in_px) {
      result = iter;
      break;
    }
  }
  return result == map_.cend() ? base::EmptyString() : result->second;
}

bool ExtensionIconSet::ContainsPath(std::string_view path) const {
  return GetIconSizeFromPath(path) != 0;
}

int ExtensionIconSet::GetIconSizeFromPath(std::string_view path) const {
  if (path.empty())
    return 0;

  DCHECK_NE(path[0], '/') <<
      "ExtensionIconSet stores icon paths without leading slash.";

  for (const auto& entry : map_) {
    if (entry.second == path) {
      return entry.first;
    }
  }

  return 0;
}

void ExtensionIconSet::GetPaths(std::set<base::FilePath>* paths) const {
  CHECK(paths);
  for (const auto& iter : map())
    paths->insert(base::FilePath::FromUTF8Unsafe(iter.second));
}
