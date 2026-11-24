// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"

namespace disk_cache {

namespace internal {

bool DeleteCacheContents(const base::FilePath& path) {
  const DWORD attr = ::GetFileAttributes(path.value().c_str());
  if (attr == INVALID_FILE_ATTRIBUTES) {
    const DWORD error = ::GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
  }
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY) || base::IsLink(path)) {
    return false;
  }

  base::FileEnumerator iter(
      path, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file = iter.Next(); !file.value().empty();
       file = iter.Next()) {
    if (!base::DeletePathRecursively(file)) {
      return false;
    }
  }
  return true;
}

}  // namespace internal

bool MoveCache(const base::FilePath& from_path, const base::FilePath& to_path) {
  if (from_path.empty() || to_path.empty() || from_path.ReferencesParent() ||
      to_path.ReferencesParent()) {
    return false;
  }
  const base::FilePath stripped_from = from_path.StripTrailingSeparators();
  const base::FilePath stripped_to = to_path.StripTrailingSeparators();
  if (base::IsLink(stripped_from) || !base::DirectoryExists(stripped_from) ||
      base::IsLink(stripped_to) || base::PathExists(stripped_to)) {
    return false;
  }

  // I don't want to use the shell version of move because if something goes
  // wrong, that version will attempt to move file by file and fail at the end.
  bool result = MoveFileEx(stripped_from.value().c_str(),
                           stripped_to.value().c_str(), /*dwFlags=*/0);
  base::UmaHistogramBoolean("DiskCache.MoveCacheToRestartCache.Windows",
                            result);
  if (!result) {
    PLOG(ERROR) << "Unable to move the cache";
    base::UmaHistogramSparse("DiskCache.MoveCacheToRestartCacheError.Windows",
                             ::GetLastError());
    return false;
  }
  return true;
}

}  // namespace disk_cache
