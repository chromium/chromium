// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

constexpr base::StringPiece kToken = "For: ";
constexpr base::StringPiece kOneTimeCode = "otp";

SmsParser::Result::Result(const url::Origin& origin,
                          const std::string& one_time_code)
    : origin(std::move(origin)), one_time_code(one_time_code) {}

SmsParser::Result::~Result() {}

// static
base::Optional<SmsParser::Result> SmsParser::Parse(base::StringPiece sms) {
  size_t found = sms.rfind(kToken);

  if (found == base::StringPiece::npos) {
    return base::nullopt;
  }

  base::StringPiece url = sms.substr(found + kToken.length());

  GURL gurl(url);

  if (!gurl.is_valid())
    return base::nullopt;

  if (!(gurl.SchemeIs(url::kHttpsScheme) || net::IsLocalhost(gurl)))
    return base::nullopt;

  std::string one_time_code;

  if (!net::GetValueForKeyInQuery(gurl, kOneTimeCode.data(), &one_time_code))
    return base::nullopt;

  return Result(url::Origin::Create(gurl), one_time_code);
}

}  // namespace content
