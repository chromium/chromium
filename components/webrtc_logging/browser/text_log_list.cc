// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/text_log_list.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_file.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "components/upload_list/text_log_upload_list.h"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "base/posix/eintr_wrapper.h"
#endif

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

// static
LogListReadResult TextLogList::ReadLogListFile(
    const base::FilePath& log_list_path,
    std::string* contents) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(contents);
  contents->clear();
  if (log_list_path.empty() || base::IsLink(log_list_path.DirName())) {
    return LogListReadResult::kError;
  }
  // Best-effort, racy pre-check on all platforms. On POSIX, O_NOFOLLOW below
  // is the real guard; on Windows (where symlink creation is normally
  // privileged and FLAG_NO_FOLLOW has no effect) this is the only one.
  if (base::IsLink(log_list_path)) {
    return LogListReadResult::kNonRegularReplaceable;
  }

#if BUILDFLAG(IS_POSIX)
  // base::File can't open with O_NONBLOCK, which keeps a FIFO here from
  // blocking open() until a writer appears.
  base::ScopedFD fd(
      HANDLE_EINTR(open(log_list_path.value().c_str(),
                        O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)));
  if (!fd.is_valid()) {
    const int saved_errno = errno;
    if (base::IsLink(log_list_path)) {
      return LogListReadResult::kNonRegularReplaceable;
    }
    return saved_errno == ENOENT ? LogListReadResult::kNotFound
                                 : LogListReadResult::kError;
  }

  base::stat_wrapper_t st;
  if (base::File::Fstat(fd.get(), &st) != 0) {
    return LogListReadResult::kError;
  }
  if (S_ISFIFO(st.st_mode)) {
    return LogListReadResult::kNonRegularReplaceable;
  }
  if (!S_ISREG(st.st_mode)) {
    return LogListReadResult::kError;
  }

  base::File file(std::move(fd));
#else
  base::File file(log_list_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                     base::File::FLAG_NO_FOLLOW);
  if (!file.IsValid()) {
    if (base::IsLink(log_list_path)) {
      return LogListReadResult::kNonRegularReplaceable;
    }
    return file.error_details() == base::File::FILE_ERROR_NOT_FOUND
               ? LogListReadResult::kNotFound
               : LogListReadResult::kError;
  }
#endif

  base::ScopedFILE stream(base::FileToFILE(std::move(file), "rb"));
  if (!stream || !base::ReadStreamToStringWithMaxSize(
                     stream.get(), kMaxLogListFileSizeBytes, contents)) {
    contents->clear();
    return LogListReadResult::kError;
  }
  return LogListReadResult::kRead;
}

// static
bool TextLogList::WriteLogListFile(const base::FilePath& log_list_path,
                                   std::string_view contents) {
  if (log_list_path.empty() || base::IsLink(log_list_path.DirName())) {
    return false;
  }
  return base::ImportantFileWriter::WriteFileAtomically(log_list_path,
                                                        contents);
}

}  // namespace webrtc_logging
