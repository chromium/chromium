// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/sent_report_info.h"

namespace content {

SentReportInfo::SentReportInfo(int64_t conversion_id,
                               base::Time original_report_time,
                               GURL report_url,
                               std::string report_body,
                               int http_response_code,
                               bool should_retry)
    : conversion_id(conversion_id),
      original_report_time(original_report_time),
      report_url(report_url),
      report_body(std::move(report_body)),
      http_response_code(http_response_code),
      should_retry(should_retry) {}

SentReportInfo::SentReportInfo(const SentReportInfo& other) = default;
SentReportInfo& SentReportInfo::operator=(const SentReportInfo& other) =
    default;
SentReportInfo::SentReportInfo(SentReportInfo&& other) = default;
SentReportInfo& SentReportInfo::operator=(SentReportInfo&& other) = default;

SentReportInfo::~SentReportInfo() = default;

}  // namespace content
