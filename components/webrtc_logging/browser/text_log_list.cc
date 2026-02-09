// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/text_log_list.h"

#include "base/files/file.h"
#include "components/upload_list/text_log_upload_list.h"

namespace webrtc_logging {

namespace {

const char kWebRtcExtensionApiLogDirectory[] = "WebRTC Logs";
const char kWebRtcWebApiLogDirectory[] = "WebRTC Web API Logs";
const char kWebRtcLogListFilename[] = "Log List";

}  // namespace

// static
base::FilePath TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
    const base::FilePath& browser_context_path,
    ApiType api_type) {
  DCHECK(!browser_context_path.empty());
  return browser_context_path.AppendASCII(
      api_type == ApiType::kWeb ? kWebRtcWebApiLogDirectory
                                : kWebRtcExtensionApiLogDirectory);
}

// static
std::vector<base::FilePath>
TextLogList::GetWebRtcLogDirectoriesForBrowserContextPath(
    const base::FilePath& browser_context_path) {
  DCHECK(!browser_context_path.empty());
  return {
      browser_context_path.AppendASCII(kWebRtcExtensionApiLogDirectory),
      browser_context_path.AppendASCII(kWebRtcWebApiLogDirectory),
  };
}

// static
base::FilePath TextLogList::GetWebRtcLogListFileForDirectory(
    const base::FilePath& dir) {
  DCHECK(!dir.empty());
  return dir.AppendASCII(kWebRtcLogListFilename);
}

}  // namespace webrtc_logging
