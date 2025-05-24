// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_ANDROID)
#include "google_apis/gaia/android/jni_headers/GoogleServiceAuthError_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

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
  }
}

const char* ScopeLimitedUnrecoverableErrorReasonToString(
    GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason reason) {
  using enum GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason;
  switch (reason) {
    case kInvalidGrantRaptError:
      return "invalid grant rapt error";
    case kInvalidScope:
      return "invalid scope";
    case kRestrictedClient:
      return "restricted client";
    case kAdminPolicyEnforced:
      return "admin policy enforced";
    case kRemoteConsentResolutionRequired:
      return "remote consent resolution required";
    case kAccessDenied:
      return "access denied";
  }
  NOTREACHED();
}

bool IsTransientError(GoogleServiceAuthError::State state) {
  switch (state) {
    // These are failures that are likely to succeed if tried again.
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED:
      return true;
    // Everything else will have the same result.
    default:
      return false;
  }
}
}  // namespace

GoogleServiceAuthError::GoogleServiceAuthError()
    : GoogleServiceAuthError(NONE) {}

GoogleServiceAuthError::GoogleServiceAuthError(State s)
    : GoogleServiceAuthError(s, std::string()) {}

GoogleServiceAuthError::GoogleServiceAuthError(State state,
                                               const std::string& error_message)
    : GoogleServiceAuthError(state,
                             (state == CONNECTION_FAILED) ? net::ERR_FAILED : 0,
                             std::nullopt,
                             error_message) {}

GoogleServiceAuthError::GoogleServiceAuthError(State s, int error)
    : GoogleServiceAuthError(s, error, std::nullopt, std::string()) {}

GoogleServiceAuthError::GoogleServiceAuthError(
    State s,
    int error,
    std::optional<ScopeLimitedUnrecoverableErrorReason> reason,
    const std::string& error_message)
    : state_(s),
      network_error_(error),
      error_message_(error_message),
      scope_limited_unrecoverable_error_reason_(reason) {
  CHECK(s != SCOPE_LIMITED_UNRECOVERABLE_ERROR ||
        scope_limited_unrecoverable_error_reason_.has_value())
      << "SCOPE_LIMITED_UNRECOVERABLE_ERROR type errors must provide a reason.";
}

GoogleServiceAuthError::GoogleServiceAuthError(
    const GoogleServiceAuthError& other) = default;

GoogleServiceAuthError& GoogleServiceAuthError::operator=(
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
GoogleServiceAuthError GoogleServiceAuthError::FromServiceUnavailable(
    const std::string& error_message) {
  return GoogleServiceAuthError(SERVICE_UNAVAILABLE, error_message);
}

// static
GoogleServiceAuthError
GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
    ScopeLimitedUnrecoverableErrorReason reason) {
  GoogleServiceAuthError error(SCOPE_LIMITED_UNRECOVERABLE_ERROR, 0, reason,
                               std::string());
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
GoogleServiceAuthError GoogleServiceAuthError::FromTokenBindingChallenge(
    const std::string& challenge) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError(CHALLENGE_RESPONSE_REQUIRED, /*error=*/0);
  error.token_binding_challenge_ = challenge;
  return error;
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
    case SCOPE_LIMITED_UNRECOVERABLE_ERROR:
    case CHALLENGE_RESPONSE_REQUIRED:
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

const std::string& GoogleServiceAuthError::GetTokenBindingChallenge() const {
  CHECK_EQ(CHALLENGE_RESPONSE_REQUIRED, state());
  return token_binding_challenge_;
}

GoogleServiceAuthError::InvalidGaiaCredentialsReason
GoogleServiceAuthError::GetInvalidGaiaCredentialsReason() const {
  CHECK_EQ(INVALID_GAIA_CREDENTIALS, state());
  return invalid_gaia_credentials_reason_;
}

GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason
GoogleServiceAuthError::GetScopeLimitedUnrecoverableErrorReason() const {
  CHECK_EQ(SCOPE_LIMITED_UNRECOVERABLE_ERROR, state());
  CHECK(scope_limited_unrecoverable_error_reason_.has_value());
  return scope_limited_unrecoverable_error_reason_.value();
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
    case SCOPE_LIMITED_UNRECOVERABLE_ERROR:
      CHECK(scope_limited_unrecoverable_error_reason_.has_value());
      return base::StringPrintf(
          "OAuth scope error (%s).",
          ScopeLimitedUnrecoverableErrorReasonToString(
              scope_limited_unrecoverable_error_reason_.value()));
    case CHALLENGE_RESPONSE_REQUIRED:
      return "Service responded with a token binding challenge.";
    case NUM_STATES:
      NOTREACHED();
  }
}

bool GoogleServiceAuthError::IsPersistentError() const {
  if (state_ == GoogleServiceAuthError::NONE) return false;
  return !IsTransientError();
}

bool GoogleServiceAuthError::IsScopePersistentError() const {
  return state_ == GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR;
}

bool GoogleServiceAuthError::IsTransientError() const {
  return ::IsTransientError(state_);
}

#if BUILDFLAG(IS_ANDROID)
// static
GoogleServiceAuthError GoogleServiceAuthError::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_auth_error) {
  CHECK(j_auth_error);
  GoogleServiceAuthError::State state =
      static_cast<GoogleServiceAuthError::State>(
          Java_GoogleServiceAuthError_getState(env, j_auth_error));
  if (state == GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR) {
    // Android doesn't provide reasons for this type of errors and only creates
    // them for enterprise policy enforced scopes. So we hardcode the value
    // here.
    return GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
        GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason::
            kAdminPolicyEnforced);
  } else {
    return GoogleServiceAuthError(state);
  }
}

jni_zero::ScopedJavaLocalRef<jobject> GoogleServiceAuthError::ToJavaObject(
    JNIEnv* env) const {
  return Java_GoogleServiceAuthError_Constructor(env, state_);
}

jboolean JNI_GoogleServiceAuthError_IsTransientError(JNIEnv* env, jint state) {
  return IsTransientError(static_cast<GoogleServiceAuthError::State>(state));
}
#endif  // BUILDFLAG(IS_ANDROID)
