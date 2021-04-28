// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status.h"

#include <memory>
#include "base/strings/string_piece.h"
#include "media/base/media_serializers.h"

namespace media {

Status::Status() = default;

Status::Status(StatusCode code,
               base::StringPiece message,
               const base::Location& location) {
  // Note that |message| is dropped in this case.
  if (code == StatusCode::kOk) {
    DCHECK(message.empty());
    return;
  }
  data_ = std::make_unique<StatusInternal>(code, std::string(message));
  AddFrame(location);
}

// Copy Constructor
Status::Status(const Status& copy) {
  *this = copy;
}

Status& Status::operator=(const Status& copy) {
  if (copy.is_ok()) {
    data_.reset();
    return *this;
  }

  data_ = std::make_unique<StatusInternal>(copy.code(), copy.message());
  for (const base::Value& frame : copy.data_->frames)
    data_->frames.push_back(frame.Clone());
  for (const Status& err : copy.data_->causes)
    data_->causes.push_back(err);
  data_->data = copy.data_->data.Clone();
  return *this;
}

// Allow move.
Status::Status(Status&&) = default;
Status& Status::operator=(Status&&) = default;

Status::~Status() = default;

Status::StatusInternal::StatusInternal(StatusCode code, std::string message)
    : code(code),
      message(std::move(message)),
      data(base::Value(base::Value::Type::DICTIONARY)) {}

Status::StatusInternal::~StatusInternal() = default;

Status&& Status::AddHere(const base::Location& location) && {
  DCHECK(data_);
  AddFrame(location);
  return std::move(*this);
}

Status&& Status::AddCause(Status&& cause) && {
  DCHECK(data_ && cause.data_);
  data_->causes.push_back(std::move(cause));
  return std::move(*this);
}

void Status::AddFrame(const base::Location& location) {
  DCHECK(data_);
  data_->frames.push_back(MediaSerialize(location));
}

Status OkStatus() {
  return Status(StatusCode::kOk);
}

}  // namespace media
