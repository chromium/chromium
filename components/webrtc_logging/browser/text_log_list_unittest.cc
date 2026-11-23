// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/text_log_list.h"

#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/eintr_wrapper.h"
#endif

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

#if BUILDFLAG(IS_POSIX)
TEST(TextLogListTest, ReadLogListFileHandlesFifoWithOpenWriterAndDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath log_list_path =
      TextLogList::GetWebRtcLogListFileForDirectory(temp_dir.GetPath());

  // A FIFO with a writer held open must return kNonRegularReplaceable rather
  // than failing with EAGAIN.
  ASSERT_EQ(0, mkfifo(log_list_path.value().c_str(), 0600));
  base::ScopedFD writer(
      HANDLE_EINTR(open(log_list_path.value().c_str(), O_RDWR | O_NONBLOCK)));
  ASSERT_TRUE(writer.is_valid());
  std::string contents;
  EXPECT_EQ(LogListReadResult::kNonRegularReplaceable,
            TextLogList::ReadLogListFile(log_list_path, &contents));
  EXPECT_TRUE(contents.empty());
  writer.reset();

  // A directory at Log List must return kError.
  ASSERT_TRUE(base::DeleteFile(log_list_path));
  ASSERT_TRUE(base::CreateDirectory(log_list_path));
  EXPECT_EQ(LogListReadResult::kError,
            TextLogList::ReadLogListFile(log_list_path, &contents));
  EXPECT_TRUE(contents.empty());
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace webrtc_logging
