// Copyright 2015 The Chromium Authors. All rights reserved.
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
  int error_code = 0;
  if (event.type == MediaLogRecord::Type::kMediaStatus &&
      event.params.GetInteger(media::MediaLog::kStatusText, &error_code)) {
    PipelineStatus status = static_cast<PipelineStatus>(error_code);
    return std::string(media::MediaLog::kStatusText) + " " +
           PipelineStatusToString(status);
  }

  std::string params_json;
  base::JSONWriter::Write(event.params, &params_json);
  return params_json;
}

}  // namespace media
