// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <memory>

#include "media/base/media_serializers.h"

namespace media {

namespace internal {

StatusData::StatusData() = default;

StatusData::StatusData(const StatusData& copy) {
  *this = copy;
}

StatusData::StatusData(StatusGroupType group,
                       StatusCodeType code,
                       std::string message,
                       UKMPackedType root_cause)
    : group(group),
      code(code),
      message(std::move(message)),
      data(base::Value(base::Value::Type::DICT)),
      packed_root_cause(root_cause) {}

std::unique_ptr<StatusData> StatusData::copy() const {
  auto result =
      std::make_unique<StatusData>(group, code, message, packed_root_cause);
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
  packed_root_cause = copy.packed_root_cause;
  frames = copy.frames.Clone();
  if (copy.cause)
    cause = copy.cause->copy();
  data = copy.data.Clone();
  return *this;
}

void StatusData::AddLocation(const base::Location& location) {
  frames.Append(MediaSerialize(location));
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
