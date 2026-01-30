// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// SMS one-time-passcode format:
// https://wicg.github.io/sms-one-time-codes/#parsing
constexpr char kOtpFormatRegex[] =
    "(?:^|\\s)"                 // Leading characters
    "@([a-zA-Z0-9.-]+) "        // Domain
    "#([^#\\s]+)"               // OTP
    "(?: @([a-zA-Z0-9.-]+))?";  // Optional domain

using SmsParsingStatus = SmsParser::SmsParsingStatus;
using ParseDomainResult = std::tuple<SmsParsingStatus, GURL>;

ParseDomainResult ParseDomain(std::string_view domain) {
  std::string host;
  int port;
  if (!net::ParseHostAndPort(domain, &host, &port))
    return std::make_tuple(SmsParsingStatus::kHostAndPortNotParsed, GURL());

  std::string_view scheme;
  // Expect localhost to always be http.
  if (net::HostStringIsLocalhost(host)) {
    scheme = "http://";
  } else {
    scheme = "https://";
  }

  GURL gurl = GURL(base::StrCat({scheme, domain}));

  if (!gurl.is_valid())
    return std::make_tuple(SmsParsingStatus::kGURLNotValid, GURL());

  return std::make_tuple(SmsParsingStatus::kParsed, std::move(gurl));
}

}  // namespace

SmsParser::Result::Result(SmsParsingStatus status) : parsing_status(status) {
  DCHECK(parsing_status != SmsParsingStatus::kParsed);
}

SmsParser::Result::Result(url::Origin top_origin,
                          url::Origin embedded_origin,
                          std::string_view one_time_code)
    : top_origin(std::move(top_origin)),
      embedded_origin(std::move(embedded_origin)),
      one_time_code(std::string(one_time_code)),
      parsing_status(SmsParsingStatus::kParsed) {}

SmsParser::Result::Result(Result&&) = default;

SmsParser::Result& SmsParser::Result::operator=(Result&&) = default;

SmsParser::Result::~Result() = default;

SmsFetcher::OriginList SmsParser::Result::TakeOriginList() && {
  DCHECK(IsValid());
  SmsFetcher::OriginList origin_list;
  if (!embedded_origin.opaque())
    origin_list.emplace_back(std::move(embedded_origin));
  origin_list.emplace_back(std::move(top_origin));
  return origin_list;
}

// static
SmsParser::Result SmsParser::Parse(std::string_view sms) {
  std::string_view top_domain, otp, embedded_domain;
  // TODO(yigu): The existing kOtpFormatRegex may filter out invalid SMSes that
  // would fall into |kHostAndPortNotParsed| or |kGURLNotValid| below. We should
  // clean up the code if the statement is confirmed by metrics.
  if (!re2::RE2::PartialMatch(sms, kOtpFormatRegex, &top_domain, &otp,
                              &embedded_domain)) {
    return Result(SmsParsingStatus::kOTPFormatRegexNotMatch);
  }

  auto [top_domain_parsing_status, top_gurl] = ParseDomain(top_domain);
  if (top_domain_parsing_status != SmsParsingStatus::kParsed)
    return Result(top_domain_parsing_status);
  DCHECK(top_gurl.is_valid());

  url::Origin top_origin = url::Origin::Create(top_gurl);
  DCHECK(!top_origin.opaque());

  if (embedded_domain.empty()) {
    return Result(std::move(top_origin), url::Origin(), otp);
  }

  auto [embedded_domain_parsing_status, embedded_gurl] =
      ParseDomain(embedded_domain);
  if (embedded_domain_parsing_status != SmsParsingStatus::kParsed)
    return Result(embedded_domain_parsing_status);
  DCHECK(embedded_gurl.is_valid());

  url::Origin embedded_origin = url::Origin::Create(embedded_gurl);
  DCHECK(!embedded_origin.opaque());
  return Result(std::move(top_origin), std::move(embedded_origin), otp);
}

}  // namespace content
