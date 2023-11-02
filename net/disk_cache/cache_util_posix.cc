// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"

namespace disk_cache {

bool MoveCache(const base::FilePath& from_path, const base::FilePath& to_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For ChromeOS, we don't actually want to rename the cache
  // directory, because if we do, then it'll get recreated through the
  // encrypted filesystem (with encrypted names), and we won't be able
  // to see these directories anymore in an unmounted encrypted
  // filesystem, so we just move each item in the cache to a new
  // directory.
  if (!base::CreateDirectory(to_path)) {
    LOG(ERROR) << "Unable to create destination cache directory.";
    return false;
  }
  base::FileEnumerator iter(from_path, false /* not recursive */,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath name = iter.Next(); !name.value().empty();
       name = iter.Next()) {
    base::FilePath destination = to_path.Append(name.BaseName());
    if (!base::Move(name, destination)) {
      LOG(ERROR) << "Unable to move cache item.";
      return false;
    }
  }
  return true;
#else
  return base::Move(from_path, to_path);
#endif
}

}  // namespace disk_cache
