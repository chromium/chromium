// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_LOGGING_H_
#define DEVICE_FIDO_WIN_LOGGING_H_

#include <windows.h>
#include <ostream>

#include "third_party/microsoft_webauthn/webauthn.h"

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_RP_ENTITY_INFORMATION& in);

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_USER_ENTITY_INFORMATION& in);

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_COSE_CREDENTIAL_PARAMETER& in);

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CLIENT_DATA& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIAL& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIALS& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIAL_EX& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIAL_LIST& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_EXTENSION& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_EXTENSIONS& in);

std::ostream& operator<<(
    std::ostream& out,
    const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS& in);

std::ostream& operator<<(
    std::ostream& out,
    const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS& in);

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_CREDENTIAL_ATTESTATION& in);

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_ASSERTION& in);

#endif  // DEVICE_FIDO_WIN_LOGGING_H_
