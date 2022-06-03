// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <memory>
#include "base/strings/string_piece.h"
#include "media/base/media_serializers.h"

namespace media {

namespace internal {

StatusData::StatusData() = default;

StatusData::StatusData(const StatusData& copy) {
  *this = copy;
}

StatusData::StatusData(StatusGroupType group,
                       StatusCodeType code,
                       std::string message)
    : group(group),
      code(code),
      message(std::move(message)),
      data(base::Value(base::Value::Type::DICTIONARY)) {}

std::unique_ptr<StatusData> StatusData::copy() const {
  auto result = std::make_unique<StatusData>(group, code, message);
  for (const auto& frame : frames)
    result->frames.push_back(frame.Clone());
  for (const auto& cause : causes)
    result->causes.push_back(cause);
  result->data = data.Clone();
  return result;
}

StatusData::~StatusData() = default;

StatusData& StatusData::operator=(const StatusData& copy) {
  group = copy.group;
  code = copy.code;
  message = copy.message;
  for (const auto& frame : copy.frames)
    frames.push_back(frame.Clone());
  for (const auto& cause : copy.causes)
    causes.push_back(cause);
  data = copy.data.Clone();
  return *this;
}

void StatusData::AddLocation(const base::Location& location) {
  frames.push_back(MediaSerialize(location));
}

}  // namespace internal

Status OkStatus() {
  return Status(StatusCode::kOk);
}

}  // namespace media
