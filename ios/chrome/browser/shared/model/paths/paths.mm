// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/paths/paths.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/notreached.h"
#import "base/path_service.h"
#import "base/threading/thread_restrictions.h"
#import "build/branding_buildflags.h"
#import "components/gcm_driver/gcm_driver_constants.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"

namespace ios {
namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const base::FilePath::CharType kProductDirName[] =
    FILE_PATH_LITERAL("Google/Chrome");
#else
const base::FilePath::CharType kProductDirName[] =
    FILE_PATH_LITERAL("Chromium");
#endif

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_APP_DATA, result)) {
    DUMP_WILL_BE_NOTREACHED();
    return false;
  }
  *result = result->Append(kProductDirName);
  return true;
}

bool PathProvider(int key, base::FilePath* result) {
  // Assume that creation of the directory is not required if it does not exist.
  // This flag is set to true for the case where it needs to be created.
  bool create_dir = false;

  base::FilePath cur;
  switch (key) {
    case DIR_USER_DATA:
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      create_dir = true;
      break;

    case DIR_CRASH_DUMPS:
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crash Reports"));
      create_dir = true;
      break;

    case DIR_TEST_DATA:
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("ios"));
      cur = cur.Append(FILE_PATH_LITERAL("chrome"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      break;

    case DIR_GLOBAL_GCM_STORE:
      if (!base::PathService::Get(DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(gcm_driver::kGCMStoreDirname);
      break;

    case FILE_LOCAL_STATE:
      if (!base::PathService::Get(DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Local State"));
      break;

    case FILE_RESOURCES_PACK:
      // Catalyst builds are packaged like macOS, with the binary and resource
      // directories separate. On iOS they are all together in a single dir.
      // base::DIR_ASSETS does the right thing on each platform.
      if (!base::PathService::Get(base::DIR_ASSETS, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("resources.pak"));
      break;

    case DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS:
      if (!base::PathService::Get(DIR_USER_DATA, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("OptimizationGuidePredictionModels"));
      create_dir = true;
      break;

    default:
      return false;
  }

  if (create_dir && !base::PathExists(cur) && !base::CreateDirectory(cur))
    return false;

  *result = cur;
  return true;
}

}  // namespace

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  // If the profile directory is under ~/Library/Application Support,
  // use a suitable cache directory under ~/Library/Caches.

  // Default value in cases where any of the following fails.
  *result = profile_dir;

  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_data_dir))
    return;
  base::FilePath cache_dir;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir))
    return;
  if (!app_data_dir.AppendRelativePath(profile_dir, &cache_dir)) {
    return;
  }

  *result = cache_dir;
}

}  // namespace ios
