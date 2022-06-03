// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_
#define CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_

#include <map>
#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

namespace content {

// Returns a mapping of V8 snapshot files to be preloaded for child processes
// that use V8. Note that this is defined on all platforms even though it may
// be empty or unused on some.
//
// This mapping can be passed to
// `BrowserChildProcessHost::LaunchWithPreloadedFiles()`. `process_command_line`
// is the command line that will be used in launching the process the files will
// be supplied to.
std::map<std::string, base::FilePath> GetV8SnapshotFilesToPreload(
    base::CommandLine& process_command_line);

}  // namespace content

#endif  // CONTENT_BROWSER_V8_SNAPSHOT_FILES_H_
