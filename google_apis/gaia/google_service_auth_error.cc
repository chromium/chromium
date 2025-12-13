// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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

GoogleServiceAuthError::GoogleServiceAuthError() : details_(None()) {}

GoogleServiceAuthError::GoogleServiceAuthError(State s) {
  switch (s) {
    case NONE:
      details_.emplace<None>();
      break;
    case INVALID_GAIA_CREDENTIALS:
      details_.emplace<InvalidGaiaCredentials>();
      break;
    case USER_NOT_SIGNED_UP:
      details_.emplace<UserNotSignedUp>();
      break;
    case CONNECTION_FAILED:
      details_.emplace<ConnectionFailed>();
      break;
    case SERVICE_UNAVAILABLE:
      details_.emplace<ServiceUnavailable>();
      break;
    case REQUEST_CANCELED:
      details_.emplace<RequestCanceled>();
      break;
    case UNEXPECTED_SERVICE_RESPONSE:
      details_.emplace<UnexpectedServiceResponse>();
      break;
    case SERVICE_ERROR:
      details_.emplace<ServiceError>();
      break;
    case SCOPE_LIMITED_UNRECOVERABLE_ERROR:
      details_.emplace<ScopeLimitedUnrecoverableError>();
      break;
    case CHALLENGE_RESPONSE_REQUIRED:
      details_.emplace<ChallengeResponseRequired>();
      break;
    case NUM_STATES:
      NOTREACHED();
  }
}

GoogleServiceAuthError::GoogleServiceAuthError(
    const GoogleServiceAuthError& other) = default;

GoogleServiceAuthError& GoogleServiceAuthError::operator=(
    const GoogleServiceAuthError& other) = default;

GoogleServiceAuthError::~GoogleServiceAuthError() = default;

