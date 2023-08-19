// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_media_log.h"
#include "base/json/json_writer.h"

namespace media {

MockMediaLog::MockMediaLog() = default;

MockMediaLog::~MockMediaLog() = default;

void MockMediaLog::AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) {
  const auto log_string = MediaEventToLogString(*event);
  VLOG(2) << "MediaLog: " << log_string;
  DoAddLogRecordLogString(log_string);
  most_recent_event_ = std::move(event);
}

std::string MockMediaLog::MediaEventToLogString(const MediaLogRecord& event) {
  // Special case for PIPELINE_ERROR, since that's by far the most useful
  // event for figuring out media pipeline failures, and just reporting
  // pipeline status as numeric code is not very helpful/user-friendly.
  if (event.type == MediaLogRecord::Type::kMediaStatus) {
    const std::string* group = event.params.FindString("group");
    if (group && *group == "PipelineStatus") {
      auto code = event.params.FindInt("code").value_or(0);
      return PipelineStatusToString(static_cast<PipelineStatusCodes>(code));
    }
  }

  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return params_json;
}

bool MockMediaLog::ShouldLogToDebugConsole() const {
  return false;
}

}  // namespace media
