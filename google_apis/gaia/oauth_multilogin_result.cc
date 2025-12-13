// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/token_binding_response_encryption_error.h"
#include "net/cookies/cookie_constants.h"
#include "net/http/http_status_code.h"

namespace {

using DeviceBoundSession = ::OAuthMultiloginResult::DeviceBoundSession;

// Response body that has a form of JSON contains protection characters
// against XSSI that have to be removed. See go/xssi.
std::string_view StripXSSICharacters(std::string_view raw_data) {
  std::string_view body(raw_data);
  return body.substr(std::min(body.find('\n'), body.size()));
}

void RecordMultiloginResponseStatus(OAuthMultiloginResponseStatus status) {
  base::UmaHistogramEnumeration("Signin.OAuthMultiloginResponseStatus", status);
}

void RecordMultiloginResponseEncryptionError(
    TokenBindingResponseEncryptionError error) {
  base::UmaHistogramEnumeration("Signin.OAuthMultiloginResponseEncryptionError",
                                error);
}

void RecordDeviceBoundSessionParsingError(
    OAuthMultiloginDeviceBoundSessionParsingError error) {
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError", error);
}

void RecordDeviceBoundSessionUnknownDomainsCount(int count) {
  base::UmaHistogramCounts100(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain", count);
}

DeviceBoundSession::Domain ParseDeviceBoundSessionDomain(
    std::string_view domain) {
  if (base::EqualsCaseInsensitiveASCII(domain, "GOOGLE_COM")) {
    return DeviceBoundSession::Domain::kGoogle;
  }
  return DeviceBoundSession::Domain::kUnknown;
}

OAuthMultiloginDeviceBoundSessionParsingError
ConvertRegisterPayloadParserErrorToOAuthMultiloginError(
    RegisterBoundSessionPayload::ParserError error) {
  using FromEnum = RegisterBoundSessionPayload::ParserError;
  using ToEnum = OAuthMultiloginDeviceBoundSessionParsingError;

  switch (error) {
    case FromEnum::kRequiredFieldMissing:
      return ToEnum::kRegisterPayloadRequiredFieldMissing;
    case FromEnum::kRequiredCredentialFieldMissing:
      return ToEnum::kRegisterPayloadRequiredCredentialFieldMissing;
    case FromEnum::kMalformedRefreshInitiator:
      return ToEnum::kRegisterPayloadMalformedRefreshInitiator;
    case FromEnum::kMalformedSessionScopeSpecification:
      return ToEnum::kRegisterPayloadMalformedSessionScopeSpecification;
    case FromEnum::kRequiredScopeFieldMissing:
      return ToEnum::kRegisterPayloadRequiredScopeFieldMissing;
    case FromEnum::kInvalidScopeType:
      return ToEnum::kRegisterPayloadInvalidScopeType;
    case FromEnum::kInvalidCredentialType:
      return ToEnum::kRegisterPayloadInvalidCredentialType;
  }
}

}  // namespace

OAuthMultiloginResponseStatus ParseOAuthMultiloginResponseStatus(
    const std::string& status,
    int http_response_code) {
  if (status == "OK")
    return OAuthMultiloginResponseStatus::kOk;
  if (status == "RETRY")
    return http_response_code == net::HTTP_BAD_REQUEST
               ? OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge
               : OAuthMultiloginResponseStatus::kRetry;
  if (status == "INVALID_TOKENS")
    return OAuthMultiloginResponseStatus::kInvalidTokens;
  if (status == "INVALID_INPUT")
    return OAuthMultiloginResponseStatus::kInvalidInput;
  if (status == "ERROR")
    return OAuthMultiloginResponseStatus::kError;

  return OAuthMultiloginResponseStatus::kUnknownStatus;
}

OAuthMultiloginResult::OAuthMultiloginResult(
    OAuthMultiloginResponseStatus status)
    : status_(status) {}

void OAuthMultiloginResult::TryParseFailedAccountsFromValue(
    const base::Value::Dict& json_value) {
  CHECK(status_ == OAuthMultiloginResponseStatus::kInvalidTokens ||
        status_ ==
            OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge);
  const base::Value::List* failed_accounts =
      json_value.FindList("failed_accounts");
  if (!failed_accounts) {
    VLOG(1) << "No failed accounts found in the response. status_="
            << static_cast<int>(status_);
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  for (const auto& account : *failed_accounts) {
    const base::Value::Dict* account_dict = account.GetIfDict();
    if (!account_dict) {
      VLOG(1) << "failed_accounts list contained a malformed element, ignoring";
      continue;
    }
    const std::string* gaia_id = account_dict->FindString("obfuscated_id");
    const std::string* status = account_dict->FindString("status");
    if (!status || !gaia_id || *status == "OK") {
      continue;
    }

    const std::string* challenge = nullptr;
    if (status_ ==
            OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge &&
        *status == "RECOVERABLE") {
      challenge = account_dict->FindStringByDottedPath(
          "token_binding_retry_response.challenge");
    }

    failed_accounts_.push_back(OAuthMultiloginResult::FailedAccount{
        .gaia_id = GaiaId(*gaia_id),
        .token_binding_challenge = challenge ? *challenge : std::string()});
  }
  if (failed_accounts_.empty()) {
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
  }
}

std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
OAuthMultiloginResult::GetDeviceBoundSessionsToRegister() const {
  std::vector<const OAuthMultiloginResult::DeviceBoundSession*>
      bound_sessions_to_register;
  for (const OAuthMultiloginResult::DeviceBoundSession& bound_session :
       device_bound_sessions_) {
    if (bound_session.is_device_bound &&
        bound_session.register_session_payload.has_value()) {
      bound_sessions_to_register.push_back(&bound_session);
    }
  }
  return bound_sessions_to_register;
}

void OAuthMultiloginResult::TryParseCookiesFromValue(
    const base::Value::Dict& json_value,
    const CookieDecryptor& cookie_decryptor) {
  CHECK_EQ(status_, OAuthMultiloginResponseStatus::kOk);
  const base::Value::List* cookie_list = json_value.FindList("cookies");
  if (cookie_list == nullptr) {
    VLOG(1) << "No cookies found in the response.";
    status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
    return;
  }
  bool are_cookies_encrypted =
      json_value.Find("token_binding_directed_response") != nullptr;
  if (are_cookies_encrypted && cookie_decryptor.is_null()) {
    VLOG(1) << "The response unexpectedly contains encrypted cookies";
    for ([[maybe_unused]] const auto& unused : *cookie_list) {
      // Record the histogram once per cookie for parity with other buckets.
      RecordMultiloginResponseEncryptionError(
          TokenBindingResponseEncryptionError::kResponseUnexpectedlyEncrypted);
    }
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

    std::string cookie_value = value ? *value : "";
    if (!cookie_value.empty() && are_cookies_encrypted) {
      cookie_value = cookie_decryptor.Run(cookie_value);
      if (cookie_value.empty()) {
        VLOG(1) << "Failed to decrypt a cookie.";
        RecordMultiloginResponseEncryptionError(
            TokenBindingResponseEncryptionError::kDecryptionFailed);
        continue;
      } else {
        RecordMultiloginResponseEncryptionError(
            TokenBindingResponseEncryptionError::kSuccessfullyDecrypted);
      }
    } else {
      RecordMultiloginResponseEncryptionError(
          TokenBindingResponseEncryptionError::kSuccessNoEncryption);
    }

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
      std::tie(samesite_mode, samesite_string) =
          net::StringToCookieSameSite(*same_site);
    }
    net::RecordCookieSameSiteAttributeValueHistogram(samesite_string);
    // TODO(crbug.com/40160040) Consider using CreateSanitizedCookie instead.
    std::unique_ptr<net::CanonicalCookie> new_cookie =
        net::CanonicalCookie::FromStorage(
            name ? *name : "", cookie_value, cookie_domain, path ? *path : "",
            /*creation=*/now, expiration,
            /*last_access=*/now, /*last_update=*/now, is_secure.value_or(true),
            is_http_only.value_or(true), samesite_mode,
            net::StringToCookiePriority(priority ? *priority : "medium"),
            /*partition_key=*/std::nullopt, net::CookieSourceScheme::kUnset,
            url::PORT_UNSPECIFIED, net::CookieSourceType::kOther,
            net::CanonicalCookieFromStorageCallSite::kOauthMultiloginResult);
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

// TODO(crbug.com/312719798): Update the status to `kUnknownStatus` if failed to
// parse device-bound sessions.
void OAuthMultiloginResult::TryParseDeviceBoundSessionsFromValue(
    const base::Value::Dict& json_value,
    bool standard_device_bound_session_credentials) {
  CHECK_EQ(status_, OAuthMultiloginResponseStatus::kOk);
  const base::Value::List* device_bound_sessions_list =
      json_value.FindList("device_bound_session_info");
  if (!device_bound_sessions_list) {
    // No device-bound sessions info, nothing to do.
    return;
  }

  int unknown_domains_count = 0;
  std::vector<DeviceBoundSession> device_bound_sessions;
  for (const base::Value& device_bound_session_val :
       *device_bound_sessions_list) {
    const base::Value::Dict* device_bound_session_dict =
        device_bound_session_val.GetIfDict();
    if (!device_bound_session_dict) {
      continue;
    }

    if (!device_bound_session_dict->FindBool("is_device_bound")
             .value_or(false)) {
      // Not a device-bound session, nothing to do.
      continue;
    }

    DeviceBoundSession device_bound_session;
    device_bound_session.is_device_bound = true;

    const std::string* domain = device_bound_session_dict->FindString("domain");
    if (!domain) {
      // Malformed response, domain is required.
      RecordDeviceBoundSessionParsingError(
          OAuthMultiloginDeviceBoundSessionParsingError::kInvalidDomain);
      return;
    }
    device_bound_session.domain = ParseDeviceBoundSessionDomain(*domain);
    if (device_bound_session.domain == DeviceBoundSession::Domain::kUnknown) {
      // Unknown domain, nothing to do. This is not necessarily an error, as the
      // server might return a session for a domain that the client doesn't
      // recognize yet, we record the histogram to track the rate of such
      // events.
      ++unknown_domains_count;
      continue;
    }

    const base::Value::Dict* register_session_payload_dict =
        device_bound_session_dict->FindDict("register_session_payload");
    if (!register_session_payload_dict) {
      // Missing register session payload signals the client to reuse the
      // existing session.
      device_bound_sessions.push_back(std::move(device_bound_session));
      continue;
    }
    base::expected<RegisterBoundSessionPayload,
                   RegisterBoundSessionPayload::ParserError>
        register_session_payload = RegisterBoundSessionPayload::ParseFromJson(
            *register_session_payload_dict,
            standard_device_bound_session_credentials);
    if (!register_session_payload.has_value()) {
      // Malformed response, failed to parse register session payload.
      RecordDeviceBoundSessionParsingError(
          ConvertRegisterPayloadParserErrorToOAuthMultiloginError(
              register_session_payload.error()));
      return;
    }
    device_bound_session.register_session_payload =
        *std::move(register_session_payload);

    device_bound_sessions.push_back(std::move(device_bound_session));
  }

  device_bound_sessions_ = std::move(device_bound_sessions);

  RecordDeviceBoundSessionUnknownDomainsCount(unknown_domains_count);
  RecordDeviceBoundSessionParsingError(
      OAuthMultiloginDeviceBoundSessionParsingError::kNone);
}

OAuthMultiloginResult::OAuthMultiloginResult(
    const std::string& raw_data,
    int http_response_code,
    const CookieDecryptor& cookie_decryptor,
    bool standard_device_bound_session_credentials) {
  std::string_view data = StripXSSICharacters(raw_data);
  status_ = OAuthMultiloginResponseStatus::kUnknownStatus;
  std::optional<base::Value> json_data =
      base::JSONReader::Read(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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

  status_ =
      ParseOAuthMultiloginResponseStatus(*status_string, http_response_code);
  if (status_ == OAuthMultiloginResponseStatus::kOk) {
    // Sets `status_` to `kUnknownStatus` if cookies cannot be parsed.
    TryParseCookiesFromValue(json_dict, cookie_decryptor);
    // Check the `status_` again, as `TryParseCookiesFromValue` may have set it
    // to to `kUnknownStatus` if cookies cannot be parsed.
    if (status_ == OAuthMultiloginResponseStatus::kOk) {
      TryParseDeviceBoundSessionsFromValue(
          json_dict, standard_device_bound_session_credentials);
    }
  } else if (status_ == OAuthMultiloginResponseStatus::kInvalidTokens ||
             status_ == OAuthMultiloginResponseStatus::
                            kRetryWithTokenBindingChallenge) {
    // Sets status_ to `kUnknownStatus` if failed accounts cannot be parsed.
    TryParseFailedAccountsFromValue(json_dict);
  }

  RecordMultiloginResponseStatus(status_);
}

OAuthMultiloginResult::~OAuthMultiloginResult() = default;

DeviceBoundSession::DeviceBoundSession::DeviceBoundSession() = default;
DeviceBoundSession::DeviceBoundSession::~DeviceBoundSession() = default;

DeviceBoundSession::DeviceBoundSession(DeviceBoundSession&& other) = default;
DeviceBoundSession& DeviceBoundSession::operator=(DeviceBoundSession&& other) =
    default;
