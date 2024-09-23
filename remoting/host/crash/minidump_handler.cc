// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/crash/minidump_handler.h"

#include "base/functional/bind.h"
#include "remoting/base/breakpad_utils.h"

namespace remoting {

MinidumpHandler::MinidumpHandler() {
  // base::Unretained is sound as this instance controls the lifetime of both
  // |crash_directory_watcher_| and |crash_file_uploader_| and the Upload
  // callback runs on this sequence.
  crash_directory_watcher_.Watch(
      GetMinidumpDirectoryPath(),
      base::BindRepeating(&CrashFileUploader::Upload,
                          base::Unretained(&crash_file_uploader_)));
}

MinidumpHandler::~MinidumpHandler() = default;

}  // namespace remoting
