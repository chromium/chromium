// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_icon_set.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"

ExtensionIconSet::ExtensionIconSet() {}

ExtensionIconSet::ExtensionIconSet(const ExtensionIconSet& other) = default;

ExtensionIconSet::~ExtensionIconSet() {}

void ExtensionIconSet::Clear() {
  map_.clear();
}

void ExtensionIconSet::Add(int size_in_px, const std::string& path) {
  DCHECK(!path.empty() && path[0] != '/');
  map_[size_in_px] = path;
}

const std::string& ExtensionIconSet::Get(int size_in_px,
                                         MatchType match_type) const {
  // The searches for MATCH_BIGGER and MATCH_SMALLER below rely on the fact that
  // std::map is sorted. This is per the spec, so it should be safe to rely on.
  if (match_type == MATCH_EXACTLY) {
    auto result = map_.find(size_in_px);
    return result == map_.end() ? base::EmptyString() : result->second;
  }
  if (match_type == MATCH_SMALLER) {
    auto result = map_.rend();
    for (auto iter = map_.rbegin(); iter != map_.rend(); ++iter) {
      if (iter->first <= size_in_px) {
        result = iter;
        break;
      }
    }
    return result == map_.rend() ? base::EmptyString() : result->second;
  }

  DCHECK(match_type == MATCH_BIGGER);
  auto result = map_.cend();
  for (auto iter = map_.cbegin(); iter != map_.cend(); ++iter) {
    if (iter->first >= size_in_px) {
      result = iter;
      break;
    }
  }
  return result == map_.cend() ? base::EmptyString() : result->second;
}

bool ExtensionIconSet::ContainsPath(base::StringPiece path) const {
  return GetIconSizeFromPath(path) != 0;
}

int ExtensionIconSet::GetIconSizeFromPath(base::StringPiece path) const {
  if (path.empty())
    return 0;

  DCHECK_NE(path[0], '/') <<
      "ExtensionIconSet stores icon paths without leading slash.";

  for (auto iter = map_.cbegin(); iter != map_.cend(); ++iter) {
    if (iter->second == path)
      return iter->first;
  }

  return 0;
}

void ExtensionIconSet::GetPaths(std::set<base::FilePath>* paths) const {
  CHECK(paths);
  for (const auto& iter : map())
    paths->insert(base::FilePath::FromUTF8Unsafe(iter.second));
}
