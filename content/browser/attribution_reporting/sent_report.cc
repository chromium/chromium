// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sent_report.h"

namespace content {

SentReport::SentReport(AttributionReport report,
                       Status status,
                       int http_response_code)
    : report(std::move(report)),
      status(status),
      http_response_code(http_response_code) {}

SentReport::SentReport(const SentReport& other) = default;
SentReport& SentReport::operator=(const SentReport& other) = default;
SentReport::SentReport(SentReport&& other) = default;
SentReport& SentReport::operator=(SentReport&& other) = default;

SentReport::~SentReport() = default;

}  // namespace content
