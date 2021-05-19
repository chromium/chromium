// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/sent_report_info.h"

namespace content {

std::ostream& operator<<(std::ostream& out, const SentReportInfo& info) {
  out << "report_url: " << info.report_url
      << ", report_body: " << info.report_body
      << ", http_response_code: " << info.http_response_code;
  return out;
}

}  // namespace content
