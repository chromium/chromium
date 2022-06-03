// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/error_reporter.h"

#include <sstream>

#include "base/check_op.h"
#include "base/strings/string_util.h"

namespace headless {

ErrorReporter::ErrorReporter() = default;

ErrorReporter::~ErrorReporter() = default;

#if DCHECK_IS_ON()
void ErrorReporter::Push() {
  path_.push_back(nullptr);
}

void ErrorReporter::Pop() {
  path_.pop_back();
}

void ErrorReporter::SetName(const char* name) {
  DCHECK(!path_.empty());
  path_.back() = name;
}

void ErrorReporter::AddError(base::StringPiece description) {
  std::stringstream error;
  for (size_t i = 0; i < path_.size(); i++) {
    if (!path_[i]) {
      DCHECK_EQ(i + 1, path_.size());
      break;
    }
    if (i)
      error << '.';
    error << path_[i];
  }
  if (error.tellp())
    error << ": ";
  error << description;
  errors_.push_back(error.str());
}

bool ErrorReporter::HasErrors() const {
  return !errors_.empty();
}

std::string ErrorReporter::ToString() const {
  return base::JoinString(errors_, ", ");
}
#endif  // DCHECK_IS_ON()

}  // namespace headless
