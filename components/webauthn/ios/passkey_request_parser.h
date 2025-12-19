// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_

#import "base/types/expected.h"
#import "base/values.h"
#import "components/webauthn/ios/passkey_request_params.h"

namespace webauthn {

// List of errors which can be returned by the parsing methods below.
enum class PasskeysParsingError {
  kMissingFrameId,
  kEmptyFrameId,
  kMissingRequestId,
  kEmptyRequestId,
  kMissingRequest,
};

// Builds a IOSPasskeyClient::RequestInfo object from the parameters contained
// in the provided dictionary.
base::expected<IOSPasskeyClient::RequestInfo, PasskeysParsingError>
BuildRequestInfo(const base::Value::Dict& dict);

// Builds an ExtractAssertionRequestParams object from the parameters contained
// in the provided dictionary.
base::expected<AssertionRequestParams, PasskeysParsingError>
BuildAssertionRequestParams(IOSPasskeyClient::RequestInfo request_info,
                            const base::Value::Dict& dict);

// Build a RegistrationRequestParams object from the parameters contained in the
// provided dictionary.
base::expected<RegistrationRequestParams, PasskeysParsingError>
BuildRegistrationRequestParams(IOSPasskeyClient::RequestInfo request_info,
                               const base::Value::Dict& dict);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
