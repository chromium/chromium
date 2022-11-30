// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_resource_path_normalizer.h"

#include <utility>
#include <vector>

#include "base/check.h"

bool NormalizeExtensionResourcePath(const base::FilePath& path,
                                    base::FilePath* result) {
  DCHECK(result);

  if (path.ReferencesParent())
    return false;

  base::FilePath rv;
  for (const auto& path_component : path.GetComponents()) {
    if (path_component != base::FilePath::kCurrentDirectory)
      rv = rv.Append(path_component);
  }

  if (rv.empty())
    return false;

  *result = std::move(rv);
  return true;
}

std::set<base::FilePath> NormalizeExtensionResourcePaths(
    const std::set<base::FilePath>& icons_paths) {
  std::set<base::FilePath> rv;
  for (const auto& icon_path : icons_paths) {
    base::FilePath normalized_path;
    if (NormalizeExtensionResourcePath(icon_path, &normalized_path))
      rv.emplace(std::move(normalized_path));
  }
  return rv;
}
