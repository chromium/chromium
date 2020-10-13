// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PARSER_H_
#define CONTENT_BROWSER_SMS_SMS_PARSER_H_

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {

// Extracts the origin from SMS messages according to the server-side
// convention of https://github.com/samuelgoto/sms-receiver.
// Returns an empty result if the formatting doesn't match.
class CONTENT_EXPORT SmsParser {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SmsParsingStatus {
    kParsed = 0,
    kOTPFormatRegexNotMatch = 1,
    kHostAndPortNotParsed = 2,
    kGURLNotValid = 3,
    kMaxValue = kGURLNotValid,
  };

  struct CONTENT_EXPORT Result {
    // Creates Result when the parsing has succeeded.
    Result(const url::Origin& origin, const std::string& one_time_code);
    // Creates Result when the parsing has failed.
    explicit Result(SmsParsingStatus);
    ~Result();

    bool IsValid() { return parsing_status == SmsParsingStatus::kParsed; }

    const url::Origin origin;
    const std::string one_time_code;
    SmsParsingStatus parsing_status;
  };

  static Result Parse(base::StringPiece sms);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PARSER_H_
