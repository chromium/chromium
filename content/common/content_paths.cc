// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_paths.h"

#include "base/files/file_util.h"
#include "base/path_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

namespace content {

bool PathProvider(int key, base::FilePath* result) {
  switch (key) {
    case CHILD_PROCESS_EXE:
      return base::PathService::Get(base::FILE_EXE, result);
#if BUILDFLAG(IS_ANDROID)
    case DIR_FILE_SYSTEM_API_SWAP: {
      if (!base::android::GetCacheDirectory(result)) {
        return false;
      }
      *result = result->Append("FileSystemAPISwap");
      return true;
    }
#endif
    case DIR_TEST_DATA: {
      base::FilePath cur;
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("content"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!base::PathExists(cur))  // we don't want to create this
        return false;

      *result = cur;
      return true;
    }
    default:
      return false;
  }
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace content
