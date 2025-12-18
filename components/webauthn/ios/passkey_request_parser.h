// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_

#import "base/values.h"
#import "components/webauthn/ios/passkey_request_params.h"

namespace webauthn {

// Extracts all parameters required to build an ExtractAssertionRequestParams
// object from the provided dictionary.
AssertionRequestParams ExtractAssertionRequestParams(
    const base::Value::Dict& dict);

// Extracts all parameters required to build a RegistrationRequestParams object
// from the provided dictionary.
RegistrationRequestParams ExtractRegistrationRequestParams(
    const base::Value::Dict& dict);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
