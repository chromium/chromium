// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/text_log_list.h"

#include <vector>

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_logging {

TEST(TextLogListTest, GetWebRtcLogDirectoryForBrowserContextPath) {
  base::FilePath browser_context_path(
      FILE_PATH_LITERAL("/tmp/browser_context"));

  base::FilePath web_api_log_dir =
      TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_path, ApiType::kWeb);
  EXPECT_EQ(web_api_log_dir,
            browser_context_path.AppendASCII("WebRTC Web API Logs"));

  base::FilePath extension_api_log_dir =
      TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_path, ApiType::kExtension);
  EXPECT_EQ(extension_api_log_dir,
            browser_context_path.AppendASCII("WebRTC Logs"));
}

TEST(TextLogListTest, GetWebRtcLogDirectoriesForBrowserContextPath) {
  base::FilePath browser_context_path(
      FILE_PATH_LITERAL("/tmp/browser_context"));

  std::vector<base::FilePath> log_dirs =
      TextLogList::GetWebRtcLogDirectoriesForBrowserContextPath(
          browser_context_path);

  ASSERT_EQ(log_dirs.size(), 2u);
  EXPECT_THAT(log_dirs,
              testing::UnorderedElementsAre(
                  browser_context_path.AppendASCII("WebRTC Logs"),
                  browser_context_path.AppendASCII("WebRTC Web API Logs")));
}

TEST(TextLogListTest, GetWebRtcLogListFileForDirectory) {
  base::FilePath log_dir(FILE_PATH_LITERAL("/tmp/browser_context/WebRTC Logs"));

  base::FilePath log_list_file =
      TextLogList::GetWebRtcLogListFileForDirectory(log_dir);
  EXPECT_EQ(log_list_file, log_dir.AppendASCII("Log List"));
}

}  // namespace webrtc_logging
