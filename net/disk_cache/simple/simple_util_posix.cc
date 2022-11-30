// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_util.h"

#include "base/files/file_util.h"

namespace disk_cache::simple_util {

bool SimpleCacheDeleteFile(const base::FilePath& path) {
  return base::DeleteFile(path);
}

}  // namespace disk_cache::simple_util
