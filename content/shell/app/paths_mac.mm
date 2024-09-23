// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/paths_mac.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/common/content_paths.h"

namespace {

base::FilePath GetContentsPath() {
  // Start out with the path to the running executable.
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);

  // Up to Contents.
  if (base::apple::IsBackgroundOnlyProcess()) {
    // The running executable is the helper, located at:
    // Content Shell.app/Contents/Frameworks/
    // Content Shell Framework.framework/Versions/C/Helpers/Content Shell
    // Helper.app/ Contents/MacOS/Content Shell Helper. Go up nine steps to get
    // to the main bundle's Contents directory.
    path = path.DirName()
               .DirName()
               .DirName()
               .DirName()
               .DirName()
               .DirName()
               .DirName()
               .DirName()
               .DirName();
  } else {
    // One step up to MacOS, another to Contents.
    path = path.DirName().DirName();
  }
  DCHECK_EQ("Contents", path.BaseName().value());

  return path;
}

base::FilePath GetFrameworksPath() {
  return GetContentsPath().Append("Frameworks");
}

}  // namespace

void OverrideFrameworkBundlePath() {
  base::FilePath helper_path =
      GetFrameworksPath().Append("Content Shell Framework.framework");

  base::apple::SetOverrideFrameworkBundlePath(helper_path);
}

void OverrideOuterBundlePath() {
  base::FilePath path = GetContentsPath().DirName();

  base::apple::SetOverrideOuterBundlePath(path);
}

void OverrideChildProcessPath() {
  base::FilePath helper_path = base::apple::FrameworkBundlePath()
                                   .Append("Helpers")
                                   .Append("Content Shell Helper.app")
                                   .Append("Contents")
                                   .Append("MacOS")
                                   .Append("Content Shell Helper");

  base::PathService::OverrideAndCreateIfNeeded(
      content::CHILD_PROCESS_EXE, helper_path, /*is_absolute=*/true,
      /*create=*/false);
}

void OverrideSourceRootPath() {
  // The base implementation to get base::DIR_SRC_TEST_DATA_ROOT assumes the
  // current process path is the top level app path, not a nested one.
  //
  // Going up 5 levels is needed, since frameworks path looks something like
  // src/out/foo/Content Shell.app/Contents/Framework/
  base::PathService::Override(
      base::DIR_SRC_TEST_DATA_ROOT,
      GetFrameworksPath().DirName().DirName().DirName().DirName().DirName());
}

base::FilePath GetResourcesPakFilePath() {
  NSString* pak_path =
      [base::apple::FrameworkBundle() pathForResource:@"content_shell"
                                               ofType:@"pak"];

  return base::FilePath([pak_path fileSystemRepresentation]);
}

base::FilePath GetInfoPlistPath() {
  return GetContentsPath().Append("Info.plist");
}

void OverrideBundleID() {
  NSBundle* bundle = base::apple::OuterBundle();
  base::apple::SetBaseBundleID(
      base::SysNSStringToUTF8([bundle bundleIdentifier]).c_str());
}
