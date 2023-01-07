// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_event_file_logger.h"

#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "remoting/host/win/event_trace_data.h"

namespace remoting {

namespace {

constexpr wchar_t kLatestLogSymbolicLinkName[] = L"latest.log";

}  // namespace

HostEventFileLogger::HostEventFileLogger(base::File log_file)
    : log_file_(std::move(log_file)) {}

HostEventFileLogger::~HostEventFileLogger() = default;

std::unique_ptr<HostEventLogger> HostEventFileLogger::Create() {
  base::FilePath directory;
  bool result = base::PathService::Get(base::DIR_EXE, &directory);
  DCHECK(result);

  if (!base::DirectoryExists(directory)) {
    LOG(ERROR) << "PathService returned an invalid DIR_EXE directory: "
               << directory;
    return nullptr;
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  base::FilePath log_file_path = directory.Append(base::StringPrintf(
      L"chrome_remote_desktop_%4d%02d%02d_%02d%02d%02d_%03d.log", exploded.year,
      exploded.month, exploded.day_of_month, exploded.hour, exploded.minute,
      exploded.second, exploded.millisecond));

  // Create the log_file and set the write mode to append.
  base::File log_file(log_file_path, base::File::Flags::FLAG_APPEND |
                                         base::File::Flags::FLAG_CREATE);

  if (!log_file.IsValid()) {
    LOG(ERROR) << "Failed to create the output log file at: " << log_file_path;
    return nullptr;
  }

  base::FilePath sym_link_path = directory.Append(kLatestLogSymbolicLinkName);
  // We don't need to check for existence first as DeleteFile() 'succeeds' if
  // the file doesn't exist.
  if (!base::DeleteFile(sym_link_path)) {
    PLOG(WARNING) << "Failed to delete symlink";
  }
  if (!::CreateSymbolicLink(sym_link_path.value().c_str(),
                            log_file_path.value().c_str(),
                            /*file*/ 0)) {
    PLOG(WARNING) << "Failed to create symbolic link for latest log file.";
  }

  return std::make_unique<HostEventFileLogger>(std::move(log_file));
}

void HostEventFileLogger::LogEvent(const EventTraceData& data) {
  // Log format is:
  // [YYYYMMDD/HHMMSS.sss:pid:tid:severity:file_name(line)] <message>
  std::string message(base::StringPrintf(
      "[%4d%02d%02d/%02d%02d%02d.%03d:%05d:%05d:%s:%s(%d)] %s",
      data.time_stamp.year, data.time_stamp.month, data.time_stamp.day_of_month,
      data.time_stamp.hour, data.time_stamp.minute, data.time_stamp.second,
      data.time_stamp.millisecond, data.process_id, data.thread_id,
      EventTraceData::SeverityToString(data.severity).c_str(),
      data.file_name.c_str(), data.line, data.message.c_str()));

  log_file_.Write(/*unused*/ 0, message.c_str(), message.size());
}

}  // namespace remoting
