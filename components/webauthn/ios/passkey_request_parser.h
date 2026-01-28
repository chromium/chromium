// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_

#import "base/functional/function_ref.h"
#import "base/types/expected.h"
#import "base/values.h"
#import "components/webauthn/ios/passkey_request_params.h"

namespace webauthn {

// Events received from the Passkey JavaScript shim.
enum class PasskeyScriptEvent {
  kHandleGetRequest,
  kHandleCreateRequest,
  kLogGetRequest,
  kLogCreateRequest,
  kLogGetResolvedGpm,
  kLogGetResolvedNonGpm,
  kLogCreateResolvedGpm,
  kLogCreateResolvedNonGpm,
};

// Function to check if a credential exists in GPM.
using IsGpmPasskeyFunc =
    base::FunctionRef<bool(const std::string& rp_id,
                           const std::string& credential_id)>;

// List of errors which can be returned by the parsing methods below.
// LINT.IfChange(PasskeysParsingError)
enum class PasskeysParsingError {
  kMissingFrameId,
  kEmptyFrameId,
  kMissingRequestId,
  kEmptyRequestId,
  kMissingRequest,
  kMissingChallenge,
  kEmptyChallenge,
  kMalformedChallenge,
  kMissingRpEntity,
  kMissingRpId,
  kEmptyRpId,
  kMissingConditional,
  kMissingUserEntity,
  kMissingUserId,
  kEmptyUserId,
  kMalformedUserId,
  kMissingCredentialType,
  kMissingCredentialId,
  kEmptyCredentialId,
  kMalformedCredentialId,
  kMissingExtensions,
  kEvalByCredentialOnCreate,
  kMissingEvalByCredential,
  kMalformedEvalByCredential,
  kEvalByCredentialNotAllowed,
  kMissingFirstPRFInput,
  kMalformedFirstPRFInput,
  kMalformedSecondPRFInput,
  kPRFInputTooLarge,
  kMaxValue = kPRFInputTooLarge,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PasskeysParsingError)

// Builds a IOSPasskeyClient::RequestInfo object from the parameters contained
// in the provided dictionary.
base::expected<IOSPasskeyClient::RequestInfo, PasskeysParsingError>
BuildRequestInfo(const base::DictValue& dict);

// Builds an ExtractAssertionRequestParams object from the parameters contained
// in the provided dictionary.
base::expected<AssertionRequestParams, PasskeysParsingError>
BuildAssertionRequestParams(IOSPasskeyClient::RequestInfo request_info,
                            const base::DictValue& dict);

// Build a RegistrationRequestParams object from the parameters contained in the
// provided dictionary.
base::expected<RegistrationRequestParams, PasskeysParsingError>
BuildRegistrationRequestParams(IOSPasskeyClient::RequestInfo request_info,
                               const base::DictValue& dict);

// Converts an ExtensionOutputData object to the
// AuthenticationExtensionsClientOutputsJSON structure defined in
// passkey_controller.ts.
base::DictValue ToAuthenticationExtensionsClientOutputsJSON(
    passkey_model_utils::ExtensionOutputData extension_output_data);

// Parses the event string into a strongly typed enum.
std::optional<PasskeyScriptEvent> ParsePasskeyScriptEvent(
    const base::DictValue& dict,
    IsGpmPasskeyFunc is_gpm_passkey_func);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
