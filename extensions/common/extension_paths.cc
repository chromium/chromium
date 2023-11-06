// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_paths.h"

#include "base/files/file_util.h"
#include "base/path_service.h"

namespace extensions {

bool PathProvider(int key, base::FilePath* result) {
  if (key != DIR_TEST_DATA)
    return false;
  base::FilePath cur;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
    return false;
  }
  cur = cur.Append(FILE_PATH_LITERAL("extensions"));
  cur = cur.Append(FILE_PATH_LITERAL("test"));
  cur = cur.Append(FILE_PATH_LITERAL("data"));
  if (!base::PathExists(cur))  // we don't want to create this
    return false;
  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace extensions
