// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STATEMENT_ID_H_
#define SQL_STATEMENT_ID_H_

#include <cstddef>

#include "base/component_export.h"

namespace sql {

// Identifies a compiled SQLite statement in a statement cache.
//
// This is a value type with the same performance characteristics as
// std::string_view. Instances are thread-unsafe, but not thread-hostile.
//
// StatementID instances should be constructed by using the SQL_FROM_HERE
// macro, which produces an unique ID based on the source file name and line.
class COMPONENT_EXPORT(SQL) StatementID {
 public:
  // Creates an ID representing a line in the source tree.
  //
  // SQL_FROM_HERE should be preferred to calling this constructor directly.
  //
  // |source_file| should point to a C-style string that lives for the duration
  // of the program.
  explicit StatementID(const char* source_file, size_t source_line) noexcept
      : source_file_(source_file), source_line_(source_line) {}

  // Copying intentionally allowed.
  StatementID(const StatementID&) noexcept = default;
  StatementID& operator=(const StatementID&) noexcept = default;

  // Facilitates storing StatementID instances in maps.
  bool operator<(const StatementID& rhs) const noexcept;

 private:
  // Instances cannot be immutable because they support being used as map keys.
  //
  // It seems tempting to merge source_file_ and source_line_ in a single
  // source_location_ member, and to have SQL_FROM_HERE use C++ preprocessor
  // magic to generate strings like "sql/connection.cc:42". This causes a
  // non-trivial binary size increase, because Chrome uses -fmerge-constants and
  // SQL_FROM_HERE tends to be used many times in the same few files.
  const char* source_file_;
  size_t source_line_;
};

}  // namespace sql

// Produces a StatementID based on the current line in the source tree.
#define SQL_FROM_HERE sql::StatementID(__FILE__, __LINE__)

#endif  // SQL_STATEMENT_ID_H_
