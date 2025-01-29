// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <cstring>

#include "sql/statement_id.h"

namespace sql {

bool StatementID::operator<(const StatementID& rhs) const noexcept {
  if (rhs.source_line_ != source_line_)
    return source_line_ < rhs.source_line_;
  return std::strcmp(source_file_, rhs.source_file_) < 0;
}

}  // namespace sql
