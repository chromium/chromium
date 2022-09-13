// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
#define COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_

#include "base/files/file_path.h"

class UploadList;

namespace content {
class BrowserContext;
}  // namespace content

namespace webrtc_logging {

class TextLogList {
 public:
  // Creates the upload list for a browser context. The upload list loads and
  // parses a text file list of WebRTC logs stored locally and/or uploaded.
  static UploadList* CreateWebRtcLogList(
      content::BrowserContext* browser_context);

  // Gets the file path for the log directory in a browser context's directory.
  // The directory name will be appended to |browser_context_path| and returned.
  static base::FilePath GetWebRtcLogDirectoryForBrowserContextPath(
      const base::FilePath& browser_context_path);

  // Gets the file path for the log list file in a directory. The log list file
  // name will be appended to |dir| and returned.
  static base::FilePath GetWebRtcLogListFileForDirectory(
      const base::FilePath& dir);
};

}  // namespace webrtc_logging

#endif  // COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
