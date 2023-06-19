// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_MANAGER_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_MANAGER_TYPE_CONVERTERS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class AuthenticationExtensionsClientInputs;
class AuthenticationExtensionsDevicePublicKeyInputs;
class AuthenticationExtensionsPRFInputs;
class AuthenticationExtensionsPRFValues;
class AuthenticatorSelectionCriteria;
class CableAuthenticationData;
class Credential;
class IdentityCredentialLogoutRPsRequest;
class IdentityProviderConfig;
class IdentityUserInfo;
class PublicKeyCredentialCreationOptions;
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialParameters;
class PublicKeyCredentialRequestOptions;
class PublicKeyCredentialRpEntity;
class PublicKeyCredentialUserEntity;
class RemoteDesktopClientOverride;
class UserVerificationRequirement;
class V8IdentityCredentialRequestOptionsContext;
class V8UnionArrayBufferOrArrayBufferView;
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
struct TypeConverter<Vector<uint8_t>,
                     blink::V8UnionArrayBufferOrArrayBufferView*> {
  static Vector<uint8_t> Convert(
      const blink::V8UnionArrayBufferOrArrayBufferView*);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialType, String> {
  static blink::mojom::blink::PublicKeyCredentialType Convert(const String&);
};

template <>
struct TypeConverter<
    absl::optional<blink::mojom::blink::AuthenticatorTransport>,
    String> {
  static absl::optional<blink::mojom::blink::AuthenticatorTransport> Convert(
      const String&);
};

template <>
struct TypeConverter<String, blink::mojom::blink::AuthenticatorTransport> {
  static String Convert(const blink::mojom::blink::AuthenticatorTransport&);
};

template <>
struct TypeConverter<
    absl::optional<blink::mojom::blink::ResidentKeyRequirement>,
    String> {
  static absl::optional<blink::mojom::blink::ResidentKeyRequirement> Convert(
      const String&);
};

template <>
struct TypeConverter<
    absl::optional<blink::mojom::blink::UserVerificationRequirement>,
    String> {
  static absl::optional<blink::mojom::blink::UserVerificationRequirement>
  Convert(const String&);
};

template <>
struct TypeConverter<
    absl::optional<blink::mojom::blink::AttestationConveyancePreference>,
    String> {
  static absl::optional<blink::mojom::blink::AttestationConveyancePreference>
  Convert(const String&);
};

template <>
struct TypeConverter<
    absl::optional<blink::mojom::blink::AuthenticatorAttachment>,
    absl::optional<String>> {
  static absl::optional<blink::mojom::blink::AuthenticatorAttachment> Convert(
      const absl::optional<String>&);
};

template <>
struct TypeConverter<blink::mojom::blink::LargeBlobSupport,
                     absl::optional<String>> {
  static blink::mojom::blink::LargeBlobSupport Convert(
      const absl::optional<String>&);
};

template <>
struct TypeConverter<blink::mojom::blink::AuthenticatorSelectionCriteriaPtr,
                     blink::AuthenticatorSelectionCriteria> {
  static blink::mojom::blink::AuthenticatorSelectionCriteriaPtr Convert(
      const blink::AuthenticatorSelectionCriteria&);
};

template <>
struct TypeConverter<blink::mojom::blink::LogoutRpsRequestPtr,
                     blink::IdentityCredentialLogoutRPsRequest> {
  static blink::mojom::blink::LogoutRpsRequestPtr Convert(
      const blink::IdentityCredentialLogoutRPsRequest&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialUserEntityPtr,
                     blink::PublicKeyCredentialUserEntity> {
  static blink::mojom::blink::PublicKeyCredentialUserEntityPtr Convert(
      const blink::PublicKeyCredentialUserEntity&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialRpEntityPtr,
                     blink::PublicKeyCredentialRpEntity> {
  static blink::mojom::blink::PublicKeyCredentialRpEntityPtr Convert(
      const blink::PublicKeyCredentialRpEntity&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialDescriptorPtr,
                     blink::PublicKeyCredentialDescriptor> {
  static blink::mojom::blink::PublicKeyCredentialDescriptorPtr Convert(
      const blink::PublicKeyCredentialDescriptor&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialParametersPtr,
                     blink::PublicKeyCredentialParameters> {
  static blink::mojom::blink::PublicKeyCredentialParametersPtr Convert(
      const blink::PublicKeyCredentialParameters&);
};

template <>
struct TypeConverter<blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr,
                     blink::PublicKeyCredentialCreationOptions> {
  static blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr Convert(
      const blink::PublicKeyCredentialCreationOptions&);
};

template <>
struct TypeConverter<blink::mojom::blink::CableAuthenticationPtr,
                     blink::CableAuthenticationData> {
  static blink::mojom::blink::CableAuthenticationPtr Convert(
      const blink::CableAuthenticationData&);
};

template <>
struct MODULES_EXPORT
    TypeConverter<blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr,
                  blink::PublicKeyCredentialRequestOptions> {
  static blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr Convert(
      const blink::PublicKeyCredentialRequestOptions&);
};

template <>
struct MODULES_EXPORT
    TypeConverter<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr,
                  blink::AuthenticationExtensionsClientInputs> {
  static blink::mojom::blink::AuthenticationExtensionsClientInputsPtr Convert(
      const blink::AuthenticationExtensionsClientInputs&);
};

template <>
struct TypeConverter<blink::mojom::blink::RemoteDesktopClientOverridePtr,
                     blink::RemoteDesktopClientOverride> {
  static blink::mojom::blink::RemoteDesktopClientOverridePtr Convert(
      const blink::RemoteDesktopClientOverride&);
};

template <>
struct TypeConverter<blink::mojom::blink::IdentityProviderConfigPtr,
                     blink::IdentityProviderConfig> {
  static blink::mojom::blink::IdentityProviderConfigPtr Convert(
      const blink::IdentityProviderConfig&);
};

template <>
struct TypeConverter<blink::mojom::blink::IdentityProviderPtr,
                     blink::IdentityProviderConfig> {
  static blink::mojom::blink::IdentityProviderPtr Convert(
      const blink::IdentityProviderConfig&);
};

template <>
struct MODULES_EXPORT
    TypeConverter<blink::mojom::blink::RpContext,
                  blink::V8IdentityCredentialRequestOptionsContext> {
  static blink::mojom::blink::RpContext Convert(
      const blink::V8IdentityCredentialRequestOptionsContext&);
};

template <>
struct TypeConverter<blink::mojom::blink::IdentityUserInfoPtr,
                     blink::IdentityUserInfo> {
  static blink::mojom::blink::IdentityUserInfoPtr Convert(
      const blink::IdentityUserInfo&);
};

template <>
struct TypeConverter<blink::mojom::blink::DevicePublicKeyRequestPtr,
                     blink::AuthenticationExtensionsDevicePublicKeyInputs> {
  static blink::mojom::blink::DevicePublicKeyRequestPtr Convert(
      const blink::AuthenticationExtensionsDevicePublicKeyInputs&);
};

template <>
struct TypeConverter<blink::mojom::blink::PRFValuesPtr,
                     blink::AuthenticationExtensionsPRFValues> {
  static StructPtr<blink::mojom::blink::PRFValues> Convert(
      const blink::AuthenticationExtensionsPRFValues&);
};

template <>
struct TypeConverter<Vector<blink::mojom::blink::PRFValuesPtr>,
                     blink::AuthenticationExtensionsPRFInputs> {
  static Vector<StructPtr<blink::mojom::blink::PRFValues>> Convert(
      const blink::AuthenticationExtensionsPRFInputs&);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_MANAGER_TYPE_CONVERTERS_H_
