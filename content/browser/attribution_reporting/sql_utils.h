// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_

#include <string>

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

url::Origin DeserializeOrigin(const std::string& origin);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
