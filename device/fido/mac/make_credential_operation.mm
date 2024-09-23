// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/make_credential_operation.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device::fido::mac {

MakeCredentialOperation::MakeCredentialOperation(
    CtapMakeCredentialRequest request,
    TouchIdCredentialStore* credential_store,
    Callback callback)
    : request_(std::move(request)),
      credential_store_(credential_store),
      callback_(std::move(callback)) {}

MakeCredentialOperation::~MakeCredentialOperation() = default;

void MakeCredentialOperation::Run() {
  if (!base::Contains(
          request_.public_key_credential_params.public_key_credential_params(),
          static_cast<int>(CoseAlgorithmIdentifier::kEs256),
          &PublicKeyCredentialParams::CredentialInfo::algorithm)) {
    FIDO_LOG(ERROR) << "No supported algorithm found";
    std::move(callback_).Run(MakeCredentialStatus::kNoCommonAlgorithms,
                             std::nullopt);
    return;
  }

  const bool require_uv = ProfileAuthenticatorWillDoUserVerification(
      request_.user_verification,
      device::fido::mac::DeviceHasBiometricsAvailable());
  if (require_uv) {
    touch_id_context_->PromptTouchId(
        l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                   base::UTF8ToUTF16(request_.rp.id)),
        base::BindOnce(&MakeCredentialOperation::PromptTouchIdDone,
                       base::Unretained(this)));
    return;
  }

  CreateCredential(/*has_uv=*/false);
}

void MakeCredentialOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback_).Run(MakeCredentialStatus::kUserConsentDenied,
                             std::nullopt);
    return;
  }

  // Setting an authentication context authorizes credentials returned from the
  // credential store for signing without triggering yet another Touch ID
  // prompt.
  credential_store_->SetAuthenticationContext(
      touch_id_context_->authentication_context());

  CreateCredential(/*has_uv=*/true);
}

void MakeCredentialOperation::CreateCredential(bool has_uv) {
  if (!request_.exclude_list.empty()) {
    std::optional<std::list<Credential>> credentials =
        credential_store_->FindCredentialsFromCredentialDescriptorList(
            request_.rp.id, request_.exclude_list);
    if (!credentials) {
      FIDO_LOG(ERROR) << "Failed to check for excluded credentials";
      std::move(callback_).Run(
          MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
      return;
    }
    if (!credentials->empty()) {
      std::move(callback_).Run(
          MakeCredentialStatus::kUserConsentButCredentialExcluded,
          std::nullopt);
      return;
    }
  }

  // Delete the key pair for this RP + user handle if one already exists.
  if (!credential_store_->DeleteCredentialsForUserId(request_.rp.id,
                                                     request_.user.id)) {
    FIDO_LOG(ERROR) << "DeleteCredentialsForUserId() failed";
    std::move(callback_).Run(
        MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
    return;
  }

  // Generate the new key pair.
  //
  // New credentials are always discoverable. But older non-discoverable
  // credentials may exist.
  std::optional<std::pair<Credential, base::apple::ScopedCFTypeRef<SecKeyRef>>>
      credential_result = credential_store_->CreateCredential(
          request_.rp.id, request_.user, TouchIdCredentialStore::kDiscoverable);
  if (!credential_result) {
    FIDO_LOG(ERROR) << "CreateCredential() failed";
    std::move(callback_).Run(
        MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
    return;
  }
  auto [credential, sec_key_ref] = std::move(*credential_result);

  // Create attestation object. There is no separate attestation key pair, so
  // we perform self-attestation.
  std::optional<AttestedCredentialData> attested_credential_data =
      MakeAttestedCredentialData(credential.credential_id,
                                 SecKeyRefToECPublicKey(sec_key_ref.get()));
  if (!attested_credential_data) {
    FIDO_LOG(ERROR) << "MakeAttestedCredentialData failed";
    std::move(callback_).Run(
        MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
    return;
  }
  AuthenticatorData authenticator_data = MakeAuthenticatorData(
      credential.metadata.sign_counter_type, request_.rp.id,
      std::move(*attested_credential_data), has_uv);
  std::optional<std::vector<uint8_t>> signature =
      GenerateSignature(authenticator_data, request_.client_data_hash,
                        credential.private_key.get());
  if (!signature) {
    FIDO_LOG(ERROR) << "MakeSignature failed";
    std::move(callback_).Run(
        MakeCredentialStatus::kAuthenticatorResponseInvalid, std::nullopt);
    return;
  }
  AuthenticatorMakeCredentialResponse response(
      FidoTransportProtocol::kInternal,
      AttestationObject(
          std::move(authenticator_data),
          std::make_unique<PackedAttestationStatement>(
              CoseAlgorithmIdentifier::kEs256, std::move(*signature),
              /*x509_certificates=*/std::vector<std::vector<uint8_t>>())));
  // New credentials are always discoverable.
  response.is_resident_key = true;
  response.transports.emplace();
  response.transports->insert(FidoTransportProtocol::kInternal);
  std::move(callback_).Run(MakeCredentialStatus::kSuccess, std::move(response));
}

}  // namespace device::fido::mac
