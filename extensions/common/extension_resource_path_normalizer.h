// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_RESOURCE_PATH_NORMALIZER_H_
#define EXTENSIONS_COMMON_EXTENSION_RESOURCE_PATH_NORMALIZER_H_

#include <set>

#include "base/files/file_path.h"

// Normalize extension resource relative path. Removes ".". Returns false if
// path can not be normalized, i.e. it references its parent or empty after
// normalization.
bool NormalizeExtensionResourcePath(const base::FilePath& path,
                                    base::FilePath* result);

// Applies |NormalizeExtensionResourcePath| for each resource path and adds it
// to result in the case of success.
std::set<base::FilePath> NormalizeExtensionResourcePaths(
    const std::set<base::FilePath>& paths);

#endif  // EXTENSIONS_COMMON_EXTENSION_RESOURCE_PATH_NORMALIZER_H_
