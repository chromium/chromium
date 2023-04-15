// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/get_assertion_operation.h"

#include <set>
#include <string>

#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/features.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device::fido::mac {

using base::ScopedCFTypeRef;

GetAssertionOperation::GetAssertionOperation(
    CtapGetAssertionRequest request,
    TouchIdCredentialStore* credential_store,
    Callback callback)
    : request_(std::move(request)),
      credential_store_(credential_store),
      callback_(std::move(callback)) {}

GetAssertionOperation::~GetAssertionOperation() = default;

void GetAssertionOperation::Run() {
  const bool empty_allow_list = request_.allow_list.empty();
  absl::optional<std::list<Credential>> credentials =
      empty_allow_list
          ? credential_store_->FindResidentCredentials(request_.rp_id)
          : credential_store_->FindCredentialsFromCredentialDescriptorList(
                request_.rp_id, request_.allow_list);

  if (!credentials) {
    FIDO_LOG(ERROR) << "FindCredentialsFromCredentialDescriptorList() failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther, {});
    return;
  }

  if (credentials->empty()) {
    // This can happen if e.g. a credential is deleted after it is shown to the
    // user on the account picker.
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                             {});
    return;
  }

  bool require_uv =
      !base::FeatureList::IsEnabled(
          kWebAuthnMacPlatformAuthenticatorOptionalUv) ||
      DeviceHasBiometricsAvailable() ||
      request_.user_verification == UserVerificationRequirement::kRequired ||
      std::any_of(credentials->begin(), credentials->end(),
                  [](const Credential& credential) {
                    return credential.RequiresUvForSignature();
                  });
  if (require_uv) {
    touch_id_context_->PromptTouchId(
        l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                   base::UTF8ToUTF16(request_.rp_id)),
        base::BindOnce(
            &GetAssertionOperation::PromptTouchIdDone,
            // Safe to use Unretained because `touch_id_context_` is owned by
            // `this` and the callback won't run after its destruction.
            base::Unretained(this)));
    return;
  }

  GenerateResponses(std::move(*credentials), /*has_uv=*/false);
}

void GetAssertionOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             {});
    return;
  }

  // Re-fetch credentials with the now evaluated LAContext, so that making
  // signatures does not trigger yet another Touch ID prompt.
  credential_store_->SetAuthenticationContext(
      touch_id_context_->authentication_context());

  absl::optional<std::list<Credential>> credentials =
      request_.allow_list.empty()
          ? credential_store_->FindResidentCredentials(request_.rp_id)
          : credential_store_->FindCredentialsFromCredentialDescriptorList(
                request_.rp_id, request_.allow_list);

  if (!credentials || credentials->empty()) {
    FIDO_LOG(ERROR) << "Failed to fetch credentials";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             {});
    return;
  }

  GenerateResponses(std::move(*credentials), /*has_uv=*/true);
}

void GetAssertionOperation::GenerateResponses(std::list<Credential> credentials,
                                              bool has_uv) {
  DCHECK(has_uv || std::none_of(credentials.begin(), credentials.end(),
                                [](const Credential& credential) {
                                  return credential.RequiresUvForSignature();
                                }));

  std::vector<AuthenticatorGetAssertionResponse> responses;
  for (const Credential& credential : credentials) {
    absl::optional<AuthenticatorGetAssertionResponse> response =
        ResponseForCredential(credential, has_uv);
    if (!response) {
      FIDO_LOG(ERROR) << "Could not generate response for credential, skipping";
      continue;
    }
    responses.emplace_back(std::move(*response));
  }

  if (responses.empty()) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther, {});
    return;
  }

  std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                           std::move(responses));
}

absl::optional<AuthenticatorGetAssertionResponse>
GetAssertionOperation::ResponseForCredential(const Credential& credential,
                                             bool has_uv) {
  AuthenticatorData authenticator_data = MakeAuthenticatorData(
      credential.metadata.sign_counter_type, request_.rp_id,
      /*attested_credential_data=*/absl::nullopt, has_uv);
  absl::optional<std::vector<uint8_t>> signature = GenerateSignature(
      authenticator_data, request_.client_data_hash, credential.private_key);
  if (!signature) {
    FIDO_LOG(ERROR) << "GenerateSignature failed";
    return absl::nullopt;
  }
  AuthenticatorGetAssertionResponse response(std::move(authenticator_data),
                                             std::move(*signature));
  response.transport_used = FidoTransportProtocol::kInternal;
  response.credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, credential.credential_id);
  if (has_uv) {
    response.user_entity =
        credential.metadata.ToPublicKeyCredentialUserEntity();
  }
  return response;
}

}  // namespace device::fido::mac
