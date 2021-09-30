// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sent_report_info.h"

namespace content {

SentReportInfo::SentReportInfo(AttributionReport report,
                               Status status,
                               int http_response_code)
    : report(std::move(report)),
      status(status),
      http_response_code(http_response_code) {}

SentReportInfo::SentReportInfo(const SentReportInfo& other) = default;
SentReportInfo& SentReportInfo::operator=(const SentReportInfo& other) =
    default;
SentReportInfo::SentReportInfo(SentReportInfo&& other) = default;
SentReportInfo& SentReportInfo::operator=(SentReportInfo&& other) = default;

SentReportInfo::~SentReportInfo() = default;

}  // namespace content
