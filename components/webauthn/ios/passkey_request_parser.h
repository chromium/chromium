// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_

#import "base/values.h"
#import "components/webauthn/ios/passkey_request_params.h"

namespace webauthn {

// Builds an ExtractAssertionRequestParams object from the parameters contained
// in the provided dictionary.
AssertionRequestParams BuildAssertionRequestParams(
    const base::Value::Dict& dict);

// Build a RegistrationRequestParams object from the parameters contained in the
// provided dictionary.
RegistrationRequestParams BuildRegistrationRequestParams(
    const base::Value::Dict& dict);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_REQUEST_PARSER_H_
