// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_util.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/disk_cache/cache_util.h"

namespace disk_cache {
namespace simple_util {

bool SimpleCacheDeleteFile(const base::FilePath& path) {
  // Even if a file was opened with FLAG_WIN_SHARE_DELETE, it is not possible to
  // create a new file with the same name until the original file is actually
  // deleted. To allow new files to be created with the new name right away,
  // the file is renamed before it is deleted.

  // Why a random name? Because if the name was derived from our original name,
  // then churn on a particular cache entry could cause flakey behaviour.

  // TODO(morlovich): Ensure these "todelete_" files are cleaned up on periodic
  // directory sweeps.
  const base::FilePath rename_target =
      path.DirName().AppendASCII(base::StringPrintf("todelete_%016" PRIx64,
                                                    base::RandUint64()));

  bool rename_succeeded =
      !!MoveFile(path.value().c_str(), rename_target.value().c_str());
  if (rename_succeeded)
    return base::DeleteFile(rename_target);

  // The rename did not succeed. The fallback behaviour is to delete the file in
  // place, which might cause some flake.
  return base::DeleteFile(path);
}

}  // namespace simple_util
}  // namespace disk_cache
