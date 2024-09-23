// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_PATHS_MAC_H_
#define CONTENT_SHELL_APP_PATHS_MAC_H_

namespace base {
class FilePath;
}

// Sets up base::apple::FrameworkBundle.
void OverrideFrameworkBundlePath();

// Set up base::apple::OuterBundle.
void OverrideOuterBundlePath();

// Sets up the CHILD_PROCESS_EXE path to properly point to the helper app.
void OverrideChildProcessPath();

// Sets up base::DIR_SRC_TEST_DATA_ROOT to properly point to the source
// directory.
void OverrideSourceRootPath();

// Gets the path to the content shell's pak file.
base::FilePath GetResourcesPakFilePath();

// Gets the path to content shell's Info.plist file.
base::FilePath GetInfoPlistPath();

// Sets up base::apple::BaseBundleID.
void OverrideBundleID();

#endif  // CONTENT_SHELL_APP_PATHS_MAC_H_
