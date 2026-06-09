// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_IDENTITY_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_IDENTITY_CREDENTIAL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
class Credential;
class CredentialCreationOptions;
class CredentialRequestOptions;
class ExecutionContext;

// Returns whether `CredentialRequestOptions options` contains a credential of
// digital-identity type.
//
// The return value is not affected by additional non-digital-identity
// credential types in `options`.
MODULES_EXPORT bool IsDigitalIdentityCredentialType(
    const CredentialRequestOptions& options);

// Returns whether `CredentialCreationOptions options` contains a credential of
// digital-identity type.
//
// The return value is not affected by additional non-digital-identity
// credential types in `options`.
MODULES_EXPORT bool IsDigitalIdentityCredentialType(
    const CredentialCreationOptions& options);

// Requests the digital-identity credential specified by `options`. Credentials
// are stored in external wallets, and not stored in the browser. Therefore, the
// browser will forward the request to the underlying platform to handle the
// communication with external sources.
MODULES_EXPORT void DiscoverDigitalIdentityCredentialFromExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialRequestOptions& options);

// Creates the digital-identity credential specified by `options`. Credentials
// are stored in external wallets, and not stored in the browser. Therefore, the
// browser will forward the request to the underlying platform to handle the
// communication with external sources.
MODULES_EXPORT void CreateDigitalIdentityCredentialInExternalSource(
    ScriptPromiseResolver<IDLNullable<Credential>>* resolver,
    const CredentialCreationOptions& options);

enum class DigitalCredentialExchangeType {
  kPresentation,
  kIssuance,
  kAny,
};

// Returns true if the given protocol is supported by the Digital Credentials
// API for the given call type. Also records a UseCounter for the protocol
// usage.
MODULES_EXPORT bool CheckSupportedProtocol(ExecutionContext* execution_context,
                                           const String& protocol,
                                           DigitalCredentialExchangeType type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_IDENTITY_CREDENTIAL_H_
