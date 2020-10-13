// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// SMS one-time-passcode format:
// https://wicg.github.io/sms-one-time-codes/#parsing
constexpr char kOtpFormatRegex[] = "(?:^|\\s)@([a-zA-Z0-9.-]+) #([^#\\s]+)";

SmsParser::Result::Result(SmsParsingStatus status) : parsing_status(status) {
  DCHECK(parsing_status != SmsParsingStatus::kParsed);
}
SmsParser::Result::Result(const url::Origin& origin,
                          const std::string& one_time_code)
    : origin(std::move(origin)), one_time_code(one_time_code) {
  parsing_status = SmsParsingStatus::kParsed;
}

SmsParser::Result::~Result() {}

// static
SmsParser::Result SmsParser::Parse(base::StringPiece sms) {
  std::string url, otp;
  // TODO(yigu): The existing kOtpFormatRegex may filter out invalid SMSes that
  // would fall into |kHostAndPortNotParsed| or |kGURLNotValid| below. We should
  // clean up the code if the statement is confirmed by metrics.
  if (!re2::RE2::PartialMatch(sms.as_string(), kOtpFormatRegex, &url, &otp))
    return Result(SmsParsingStatus::kOTPFormatRegexNotMatch);

  std::string host, scheme;
  int port;
  if (!net::ParseHostAndPort(url, &host, &port))
    return Result(SmsParsingStatus::kHostAndPortNotParsed);

  // Expect localhost to always be http.
  if (net::HostStringIsLocalhost(base::StringPiece(host))) {
    scheme = "http://";
  } else {
    scheme = "https://";
  }

  GURL gurl(scheme + url);

  if (!gurl.is_valid())
    return Result(SmsParsingStatus::kGURLNotValid);

  return Result(url::Origin::Create(gurl), otp);
}

}  // namespace content
