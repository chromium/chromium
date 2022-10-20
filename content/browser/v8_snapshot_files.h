// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_
#define CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_

#include <map>
#include <string>

#include "base/files/file_path.h"

namespace content {

// Returns a mapping of V8 snapshot files to be preloaded for child processes
// that use V8. Note that this is defined on all platforms even though it may
// be empty or unused on some.
//
// This mapping can be used in `content::ChildProcessLauncherFileData` when
// constructing a ChildProcessLauncher.
std::map<std::string, base::FilePath> GetV8SnapshotFilesToPreload();

}  // namespace content

#endif  // CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_
