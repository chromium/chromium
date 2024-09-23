// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/cookies/cookie_constants.h"

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
std::string_view OAuthMultiloginResult::StripXSSICharacters(
    const std::string& raw_data) {
  std::string_view body(raw_data);
  return body.substr(std::min(body.find('\n'), body.size()));
}

void OAuthMultiloginResult::TryParseFailedAccountsFromValue(
    const base::Value::Dict& json_value) {
  const base::Value::List* failed_accounts =
      json_value.FindList("failed_accounts");
  if (failed_accounts == nullptr) {
    VLOG(1) << "No invalid accounts found in the response but error is set to "
               "INVALID_TOKENS";
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  for (auto& account : *failed_accounts) {
    const std::string* gaia_id = account.GetDict().FindString("obfuscated_id");
    const std::string* status = account.GetDict().FindString("status");
    if (status && gaia_id && *status != "OK")
      failed_gaia_ids_.push_back(*gaia_id);
  }
  if (failed_gaia_ids_.empty())
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
}

void OAuthMultiloginResult::TryParseCookiesFromValue(
    const base::Value::Dict& json_value) {
  const base::Value::List* cookie_list = json_value.FindList("cookies");
  if (cookie_list == nullptr) {
    VLOG(1) << "No cookies found in the response.";
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  for (const auto& cookie : *cookie_list) {
    const base::Value::Dict& cookie_dict = cookie.GetDict();
    const std::string* name = cookie_dict.FindString("name");
    const std::string* value = cookie_dict.FindString("value");
    const std::string* domain = cookie_dict.FindString("domain");
    const std::string* host = cookie_dict.FindString("host");
    const std::string* path = cookie_dict.FindString("path");
    std::optional<bool> is_secure = cookie_dict.FindBool("isSecure");
    std::optional<bool> is_http_only = cookie_dict.FindBool("isHttpOnly");
    const std::string* priority = cookie_dict.FindString("priority");
    std::optional<double> expiration_delta = cookie_dict.FindDouble("maxAge");
    const std::string* same_site = cookie_dict.FindString("sameSite");

    base::Time now = base::Time::Now();
    // TODO(crbug.com/40800807) If CreateSanitizedCookie were used below, this
    // wouldn't be needed and ValidateAndAdjustExpiryDate could be moved back
    // into anon namespace instead of being exposed as a static function.
    // Alternatly, if we were sure GAIA cookies wouldn't try to expire more
    // than 400 days in the future we wouldn't need this either.
    base::Time expiration = net::CanonicalCookie::ValidateAndAdjustExpiryDate(
        now + base::Seconds(expiration_delta.value_or(0.0)), now,
        net::CookieSourceScheme::kSecure);
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
    // TODO(crbug.com/40160040) Consider using CreateSanitizedCookie instead.
    std::unique_ptr<net::CanonicalCookie> new_cookie =
        net::CanonicalCookie::FromStorage(
            name ? *name : "", value ? *value : "", cookie_domain,
            path ? *path : "", /*creation=*/now, expiration,
            /*last_access=*/now, /*last_update=*/now, is_secure.value_or(true),
            is_http_only.value_or(true), samesite_mode,
            net::StringToCookiePriority(priority ? *priority : "medium"),
            /*partition_key=*/std::nullopt, net::CookieSourceScheme::kUnset,
            url::PORT_UNSPECIFIED, net::CookieSourceType::kOther);
    // If the unique_ptr is null, it means the cookie was not canonical.
    // FromStorage() also uses a less strict version of IsCanonical(), we need
    // to check the stricter version as well here.
    if (new_cookie && new_cookie->IsCanonical()) {
      cookies_.push_back(std::move(*new_cookie));
    } else {
      LOG(ERROR) << "Non-canonical cookie found.";
    }
  }
}

OAuthMultiloginResult::OAuthMultiloginResult(const std::string& raw_data) {
  std::string_view data = StripXSSICharacters(raw_data);
  status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
  std::optional<base::Value> json_data = base::JSONReader::Read(data);
  if (!json_data) {
    RecordMultiloginResponseStatus(status_);
    return;
  }

  const base::Value::Dict& json_dict = json_data->GetDict();
  const std::string* status_string = json_dict.FindString("status");
  if (!status_string) {
    RecordMultiloginResponseStatus(status_);
    return;
  }

  status_ = ParseOAuthMultiloginResponseStatus(*status_string);
  if (status_ == OAuthMultiloginResponseStatus::kOk) {
    // Sets status_ to kUnknownStatus if cookies cannot be parsed.
    TryParseCookiesFromValue(json_dict);
  } else if (status_ == OAuthMultiloginResponseStatus::kInvalidTokens) {
    // Sets status_ to kUnknownStatus if failed accounts cannot be parsed.
    TryParseFailedAccountsFromValue(json_dict);
  }

  RecordMultiloginResponseStatus(status_);
}

OAuthMultiloginResult::~OAuthMultiloginResult() = default;
