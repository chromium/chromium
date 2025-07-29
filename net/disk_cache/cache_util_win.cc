// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"

namespace disk_cache {

bool MoveCache(const base::FilePath& from_path, const base::FilePath& to_path) {
  // I don't want to use the shell version of move because if something goes
  // wrong, that version will attempt to move file by file and fail at the end.
  bool result =
      MoveFileEx(from_path.value().c_str(), to_path.value().c_str(), 0);
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
