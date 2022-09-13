// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/text_log_list.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "components/upload_list/text_log_upload_list.h"
#include "content/public/browser/browser_context.h"

namespace webrtc_logging {

namespace {

const char kWebRtcLogDirectory[] = "WebRTC Logs";
const char kWebRtcLogListFilename[] = "Log List";

}  // namespace

// static
UploadList* TextLogList::CreateWebRtcLogList(
    content::BrowserContext* browser_context) {
  base::FilePath log_list_path = GetWebRtcLogListFileForDirectory(
      GetWebRtcLogDirectoryForBrowserContextPath(browser_context->GetPath()));
  return new TextLogUploadList(log_list_path);
}

// static
base::FilePath TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
    const base::FilePath& browser_context_path) {
  DCHECK(!browser_context_path.empty());
  return browser_context_path.AppendASCII(kWebRtcLogDirectory);
}

// static
base::FilePath TextLogList::GetWebRtcLogListFileForDirectory(
    const base::FilePath& dir) {
  DCHECK(!dir.empty());
  return dir.AppendASCII(kWebRtcLogListFilename);
}

}  // namespace webrtc_logging
