// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"

namespace {
const char* InvalidCredentialsReasonToString(
    GoogleServiceAuthError::InvalidGaiaCredentialsReason reason) {
  using InvalidGaiaCredentialsReason =
      GoogleServiceAuthError::InvalidGaiaCredentialsReason;
  switch (reason) {
    case InvalidGaiaCredentialsReason::UNKNOWN:
      return "unknown";
    case InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_SERVER:
      return "credentials rejected by server";
    case InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_CLIENT:
      return "credentials rejected by client";
    case InvalidGaiaCredentialsReason::CREDENTIALS_MISSING:
      return "credentials missing";
    case InvalidGaiaCredentialsReason::NUM_REASONS:
      NOTREACHED();
      return "";
  }
}
}  // namespace

bool GoogleServiceAuthError::operator==(
    const GoogleServiceAuthError& b) const {
  return (state_ == b.state_) && (network_error_ == b.network_error_) &&
         (error_message_ == b.error_message_) &&
         (invalid_gaia_credentials_reason_ ==
          b.invalid_gaia_credentials_reason_);
}

bool GoogleServiceAuthError::operator!=(
    const GoogleServiceAuthError& b) const {
  return !(*this == b);
}

GoogleServiceAuthError::GoogleServiceAuthError()
    : GoogleServiceAuthError(NONE) {}

GoogleServiceAuthError::GoogleServiceAuthError(State s)
    : GoogleServiceAuthError(s, std::string()) {}

GoogleServiceAuthError::GoogleServiceAuthError(State state,
                                               const std::string& error_message)
    : GoogleServiceAuthError(
          state,
          (state == CONNECTION_FAILED) ? net::ERR_FAILED : 0) {
  error_message_ = error_message;
}

GoogleServiceAuthError::GoogleServiceAuthError(
    const GoogleServiceAuthError& other) = default;

// static
GoogleServiceAuthError
    GoogleServiceAuthError::FromConnectionError(int error) {
  return GoogleServiceAuthError(CONNECTION_FAILED, error);
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
    InvalidGaiaCredentialsReason reason) {
  GoogleServiceAuthError error(INVALID_GAIA_CREDENTIALS);
  error.invalid_gaia_credentials_reason_ = reason;
  return error;
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromServiceError(
    const std::string& error_message) {
  return GoogleServiceAuthError(SERVICE_ERROR, error_message);
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromUnexpectedServiceResponse(
    const std::string& error_message) {
  return GoogleServiceAuthError(UNEXPECTED_SERVICE_RESPONSE, error_message);
}

// static
GoogleServiceAuthError GoogleServiceAuthError::AuthErrorNone() {
  return GoogleServiceAuthError(NONE);
}

// static
bool GoogleServiceAuthError::IsValid(State state) {
  switch (state) {
    case NONE:
    case INVALID_GAIA_CREDENTIALS:
    case USER_NOT_SIGNED_UP:
    case CONNECTION_FAILED:
    case SERVICE_UNAVAILABLE:
    case REQUEST_CANCELED:
    case UNEXPECTED_SERVICE_RESPONSE:
    case SERVICE_ERROR:
      return true;
    case NUM_STATES:
      return false;
  }

  return false;
}

GoogleServiceAuthError::State GoogleServiceAuthError::state() const {
  return state_;
}

int GoogleServiceAuthError::network_error() const {
  return network_error_;
}

const std::string& GoogleServiceAuthError::error_message() const {
  return error_message_;
}

GoogleServiceAuthError::InvalidGaiaCredentialsReason
GoogleServiceAuthError::GetInvalidGaiaCredentialsReason() const {
  DCHECK_EQ(INVALID_GAIA_CREDENTIALS, state());
  return invalid_gaia_credentials_reason_;
}

std::string GoogleServiceAuthError::ToString() const {
  switch (state_) {
    case NONE:
      return std::string();
    case INVALID_GAIA_CREDENTIALS:
      return base::StringPrintf(
          "Invalid credentials (%s).",
          InvalidCredentialsReasonToString(invalid_gaia_credentials_reason_));
    case USER_NOT_SIGNED_UP:
      return "Not authorized.";
    case CONNECTION_FAILED:
      return base::StringPrintf("Connection failed (%d).", network_error_);
    case SERVICE_UNAVAILABLE:
      return "Service unavailable; try again later.";
    case REQUEST_CANCELED:
      return "Request canceled.";
    case UNEXPECTED_SERVICE_RESPONSE:
      return base::StringPrintf("Unexpected service response (%s)",
                                error_message_.c_str());
    case SERVICE_ERROR:
      return base::StringPrintf("Service responded with error: '%s'",
                                error_message_.c_str());
    case NUM_STATES:
      NOTREACHED();
      return std::string();
  }
}

bool GoogleServiceAuthError::IsPersistentError() const {
  if (state_ == GoogleServiceAuthError::NONE) return false;
  return !IsTransientError();
}

bool GoogleServiceAuthError::IsTransientError() const {
  switch (state_) {
  // These are failures that are likely to succeed if tried again.
  case GoogleServiceAuthError::CONNECTION_FAILED:
  case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
  case GoogleServiceAuthError::REQUEST_CANCELED:
    return true;
  // Everything else will have the same result.
  default:
    return false;
  }
}

GoogleServiceAuthError::GoogleServiceAuthError(State s, int error)
    : state_(s),
      network_error_(error),
      invalid_gaia_credentials_reason_(InvalidGaiaCredentialsReason::UNKNOWN) {}
