// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <memory>

#include "base/json/json_writer.h"
#include "media/base/media_serializers.h"

namespace media {

namespace internal {

StatusData::StatusData() = default;

StatusData::StatusData(const StatusData& copy) {
  *this = copy;
}

StatusData::StatusData(StatusGroupType group,
                       StatusCodeType code,
                       std::string_view message)
    : group(group),
      code(code),
      message(message),
      data(base::Value(base::Value::Type::DICT)) {}

std::unique_ptr<StatusData> StatusData::copy() const {
  auto result = std::make_unique<StatusData>(group, code, message);
  result->frames = frames.Clone();
  if (cause)
    result->cause = cause->copy();
  result->data = data.Clone();
  return result;
}

StatusData::~StatusData() = default;

StatusData& StatusData::operator=(const StatusData& copy) {
  group = copy.group;
  code = copy.code;
  message = copy.message;
  frames = copy.frames.Clone();
  if (copy.cause)
    cause = copy.cause->copy();
  data = copy.data.Clone();
  return *this;
}

void StatusData::AddLocation(const base::Location& location) {
  frames.Append(MediaSerialize(location));
}

void StatusData::RenderToLogWriter(logging::LogSeverity severity) const {
  auto* file = frames.front().GetDict().FindString(StatusConstants::kFileKey);
  auto line = frames.front().GetDict().FindInt(StatusConstants::kLineKey);
  DCHECK(file);
  DCHECK(line);

  auto log_writer = logging::LogMessage(file->c_str(), *line, severity);
  log_writer.stream() << group;

  if (message.size()) {
    log_writer.stream() << ": " << message;
  }

  if (data.GetDict().size()) {
    log_writer.stream()
        << " Data: " << base::WriteJson(data.GetDict()).value_or(std::string());
  }

  if (frames.size() > 1) {
    log_writer.stream() << " Trace: "
                        << base::WriteJson(frames).value_or(std::string());
  }

  if (cause) {
    cause->RenderToLogWriter(severity);
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const OkStatusImplicitConstructionHelper&) {
  stream << "kOk";
  return stream;
}

}  // namespace internal

const char StatusConstants::kCodeKey[] = "code";
const char StatusConstants::kGroupKey[] = "group";
const char StatusConstants::kMsgKey[] = "message";
const char StatusConstants::kStackKey[] = "stack";
const char StatusConstants::kDataKey[] = "data";
const char StatusConstants::kCauseKey[] = "cause";
const char StatusConstants::kFileKey[] = "file";
const char StatusConstants::kLineKey[] = "line";

internal::OkStatusImplicitConstructionHelper OkStatus() {
  return {};
}

}  // namespace media
