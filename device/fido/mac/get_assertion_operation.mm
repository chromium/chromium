// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/get_assertion_operation.h"

#include <set>
#include <string>
#include "device/fido/fido_transport_protocol.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {
namespace fido {
namespace mac {

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
  // Display the macOS Touch ID prompt.
  touch_id_context_->PromptTouchId(
      l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                 base::UTF8ToUTF16(request_.rp_id)),
      base::BindOnce(&GetAssertionOperation::PromptTouchIdDone,
                     base::Unretained(this)));
}

void GetAssertionOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             absl::nullopt);
    return;
  }

  // Setting an authentication context authorizes credentials returned from the
  // credential store for signing without triggering yet another Touch ID
  // prompt.
  credential_store_->set_authentication_context(
      touch_id_context_->authentication_context());

  const bool empty_allow_list = request_.allow_list.empty();
  absl::optional<std::list<Credential>> credentials =
      empty_allow_list
          ? credential_store_->FindResidentCredentials(request_.rp_id)
          : credential_store_->FindCredentialsFromCredentialDescriptorList(
                request_.rp_id, request_.allow_list);

  if (!credentials) {
    FIDO_LOG(ERROR) << "FindCredentialsFromCredentialDescriptorList() failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             absl::nullopt);
    return;
  }

  if (credentials->empty()) {
    // TouchIdAuthenticator::HasCredentialForGetAssertionRequest() is
    // invoked first to ensure this doesn't occur.
    NOTREACHED();
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                             absl::nullopt);
    return;
  }

  absl::optional<AuthenticatorGetAssertionResponse> response =
      ResponseForCredential(credentials->front());
  if (!response) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                             absl::nullopt);
    return;
  }

  if (empty_allow_list) {
    response->num_credentials = credentials->size();
    credentials->pop_front();
    matching_credentials_ = std::move(*credentials);
  }

  std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                           std::move(*response));
}

void GetAssertionOperation::GetNextAssertion(Callback callback) {
  DCHECK(!matching_credentials_.empty());
  auto response =
      ResponseForCredential(std::move(matching_credentials_.front()));
  matching_credentials_.pop_front();
  if (!response) {
    NOTREACHED();
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            absl::nullopt);
    return;
  }
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(*response));
}

absl::optional<AuthenticatorGetAssertionResponse>
GetAssertionOperation::ResponseForCredential(const Credential& credential) {
  AuthenticatorData authenticator_data = MakeAuthenticatorData(
      credential.metadata.sign_counter_type, request_.rp_id,
      /*attested_credential_data=*/absl::nullopt);
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
  response.user_entity = credential.metadata.ToPublicKeyCredentialUserEntity();
  return response;
}

}  // namespace mac
}  // namespace fido
}  // namespace device
