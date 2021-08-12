// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"

// SELECT, UPDATE, and DELETE SQL statements should document the indexes
// they intend to use by adding `DCHECK_SQL_INDEXED_BY()` after every table
// name. This makes code review easier, and enlists SQLite's help in ensuring
// that the index *can* be used to satisfy the query.
//
// The INDEXED BY clause causes SQLite to return an error if it cannot satisfy
// the query using the specified index. Without this, schema and query changes
// can silently cause an index to be ignored, leading to poor performance in the
// worst case and unnecessary storage costs in the best.
#if DCHECK_IS_ON()
#define DCHECK_SQL_INDEXED_BY(index) " INDEXED BY " index " "
#else
#define DCHECK_SQL_INDEXED_BY(index) ""
#endif

namespace url {
class Origin;
}  // namespace url

namespace content {

std::string SerializeOrigin(const url::Origin& origin) WARN_UNUSED_RESULT;

url::Origin DeserializeOrigin(const std::string& origin) WARN_UNUSED_RESULT;

int64_t SerializeImpressionOrConversionData(uint64_t data) WARN_UNUSED_RESULT;

uint64_t DeserializeImpressionOrConversionData(int64_t data) WARN_UNUSED_RESULT;

// Prevent these functions from being called in the wrong direction.
int64_t SerializeImpressionOrConversionData(int64_t data) = delete;
uint64_t DeserializeImpressionOrConversionData(uint64_t data) = delete;

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_SQL_UTILS_H_
