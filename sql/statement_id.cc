// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement_id.h"

#include <cstring>

#include "base/compiler_specific.h"

namespace sql {

bool StatementID::operator<(const StatementID& rhs) const noexcept {
  if (rhs.source_line_ != source_line_)
    return source_line_ < rhs.source_line_;
  return UNSAFE_TODO(std::strcmp(source_file_, rhs.source_file_)) < 0;
}

}  // namespace sql
