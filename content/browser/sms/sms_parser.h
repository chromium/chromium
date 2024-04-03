// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PARSER_H_
#define CONTENT_BROWSER_SMS_SMS_PARSER_H_

#include <string_view>

#include "content/common/content_export.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {

// Extracts the origins from SMS messages according to the server-side
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
    Result(const url::Origin& top_origin,
           const url::Origin& embedded_origin,
           const std::string& one_time_code);
    // Creates Result when the parsing has failed.
    explicit Result(SmsParsingStatus);
    Result(const Result& other);
    ~Result();

    bool IsValid() const { return parsing_status == SmsParsingStatus::kParsed; }

    // The origin list consists of the origin that made the OTP request followed
    // by its cross-origin ancestor's origin if such an ancestor exists.
    OriginList GetOriginList() const;

    const url::Origin top_origin;
    const url::Origin embedded_origin;
    const std::string one_time_code;
    SmsParsingStatus parsing_status;
  };

  static Result Parse(std::string_view sms);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PARSER_H_
