// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_QUERY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_QUERY_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "ios/chrome/browser/download/model/download_filter_util.h"

// Page size shared by `DownloadRecordDatabase::GetDownloadRecordsPage`
// and `DownloadRecordStore::GetDownloadsPage`. Balances index scan cost,
// peak memory, and per-frame UI rendering.
inline constexpr int kDownloadRecordsPageSize = 50;

// Query parameters for keyset-based pagination of download records.
//
// Records are returned ordered by `(created_time DESC, download_id DESC)`.
// When `cursor_created_time` and `cursor_download_id` are both set, only
// records strictly less than that tuple (in the same ordering) are returned,
// enabling stable continuation across pages even when new rows are inserted
// between calls.
//
// This struct lives in its own lightweight header so the public service
// interface can take a dependency on it without leaking the SQL-backed
// `DownloadRecordDatabase` implementation detail to callers.
struct DownloadRecordQuery {
  DownloadRecordQuery();
  DownloadRecordQuery(const DownloadRecordQuery& other);
  DownloadRecordQuery& operator=(const DownloadRecordQuery& other);
  ~DownloadRecordQuery();

  // Optional filter by file category (PDF/Image/Video/...). When unset or
  // `kAll`, all categories are returned.
  std::optional<DownloadFilterType> filter_type;
  // Pagination cursor: `created_time` of the last row from the previous page.
  std::optional<base::Time> cursor_created_time;
  // Pagination cursor: `download_id` of the last row from the previous page.
  std::optional<std::string> cursor_download_id;
  // Optional case-insensitive substring filter on the file name. Matched
  // against the normalized (case-folded) `file_name` column using SQL `LIKE`,
  // so e.g. "Port" matches "report.pdf".
  std::optional<std::string> name_query;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_QUERY_H_
