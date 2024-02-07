// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FILTER_H_
#define DEVICE_FIDO_FILTER_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace device {
namespace fido_filter {

// This code is intended to allow Finch control of several cases that have
// cropped up over time:
//   * Disabling a device on the USB bus that immediately answers all requests,
//     thus stopping anything else from working.
//   * Filtering attestation from devices that are found to be sending
//     excessively identifying information.
//   * Filtering attestation from websites that are performing tight
//     allowlisting in a public context.
//   * Filtering IDs from devices that are using them to tunnel other protocols.
//
// Thus Finch can set the "json" parameter of "WebAuthenticationFilter" to a
// JSON string with the following structure:
//
// {
//   "filters": [{
//     "operation": matched against "ga" (getAssertion) or "mc" (makeCredential)
//     "rp_id": matched against the RP ID (or AppID via the U2F API). Can be a
//              list of such strings which matches if any element matches.
//     "device": matched against the GetDisplayName value of the authenticator
//     "id_type": matched against "cred" (credential IDs) or "user" (user IDs).
//                "cred" matches against the allowCredentials of
//                PublicKeyCredentialRequestOptions and the excludeCredentials
//                of PublicKeyCredentialCreationOptions. "user" matches against
//                the user.id of PublicKeyCredentialCreationOptions.
//     "id": matched against the uppercase hex of the given ID. Can be
//           a list of such strings which matches if any element matches.
//     "id_min_size": matches if <= to the ID length, in bytes.
//     "id_max_size": matches if >= to the ID length, in bytes.
//     "action": "allow", "block", or "no-attestation".
//   }, { ... }
// ]}
//
// The JSON is allowed to have trailing commas, unlike standard JSON.
//
// When strings are matched, it is using base/strings/pattern.h. Note the
// comment in that file well because it's more like a file glob than a regexp.
//
// The only required field is "action", but:
//   * "id_type" must be given if "id_min_size", "id_max_size", or "id" are.
//   * At least one of "device" or "rp_id" must be given.
//
// Any structural errors, or unknown keys, in the JSON cause a parse error and
// the filter fails open.
//
// A result of "block" rejects the action. If an action is blocked for all
// devices and doesn't specify an ID then it'll result in an immediate rejection
// of the WebAuthn Promise. Otherwise, a block causes an authenticator to be
// ignored, potentially hanging the request if all authenticators are ignored. A
// result of "allow" permits the action. (This can be useful to permit a narrow
// range of cases before blocking a wider range.) Lastly, a result of
// "no-attestation" causes attestation to be suppressed for makeCredential
// operations, unless the RP ID is listed in enterprise policy.
//
// At various points in the code, |Evaluate| can be called in order to process
// any configured filter. Before |Evaluate| can be called, |MaybeInitialize|
// must be called to check whether the filter has been updated.
//
// Processing stops at the first matching filter. If none match, |ALLOW| is
// returned.

// MaybeInitialize parses any update to the Finch-controlled filter.
COMPONENT_EXPORT(DEVICE_FIDO)
void MaybeInitialize();

// Operation enumerates the possible operations for calling |Evaluate|.
enum class Operation {
  MAKE_CREDENTIAL,
  GET_ASSERTION,
};

// Operation enumerates the possible types of IDs for calling |Evaluate|.
enum class IDType {
  CREDENTIAL_ID,
  USER_ID,
};

// Action enumerates the result of evaluating a set of filterse.
enum class Action {
  ALLOW = 1,
  NO_ATTESTATION = 2,
  BLOCK = 3,
};

// Evaluate consults any configured filters and returns the result of evaluating
// them. See above about the format of filters.
COMPONENT_EXPORT(DEVICE_FIDO)
Action Evaluate(Operation op,
                std::string_view rp_id,
                std::optional<std::string_view> device,
                std::optional<std::pair<IDType, base::span<const uint8_t>>> id);

// ScopedFilterForTesting sets the current filter JSON for the duration of its
// lifetime. It is a fatal error if |json| is ill-formed.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedFilterForTesting {
 public:
  enum class PermitInvalidJSON {
    kYes,
  };

  explicit ScopedFilterForTesting(std::string_view json);
  ScopedFilterForTesting(std::string_view json, PermitInvalidJSON);
  ~ScopedFilterForTesting();

 private:
  const std::optional<std::string> previous_json_;
};

// ParseForTesting returns true iff |json| is a well-formed filter.
COMPONENT_EXPORT(DEVICE_FIDO)
bool ParseForTesting(std::string_view json);

}  // namespace fido_filter
}  // namespace device

#endif  // DEVICE_FIDO_FILTER_H_
