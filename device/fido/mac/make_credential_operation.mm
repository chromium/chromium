// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/make_credential_operation.h"

#include <string>

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {
namespace fido {
namespace mac {

MakeCredentialOperation::MakeCredentialOperation(
    CtapMakeCredentialRequest request,
    TouchIdCredentialStore* credential_store,
    Callback callback)
    : request_(std::move(request)),
      credential_store_(credential_store),
      callback_(std::move(callback)) {}

MakeCredentialOperation::~MakeCredentialOperation() = default;

void MakeCredentialOperation::Run() {
  // Verify pubKeyCredParams contains ES-256, which is the only algorithm we
  // support.
  auto is_es256 =
      [](const PublicKeyCredentialParams::CredentialInfo& cred_info) {
        return cred_info.algorithm ==
               static_cast<int>(CoseAlgorithmIdentifier::kEs256);
      };
  const auto& key_params =
      request_.public_key_credential_params.public_key_credential_params();
  if (!std::any_of(key_params.begin(), key_params.end(), is_es256)) {
    DVLOG(1) << "No supported algorithm found.";
    std::move(callback_).Run(
        CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm, base::nullopt);
    return;
  }

  // Display the macOS Touch ID prompt.
  touch_id_context_->PromptTouchId(
      l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                 base::UTF8ToUTF16(request_.rp.id)),
      base::BindOnce(&MakeCredentialOperation::PromptTouchIdDone,
                     base::Unretained(this)));
}

void MakeCredentialOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             base::nullopt);
    return;
  }

  // Setting an authentication context authorizes credentials returned from the
  // credential store for signing without triggering yet another Touch ID
  // prompt.
  credential_store_->set_authentication_context(
      touch_id_context_->authentication_context());

  if (!request_.exclude_list.empty()) {
    base::Optional<std::list<Credential>> credentials =
        credential_store_->FindCredentialsFromCredentialDescriptorList(
            request_.rp.id, request_.exclude_list);
    if (!credentials) {
      FIDO_LOG(ERROR) << "Failed to check for excluded credentials";
      std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                               base::nullopt);
      return;
    }
    if (!credentials->empty()) {
      std::move(callback_).Run(
          CtapDeviceResponseCode::kCtap2ErrCredentialExcluded, base::nullopt);
      return;
    }
  }

  // Delete the key pair for this RP + user handle if one already exists.
  //
  // TODO(crbug/1025065): Decide whether we should evict non-resident
  // credentials at all.
  if (!credential_store_->DeleteCredentialsForUserId(request_.rp.id,
                                                     request_.user.id)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  // Generate the new key pair.
  base::Optional<std::pair<Credential, base::ScopedCFTypeRef<SecKeyRef>>>
      credential = credential_store_->CreateCredential(
          request_.rp.id, request_.user, request_.resident_key_required,
          touch_id_context_->access_control());
  if (!credential) {
    FIDO_LOG(ERROR) << "CreateCredential() failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  // Create attestation object. There is no separate attestation key pair, so
  // we perform self-attestation.
  base::Optional<AttestedCredentialData> attested_credential_data =
      MakeAttestedCredentialData(credential->first.credential_id,
                                 SecKeyRefToECPublicKey(credential->second));
  if (!attested_credential_data) {
    FIDO_LOG(ERROR) << "MakeAttestedCredentialData failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  AuthenticatorData authenticator_data = MakeAuthenticatorData(
      request_.rp.id, std::move(*attested_credential_data));
  base::Optional<std::vector<uint8_t>> signature =
      GenerateSignature(authenticator_data, request_.client_data_hash,
                        credential->first.private_key);
  if (!signature) {
    FIDO_LOG(ERROR) << "MakeSignature failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  AuthenticatorMakeCredentialResponse response(
      FidoTransportProtocol::kInternal,
      AttestationObject(
          std::move(authenticator_data),
          std::make_unique<PackedAttestationStatement>(
              CoseAlgorithmIdentifier::kEs256, std::move(*signature),
              /*x509_certificates=*/std::vector<std::vector<uint8_t>>())));
  response.is_resident_key = request_.resident_key_required;
  std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                           std::move(response));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