// static
GoogleServiceAuthError GoogleServiceAuthError::FromConnectionError(int error) {
  return GoogleServiceAuthError(Details(
      ConnectionFailed{.network_error = static_cast<net::Error>(error)}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
    InvalidGaiaCredentialsReason reason) {
  return GoogleServiceAuthError(
      Details(InvalidGaiaCredentials{.reason = reason}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromServiceUnavailable(
    const std::string& error_message) {
  return GoogleServiceAuthError(
      Details(ServiceUnavailable{.error_message = error_message}));
}

// static
GoogleServiceAuthError
GoogleServiceAuthError::FromScopeLimitedUnrecoverableErrorReason(
    ScopeLimitedUnrecoverableErrorReason reason) {
  return GoogleServiceAuthError(
      Details(ScopeLimitedUnrecoverableError{.reason = reason}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromServiceError(
    const std::string& error_message) {
  return GoogleServiceAuthError(
      Details(ServiceError{.error_message = error_message}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromUnexpectedServiceResponse(
    const std::string& error_message) {
  return GoogleServiceAuthError(
      Details(UnexpectedServiceResponse{.error_message = error_message}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::FromTokenBindingChallenge(
    const std::string& challenge) {
  return GoogleServiceAuthError(
      Details(ChallengeResponseRequired{.token_binding_challenge = challenge}));
}

// static
GoogleServiceAuthError GoogleServiceAuthError::AuthErrorNone() {
  return GoogleServiceAuthError(Details(None()));
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
  return std::visit(
      absl::Overload{
          [](const None&) { return NONE; },
          [](const InvalidGaiaCredentials&) {
            return INVALID_GAIA_CREDENTIALS;
          },
          [](const UserNotSignedUp&) { return USER_NOT_SIGNED_UP; },
          [](const ConnectionFailed&) { return CONNECTION_FAILED; },
          [](const ServiceUnavailable&) { return SERVICE_UNAVAILABLE; },
          [](const RequestCanceled&) { return REQUEST_CANCELED; },
          [](const UnexpectedServiceResponse&) {
            return UNEXPECTED_SERVICE_RESPONSE;
          },
          [](const ServiceError&) { return SERVICE_ERROR; },
          [](const ScopeLimitedUnrecoverableError&) {
            return SCOPE_LIMITED_UNRECOVERABLE_ERROR;
          },
          [](const ChallengeResponseRequired&) {
            return CHALLENGE_RESPONSE_REQUIRED;
          },
      },
      details_);
}

const std::string& GoogleServiceAuthError::error_message() const {
  return std::visit(
      absl::Overload{[](const ServiceUnavailable& d) -> const std::string& {
                       return d.error_message;
                     },
                     [](const UnexpectedServiceResponse& d)
                         -> const std::string& { return d.error_message; },
                     [](const ServiceError& d) -> const std::string& {
                       return d.error_message;
                     },
                     [](const auto&) -> const std::string& {
                       return base::EmptyString();
                     }},
      details_);
}

net::Error GoogleServiceAuthError::GetNetworkError() const {
  const auto* connection_failed = std::get_if<ConnectionFailed>(&details_);
  CHECK(connection_failed);
  return connection_failed->network_error;
}

const std::string& GoogleServiceAuthError::GetTokenBindingChallenge() const {
  const auto* challenge_response_required =
      std::get_if<ChallengeResponseRequired>(&details_);
  CHECK(challenge_response_required);
  return challenge_response_required->token_binding_challenge;
}

GoogleServiceAuthError::InvalidGaiaCredentialsReason
GoogleServiceAuthError::GetInvalidGaiaCredentialsReason() const {
  const auto* invalid_gaia_credentials =
      std::get_if<InvalidGaiaCredentials>(&details_);
  CHECK(invalid_gaia_credentials);
  return invalid_gaia_credentials->reason;
}

GoogleServiceAuthError::ScopeLimitedUnrecoverableErrorReason
GoogleServiceAuthError::GetScopeLimitedUnrecoverableErrorReason() const {
  const auto* scope_limited_unrecoverable_error =
      std::get_if<ScopeLimitedUnrecoverableError>(&details_);
  CHECK(scope_limited_unrecoverable_error);
  return scope_limited_unrecoverable_error->reason;
}

std::string GoogleServiceAuthError::ToString() const {
  return std::visit(
      absl::Overload{
          [](const None&) { return std::string(); },
          [](const InvalidGaiaCredentials& invalid_gaia_credentials) {
            return base::StringPrintf("Invalid credentials (%s).",
                                      InvalidCredentialsReasonToString(
                                          invalid_gaia_credentials.reason));
          },
          [](const UserNotSignedUp&) { return std::string("Not authorized."); },
          [](const ConnectionFailed& connection_failed) {
            return base::StringPrintf("Connection failed (%d).",
                                      connection_failed.network_error);
          },
          [](const ServiceUnavailable& service_unavailable) {
            return base::StringPrintf(
                "Service unavailable; try again later (%s).",
                service_unavailable.error_message);
          },
          [](const RequestCanceled&) {
            return std::string("Request canceled.");
          },
          [](const UnexpectedServiceResponse& unexpected_service_response) {
            return base::StringPrintf(
                "Unexpected service response (%s)",
                unexpected_service_response.error_message.c_str());
          },
          [](const ServiceError& service_error) {
            return base::StringPrintf("Service responded with error: '%s'",
                                      service_error.error_message.c_str());
          },
          [](const ScopeLimitedUnrecoverableError&
                 scope_limited_unrecoverable_error) {
            return base::StringPrintf(
                "OAuth scope error (%s).",
                ScopeLimitedUnrecoverableErrorReasonToString(
                    scope_limited_unrecoverable_error.reason));
          },
          [](const ChallengeResponseRequired&) {
            return std::string(
                "Service responded with a token binding challenge.");
          },
      },
      details_);
}

bool GoogleServiceAuthError::IsPersistentError() const {
  if (state() == GoogleServiceAuthError::NONE) {
    return false;
  }
  return !IsTransientError();
}

bool GoogleServiceAuthError::IsScopePersistentError() const {
  return state() == GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR;
}

bool GoogleServiceAuthError::IsTransientError() const {
  return ::IsTransientError(state());
}

GoogleServiceAuthError::GoogleServiceAuthError(Details details)
    : details_(std::move(details)) {}

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
  return Java_GoogleServiceAuthError_Constructor(env, state());
}

static jboolean JNI_GoogleServiceAuthError_IsTransientError(JNIEnv* env,
                                                            jint state) {
  return IsTransientError(static_cast<GoogleServiceAuthError::State>(state));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(GoogleServiceAuthError)
#endif
