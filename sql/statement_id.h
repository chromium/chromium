// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STATEMENT_ID_H_
#define SQL_STATEMENT_ID_H_

#include <cstdint>
#include <string_view>
#include <tuple>

#include "base/types/strong_alias.h"

namespace sql {

// Identifies a compiled SQLite statement in a statement cache.
//
// This is a value type with the same performance characteristics as
// `std::string_view`. Instances are thread-unsafe, but not thread-hostile.
//
// `StatementID` instances should be constructed by using the `SQL_FROM_HERE`
// macro, which produces an unique ID based on the source file name and line.
//
// It may seem tempting to represent `StatementID` as a string composed of the
// source file and line number, e.g. "sql/connection.cc:42". Although this is
// easy to do with C++ preprocessor magic, it would lead to a non-trivial
// increase in binary size. Chrome is compiled with `-fmerge-constants`, so
// repeated string literals are coalesced in the binary. This happens frequently
// with `SQL_FROM_HERE` because the macro tends to be used many times in the
// same file. If `SQL_FROM_HERE` generated many distinct string literals, we'd
// no longer benefit from this compile-time deduplication.
using StatementID = base::StrongAlias<class StatementIDTag,
                                      std::tuple<std::string_view, uint32_t>>;

}  // namespace sql

// Produces a StatementID based on the current line in the source tree.
#define SQL_FROM_HERE sql::StatementID({__FILE__, uint32_t{__LINE__}})

#endif  // SQL_STATEMENT_ID_H_
