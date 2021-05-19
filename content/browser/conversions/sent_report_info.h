// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_
#define CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_

#include <ostream>
#include <string>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Struct that contains data about sent reports for display in the Conversion
// Internals WebUI.
struct CONTENT_EXPORT SentReportInfo {
  GURL report_url;
  std::string report_body;
  int http_response_code;
};

// Only used for logging.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const SentReportInfo& info);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_SENT_REPORT_INFO_H_
