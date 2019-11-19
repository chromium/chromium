// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_MANAGER_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_MANAGER_TYPE_CONVERTERS_H_

#include "base/optional.h"

#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class AuthenticatorSelectionCriteria;
class ArrayBufferOrArrayBufferView;
class CableAuthenticationData;
class CableRegistrationData;
class Credential;
class PublicKeyCredentialCreationOptions;
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialParameters;
class PublicKeyCredentialRequestOptions;
class PublicKeyCredentialRpEntity;
class PublicKeyCredentialUserEntity;
class UserVerificationRequirement;
}  // namespace blink

namespace mojo {

// blink::mojom::blink::CredentialManager --------------------------

template <>
struct TypeConverter<blink::mojom::blink::CredentialInfoPtr,
                     blink::Credential*> {
  static blink::mojom::blink::CredentialInfoPtr Convert(blink::Credential*);
};

template <>
struct TypeConverter<blink::Credential*,
                     blink::mojom::blink::CredentialInfoPtr> {
  static blink::Credential* Convert(
      const blink::mojom::blink::CredentialInfoPtr&);
};

// blink::mojom::blink::Authenticator ---------------------------------------
template <>
struct TypeConverter<blink::mojom::blink::CredentialManagerError,
                     blink::mojom::blink::AuthenticatorStatus> {
  static blink::mojom::blink::CredentialManagerError Convert(
      const blink::mojom::blink::AuthenticatorStatus&);
};

template <>
struct TypeConverter<Vector<uint8_t>, blink::ArrayBufferOrArrayBufferView> {
  static Vector<uint8_t> Convert(const blink::ArrayBufferOrArrayBufferView&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialType, String> {
  static blink::mojom::blink::PublicKeyCredentialType Convert(const String&);
};

template <>
struct TypeConverter<
    base::Optional<blink::mojom::blink::AuthenticatorTransport>,
    String> {
  static base::Optional<blink::mojom::blink::AuthenticatorTransport> Convert(
      const String&);
};

template <>
struct TypeConverter<String, blink::mojom::blink::AuthenticatorTransport> {
  static String Convert(const blink::mojom::blink::AuthenticatorTransport&);
};

template <>
struct TypeConverter<blink::mojom::blink::UserVerificationRequirement, String> {
  static blink::mojom::blink::UserVerificationRequirement Convert(
      const String&);
};

template <>
struct TypeConverter<blink::mojom::blink::AttestationConveyancePreference,
                     String> {
  static blink::mojom::blink::AttestationConveyancePreference Convert(
      const String&);
};

template <>
struct TypeConverter<blink::mojom::blink::AuthenticatorAttachment, String> {
  static blink::mojom::blink::AuthenticatorAttachment Convert(const String&);
};

template <>
struct TypeConverter<blink::mojom::blink::AuthenticatorSelectionCriteriaPtr,
                     blink::AuthenticatorSelectionCriteria*> {
  static blink::mojom::blink::AuthenticatorSelectionCriteriaPtr Convert(
      const blink::AuthenticatorSelectionCriteria*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialUserEntityPtr,
                     blink::PublicKeyCredentialUserEntity*> {
  static blink::mojom::blink::PublicKeyCredentialUserEntityPtr Convert(
      const blink::PublicKeyCredentialUserEntity*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialRpEntityPtr,
                     blink::PublicKeyCredentialRpEntity*> {
  static blink::mojom::blink::PublicKeyCredentialRpEntityPtr Convert(
      const blink::PublicKeyCredentialRpEntity*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialDescriptorPtr,
                     blink::PublicKeyCredentialDescriptor*> {
  static blink::mojom::blink::PublicKeyCredentialDescriptorPtr Convert(
      const blink::PublicKeyCredentialDescriptor*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialParametersPtr,
                     blink::PublicKeyCredentialParameters*> {
  static blink::mojom::blink::PublicKeyCredentialParametersPtr Convert(
      const blink::PublicKeyCredentialParameters*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr,
                     blink::PublicKeyCredentialCreationOptions*> {
  static blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr Convert(
      const blink::PublicKeyCredentialCreationOptions*);
};

template <>
struct TypeConverter<blink::mojom::blink::CableAuthenticationPtr,
                     blink::CableAuthenticationData*> {
  static blink::mojom::blink::CableAuthenticationPtr Convert(
      const blink::CableAuthenticationData*);
};

template <>
struct TypeConverter<blink::mojom::blink::CableRegistrationPtr,
                     blink::CableRegistrationData*> {
  static blink::mojom::blink::CableRegistrationPtr Convert(
      const blink::CableRegistrationData*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr,
                     blink::PublicKeyCredentialRequestOptions*> {
  static blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr Convert(
      const blink::PublicKeyCredentialRequestOptions*);
};

}  // namespace mojo

#endif  // CredentialManagerProxy_h
