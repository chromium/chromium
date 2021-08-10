// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_
#define CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Struct that contains data about sent reports. Some info is displayed in the
// Conversion Internals WebUI.
struct CONTENT_EXPORT SentReportInfo {
  SentReportInfo(int64_t conversion_id,
                 base::Time original_report_time,
                 GURL report_url,
                 std::string report_body,
                 int http_response_code,
                 bool should_retry);
  SentReportInfo(const SentReportInfo& other);
  SentReportInfo& operator=(const SentReportInfo& other);
  SentReportInfo(SentReportInfo&& other);
  SentReportInfo& operator=(SentReportInfo&& other);
  ~SentReportInfo();

  // Information from the `ConversionReport` that was sent.
  int64_t conversion_id;
  base::Time original_report_time;

  // Information on the network request that was sent.
  //
  // An empty `report_url` indicates that the report was not attempted to be
  // sent. For example, if the embedder disabled the API.
  GURL report_url;
  std::string report_body;
  int http_response_code;

  // Whether the report should be retried. Set if the report failed without
  // receiving response headers.
  bool should_retry;

  // When adding new members, the corresponding `operator==()` definition in
  // `conversion_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_
