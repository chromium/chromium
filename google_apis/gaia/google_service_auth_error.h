// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A GoogleServiceAuthError is immutable, plain old data representing an
// error from an attempt to authenticate with a Google service.
// It could be from Google Accounts itself, or any service using Google
// Accounts (e.g expired credentials).  It may contain additional data such as
// captcha or OTP challenges.

#ifndef GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
#define GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_

#include <string>

#include "base/component_export.h"
#include "url/gurl.h"

class COMPONENT_EXPORT(GOOGLE_APIS) GoogleServiceAuthError {
 public:
  //
  // These enumerations are referenced by integer value in HTML login code and
  // in UMA histograms. Do not change the numeric values.
  //
  enum State {
    // The user is authenticated.
    NONE = 0,

    // The credentials supplied to GAIA were either invalid, or the locally
    // cached credentials have expired.
    INVALID_GAIA_CREDENTIALS = 1,

    // Chrome does not have credentials (tokens) for this account.
    USER_NOT_SIGNED_UP = 2,

    // Could not connect to server to verify credentials. This could be in
    // response to either failure to connect to GAIA or failure to connect to
    // the service needing GAIA tokens during authentication.
    CONNECTION_FAILED = 3,

    // DEPRECATED.
    // The user needs to satisfy a CAPTCHA challenge to unlock their account.
    // If no other information is available, this can be resolved by visiting
    // https://accounts.google.com/DisplayUnlockCaptcha. Otherwise, captcha()
    // will provide details about the associated challenge.
    // CAPTCHA_REQUIRED = 4,

    // DEPRECATED.
    // The user account has been deleted.
    // ACCOUNT_DELETED = 5,

    // DEPRECATED.
    // The user account has been disabled.
    // ACCOUNT_DISABLED = 6,

    // The service is not available; try again later.
    SERVICE_UNAVAILABLE = 7,

    // DEPRECATED.
    // The password is valid but we need two factor to get a token.
    // TWO_FACTOR = 8,

    // The requestor of the authentication step cancelled the request
    // prior to completion.
    REQUEST_CANCELED = 9,

    // HOSTED accounts are deprecated.
    // HOSTED_NOT_ALLOWED_DEPRECATED = 10,

    // Indicates the service responded to a request, but we cannot
    // interpret the response.
    UNEXPECTED_SERVICE_RESPONSE = 11,

    // Indicates the service responded and response carried details of the
    // application error.
    SERVICE_ERROR = 12,

    // DEPRECATED.
    // The password is valid but web login is required to get a token.
    // WEB_LOGIN_REQUIRED = 13,

    // Indicates the service responded with an error that is bound to the scopes
    // that are in the request.
    SCOPE_LIMITED_UNRECOVERABLE_ERROR = 14,

    // Indicates the service responded with a challenge that should be signed
    // with a binding key and sent back.
    CHALLENGE_RESPONSE_REQUIRED = 15,

    // The number of known error states.
    NUM_STATES = 16,
  };

  static constexpr size_t kDeprecatedStateCount = 6;

  // Error reason for invalid credentials. Only used when the error is
  // INVALID_GAIA_CREDENTIALS.
  // Used by UMA histograms: do not remove or reorder values, add new values at
  // the end.
  enum class InvalidGaiaCredentialsReason {
    // The error was not specified.
    UNKNOWN = 0,
    // Credentials were rejectedby the Gaia server.
    CREDENTIALS_REJECTED_BY_SERVER,
    // Credentials were invalidated locally by Chrome.
    CREDENTIALS_REJECTED_BY_CLIENT,
    // Credentials are missing (e.g. could not be loaded from disk).
    CREDENTIALS_MISSING,

    NUM_REASONS
  };

  bool operator==(const GoogleServiceAuthError &b) const;
  bool operator!=(const GoogleServiceAuthError &b) const;

  // Construct a GoogleServiceAuthError from a State with no additional data.
  explicit GoogleServiceAuthError(State s);

  // Equivalent to calling GoogleServiceAuthError(NONE). Needs to exist and be
  // public for Mojo bindings code.
  GoogleServiceAuthError();

  GoogleServiceAuthError(const GoogleServiceAuthError& other);
  GoogleServiceAuthError& operator=(const GoogleServiceAuthError& other);

  // Construct a GoogleServiceAuthError from a network error.
  // It will be created with CONNECTION_FAILED set.
  static GoogleServiceAuthError FromConnectionError(int error);

  static GoogleServiceAuthError FromInvalidGaiaCredentialsReason(
      InvalidGaiaCredentialsReason reason);

  static GoogleServiceAuthError FromServiceUnavailable(
      const std::string& error_message);

  static GoogleServiceAuthError FromScopeLimitedUnrecoverableError(
      const std::string& error_message);

  // Construct a SERVICE_ERROR error, e.g. invalid client ID, with an
  // |error_message| which provides more information about the service error.
  static GoogleServiceAuthError FromServiceError(
      const std::string& error_message);

  // Construct an UNEXPECTED_SERVICE_RESPONSE error, with an |error_message|
  // detailing the problems with the response.
  static GoogleServiceAuthError FromUnexpectedServiceResponse(
      const std::string& error_message);

  // Construct a CHALLENGE_RESPONSE_REQUIRED error, with `challenge` containing
  // an opaque string that should be signed with the binding key.
  static GoogleServiceAuthError FromTokenBindingChallenge(
      const std::string& challenge);

  // Provided for convenience for clients needing to reset an instance to NONE.
  // (avoids err_ = GoogleServiceAuthError(GoogleServiceAuthError::NONE), due
  // to explicit class and State enum relation. Note: shouldn't be inlined!
  static GoogleServiceAuthError AuthErrorNone();

  static bool IsValid(State state);

  // The error information.
  State state() const;
  int network_error() const;
  const std::string& error_message() const;

  // Should only be used when the error state is CHALLENGE_RESPONSE_REQUIRED.
  const std::string& GetTokenBindingChallenge() const;

  // Should only be used when the error state is INVALID_GAIA_CREDENTIALS.
  InvalidGaiaCredentialsReason GetInvalidGaiaCredentialsReason() const;

  // Returns a message describing the error.
  std::string ToString() const;

  // In contrast to transient errors, errors in this category imply that
  // authentication shouldn't simply be retried. The error can be:
  // - User recoverable: persistent error that can be fixed by user action
  // (e.g. Sign in).
  // - Scope error: persistent error that is bound to the scopes in the access
  // token request and can't be fixed by user action.
  bool IsPersistentError() const;

  // Persistent error that is bound to the scopes in the access
  // token request and can't be fixed by user action. Authentication should not
  // be retried.
  bool IsScopePersistentError() const;

  // Check if this is error may go away simply by trying again.
  // Except for the NONE case, errors are either transient or persistent but not
  // both.
  bool IsTransientError() const;

 private:
  GoogleServiceAuthError(State s, int error);

  // Construct a GoogleServiceAuthError from |state| and |error_message|.
  GoogleServiceAuthError(State state, const std::string& error_message);

  State state_;
  int network_error_;
  std::string error_message_;
  std::string token_binding_challenge_;
  InvalidGaiaCredentialsReason invalid_gaia_credentials_reason_;
};

#endif  // GOOGLE_APIS_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
