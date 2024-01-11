// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_UPLOAD_LIST_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_UPLOAD_LIST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/token.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"
#include "content/common/content_export.h"

namespace content {

// TraceUploadList is used on to read/write from the database of trace
// reports stored locally.
class TraceUploadList {
 public:
  using FinishedProcessingCallback = base::OnceCallback<void(bool success)>;
  using GetReportsCallback =
      base::OnceCallback<void(std::vector<ClientTraceReport> result)>;
  using GetProtoCallback =
      base::OnceCallback<void(std::optional<base::span<const char>> result)>;

  virtual void OpenDatabaseIfExists() = 0;

  virtual void GetAllTraceReports(GetReportsCallback callback) = 0;

  virtual void DeleteSingleTrace(const base::Token& trace_uuid,
                                 FinishedProcessingCallback callback) = 0;

  virtual void DeleteAllTraces(FinishedProcessingCallback callback) = 0;

  virtual void UserUploadSingleTrace(const base::Token& trace_uuid,
                                     FinishedProcessingCallback callback) = 0;

  virtual void DownloadTrace(const base::Token& trace_uuid,
                             GetProtoCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_UPLOAD_LIST_H_
