// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/get_assertion_operation.h"

#include <set>
#include <string>

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {
namespace fido {
namespace mac {

using base::ScopedCFTypeRef;

GetAssertionOperation::GetAssertionOperation(CtapGetAssertionRequest request,
                                             std::string metadata_secret,
                                             std::string keychain_access_group,
                                             Callback callback)
    : OperationBase<CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>(
          std::move(request),
          std::move(metadata_secret),
          std::move(keychain_access_group),
          std::move(callback)) {}
GetAssertionOperation::~GetAssertionOperation() = default;

const std::string& GetAssertionOperation::RpId() const {
  return request().rp_id;
}

void GetAssertionOperation::Run() {
  if (!Init()) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  // Display the macOS Touch ID prompt.
  PromptTouchId(l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                           base::UTF8ToUTF16(RpId())));
}

void GetAssertionOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied, base::nullopt);
    return;
  }
  std::set<std::vector<uint8_t>> allowed_credential_ids =
      FilterInapplicableEntriesFromAllowList(request());
  if (allowed_credential_ids.empty() && !request().allow_list.empty()) {
    // The caller checking
    // TouchIdAuthenticator::HasCredentialForGetAssertionRequest() should have
    // caught this.
    NOTREACHED();
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }
  const bool empty_allow_list = request().allow_list.empty();

  std::list<Credential> credentials =
      empty_allow_list ? FindResidentCredentialsInKeychain(
                             keychain_access_group(), metadata_secret(), RpId(),
                             authentication_context())
                       : FindCredentialsInKeychain(
                             keychain_access_group(), metadata_secret(), RpId(),
                             allowed_credential_ids, authentication_context());

  if (credentials.empty()) {
    // TouchIdAuthenticator::HasCredentialForGetAssertionRequest() is invoked
    // first to ensure this doesn't occur.
    NOTREACHED();
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }

  base::Optional<AuthenticatorGetAssertionResponse> response =
      ResponseForCredential(credentials.front());
  if (!response) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }

  if (empty_allow_list) {
    response->SetNumCredentials(credentials.size());
    credentials.pop_front();
    matching_credentials_ = std::move(credentials);
  }

  std::move(callback())
      .Run(CtapDeviceResponseCode::kSuccess, std::move(*response));
}

void GetAssertionOperation::GetNextAssertion(Callback callback) {
  DCHECK(!matching_credentials_.empty());
  auto response =
      ResponseForCredential(std::move(matching_credentials_.front()));
  matching_credentials_.pop_front();
  if (!response) {
    NOTREACHED();
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(*response));
}

base::Optional<AuthenticatorGetAssertionResponse>
GetAssertionOperation::ResponseForCredential(const Credential& credential) {
  base::Optional<CredentialMetadata> metadata =
      UnsealCredentialId(metadata_secret(), RpId(), credential.credential_id);
  if (!metadata) {
    // The keychain query already filtered for the RP ID encoded under this
    // operation's metadata secret, so the credential id really should have
    // been decryptable.
    FIDO_LOG(ERROR) << "UnsealCredentialId failed";
    return base::nullopt;
  }

  AuthenticatorData authenticator_data =
      MakeAuthenticatorData(RpId(), /*attested_credential_data=*/base::nullopt);
  base::Optional<std::vector<uint8_t>> signature = GenerateSignature(
      authenticator_data, request().client_data_hash, credential.private_key);
  if (!signature) {
    FIDO_LOG(ERROR) << "GenerateSignature failed";
    return base::nullopt;
  }
  AuthenticatorGetAssertionResponse response(std::move(authenticator_data),
                                             std::move(*signature));
  response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, credential.credential_id));
  response.SetUserEntity(metadata->ToPublicKeyCredentialUserEntity());
  return response;
}

std::set<std::vector<uint8_t>> FilterInapplicableEntriesFromAllowList(
    const CtapGetAssertionRequest& request) {
  std::set<std::vector<uint8_t>> allowed_credential_ids;
  for (const auto& credential_descriptor : request.allow_list) {
    if (credential_descriptor.credential_type() == CredentialType::kPublicKey &&
        (credential_descriptor.transports().empty() ||
         base::Contains(credential_descriptor.transports(),
                        FidoTransportProtocol::kInternal))) {
      allowed_credential_ids.insert(credential_descriptor.id());
    }
  }
  return allowed_credential_ids;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
