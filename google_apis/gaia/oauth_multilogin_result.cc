// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"

namespace {

void RecordMultiloginResponseStatus(OAuthMultiloginResponseStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OAuthMultiloginResponseStatus", status);
}

}  // namespace

OAuthMultiloginResponseStatus ParseOAuthMultiloginResponseStatus(
    const std::string& status) {
  if (status == "OK")
    return OAuthMultiloginResponseStatus::kOk;
  if (status == "RETRY")
    return OAuthMultiloginResponseStatus::kRetry;
  if (status == "INVALID_TOKENS")
    return OAuthMultiloginResponseStatus::kInvalidTokens;
  if (status == "INVALID_INPUT")
    return OAuthMultiloginResponseStatus::kInvalidInput;
  if (status == "ERROR")
    return OAuthMultiloginResponseStatus::kError;

  return OAuthMultiloginResponseStatus::kUnknownStatus;
}

OAuthMultiloginResult::OAuthMultiloginResult(
    const OAuthMultiloginResult& other) {
  status_ = other.status();
  cookies_ = other.cookies();
  failed_gaia_ids_ = other.failed_gaia_ids();
}

OAuthMultiloginResult& OAuthMultiloginResult::operator=(
    const OAuthMultiloginResult& other) {
  status_ = other.status();
  cookies_ = other.cookies();
  failed_gaia_ids_ = other.failed_gaia_ids();
  return *this;
}

OAuthMultiloginResult::OAuthMultiloginResult(
    OAuthMultiloginResponseStatus status)
    : status_(status) {}

// static
base::StringPiece OAuthMultiloginResult::StripXSSICharacters(
    const std::string& raw_data) {
  base::StringPiece body(raw_data);
  return body.substr(body.find('\n'));
}

void OAuthMultiloginResult::TryParseFailedAccountsFromValue(
    base::Value* json_value) {
  base::Value* failed_accounts = json_value->FindListKey("failed_accounts");
  if (failed_accounts == nullptr) {
    VLOG(1) << "No invalid accounts found in the response but error is set to "
               "INVALID_TOKENS";
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  for (auto& account : failed_accounts->GetList()) {
    const std::string* gaia_id = account.FindStringKey("obfuscated_id");
    const std::string* status = account.FindStringKey("status");
    if (status && gaia_id && *status != "OK")
      failed_gaia_ids_.push_back(*gaia_id);
  }
  if (failed_gaia_ids_.empty())
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
}

void OAuthMultiloginResult::TryParseCookiesFromValue(base::Value* json_value) {
  base::Value* cookie_list = json_value->FindListKey("cookies");
  if (cookie_list == nullptr) {
    VLOG(1) << "No cookies found in the response.";
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  for (const auto& cookie : cookie_list->GetList()) {
    const std::string* name = cookie.FindStringKey("name");
    const std::string* value = cookie.FindStringKey("value");
    const std::string* domain = cookie.FindStringKey("domain");
    const std::string* host = cookie.FindStringKey("host");
    const std::string* path = cookie.FindStringKey("path");
    base::Optional<bool> is_secure = cookie.FindBoolKey("isSecure");
    base::Optional<bool> is_http_only = cookie.FindBoolKey("isHttpOnly");
    const std::string* priority = cookie.FindStringKey("priority");
    base::Optional<double> expiration_delta = cookie.FindDoubleKey("maxAge");
    const std::string* same_site = cookie.FindStringKey("sameSite");

    base::TimeDelta before_expiration =
        base::TimeDelta::FromSecondsD(expiration_delta.value_or(0.0));
    std::string cookie_domain = domain ? *domain : "";
    std::string cookie_host = host ? *host : "";
    if (cookie_domain.empty() && !cookie_host.empty() &&
        cookie_host[0] != '.') {
      // Host cookie case. If domain is empty but other conditions are not met,
      // there must be something wrong with the received cookie.
      cookie_domain = cookie_host;
    }
    net::CookieSameSite samesite_mode = net::CookieSameSite::UNSPECIFIED;
    net::CookieSameSiteString samesite_string =
        net::CookieSameSiteString::kUnspecified;
    if (same_site) {
      samesite_mode = net::StringToCookieSameSite(*same_site, &samesite_string);
    }
    net::RecordCookieSameSiteAttributeValueHistogram(samesite_string);
    net::CanonicalCookie new_cookie(
        name ? *name : "", value ? *value : "", cookie_domain,
        path ? *path : "", /*creation=*/base::Time::Now(),
        base::Time::Now() + before_expiration,
        /*last_access=*/base::Time::Now(), is_secure.value_or(true),
        is_http_only.value_or(true), samesite_mode,
        net::StringToCookiePriority(priority ? *priority : "medium"));
    if (new_cookie.IsCanonical()) {
      cookies_.push_back(std::move(new_cookie));
    } else {
      LOG(ERROR) << "Non-canonical cookie found.";
    }
  }
}

OAuthMultiloginResult::OAuthMultiloginResult(const std::string& raw_data) {
  base::StringPiece data = StripXSSICharacters(raw_data);
  status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
  base::Optional<base::Value> json_data = base::JSONReader::Read(data);
  if (!json_data) {
    RecordMultiloginResponseStatus(status_);
    return;
  }

  const std::string* status_string = json_data->FindStringKey("status");
  if (!status_string) {
    RecordMultiloginResponseStatus(status_);
    return;
  }

  status_ = ParseOAuthMultiloginResponseStatus(*status_string);
  if (status_ == OAuthMultiloginResponseStatus::kOk) {
    // Sets status_ to kUnknownStatus if cookies cannot be parsed.
    TryParseCookiesFromValue(&json_data.value());
  } else if (status_ == OAuthMultiloginResponseStatus::kInvalidTokens) {
    // Sets status_ to kUnknownStatus if failed accounts cannot be parsed.
    TryParseFailedAccountsFromValue(&json_data.value());
  }

  RecordMultiloginResponseStatus(status_);
}

OAuthMultiloginResult::~OAuthMultiloginResult() = default;
