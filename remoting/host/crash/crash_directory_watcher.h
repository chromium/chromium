// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CRASH_CRASH_DIRECTORY_WATCHER_H_
#define REMOTING_HOST_CRASH_CRASH_DIRECTORY_WATCHER_H_

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"

namespace remoting {

// This class watches the specified directory and runs a callback when a crash
// dump and metadata file are written to the watched directory.
class CrashDirectoryWatcher {
 public:
  using UploadCallback =
      base::RepeatingCallback<void(const base::FilePath& crash_guid)>;

  CrashDirectoryWatcher();

  CrashDirectoryWatcher(const CrashDirectoryWatcher&) = delete;
  CrashDirectoryWatcher& operator=(const CrashDirectoryWatcher&) = delete;

  ~CrashDirectoryWatcher();

  void Watch(base::FilePath directory_to_watch, UploadCallback callback);

 private:
  void OnFileChangeDetected(const base::FilePath& path, bool error);

  base::FilePathWatcher file_path_watcher_;
  UploadCallback upload_callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CRASH_CRASH_DIRECTORY_WATCHER_H_
