// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_RESPONSE_H_
#define GOOGLE_APIS_GAIA_OAUTH2_RESPONSE_H_

// Enumerated constants of server responses, partially matching RFC 6749.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OAuth2Response)
enum class OAuth2Response {
  kUnknownError = 0,
  kOk = 1,
  kOkUnexpectedFormat = 2,
  kErrorUnexpectedFormat = 3,
  kInvalidRequest = 4,
  kInvalidClient = 5,
  kInvalidGrant = 6,
  kUnauthorizedClient = 7,
  kUnsuportedGrantType = 8,
  kInvalidScope = 9,
  kRestrictedClient = 10,
  kRateLimitExceeded = 11,
  kInternalFailure = 12,
  kAdminPolicyEnforced = 13,
  kAccessDenied = 14,
  kConsentRequired = 15,
  kTokenBindingChallenge = 16,
  kMaxValue = kTokenBindingChallenge,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:OAuth2Response)

#endif  // GOOGLE_APIS_GAIA_OAUTH2_RESPONSE_H_
