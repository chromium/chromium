// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
#define COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_

#include "base/files/file_path.h"

namespace webrtc_logging {

enum class ApiType {
  kExtension,
  kWeb,
};

class TextLogList {
 public:
  // Gets the file path for the log directory in a browser context's directory
  // for a given API type. The directory name will be appended to
  // |browser_context_path| and returned.
  static base::FilePath GetWebRtcLogDirectoryForBrowserContextPath(
      const base::FilePath& browser_context_path,
      ApiType api_type);

  // Gets the file paths for log directories in a browser context's directory
  // for all supported API types. The directory names will be appended to
  // |browser_context_path| and returned.
  static std::vector<base::FilePath>
  GetWebRtcLogDirectoriesForBrowserContextPath(
      const base::FilePath& browser_context_path);

  // Gets the file path for the log list file in a directory. The log list file
  // name will be appended to |dir| and returned.
  static base::FilePath GetWebRtcLogListFileForDirectory(
      const base::FilePath& dir);
};

}  // namespace webrtc_logging

#endif  // COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
