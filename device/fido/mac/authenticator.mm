// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/authenticator.h"

#include <algorithm>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/base/features.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/get_assertion_operation.h"
#include "device/fido/mac/make_credential_operation.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device::fido::mac {

// static
void TouchIdAuthenticator::IsAvailable(
    AuthenticatorConfig config,
    base::OnceCallback<void(bool is_available)> callback) {
  TouchIdContext::TouchIdAvailable(std::move(config), std::move(callback));
}

// static
std::unique_ptr<TouchIdAuthenticator> TouchIdAuthenticator::Create(
    AuthenticatorConfig config) {
  return base::WrapUnique(
      new TouchIdAuthenticator(std::move(config.keychain_access_group),
                               std::move(config.metadata_secret)));
}

TouchIdAuthenticator::~TouchIdAuthenticator() = default;

void TouchIdAuthenticator::InitializeAuthenticator(base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void TouchIdAuthenticator::GetPlatformCredentialInfoForRequest(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    GetPlatformCredentialInfoForRequestCallback callback) {
  std::optional<std::list<Credential>> credentials =
      request.allow_list.empty()
          ? credential_store_.FindResidentCredentials(request.rp_id)
          : credential_store_.FindCredentialsFromCredentialDescriptorList(
                request.rp_id, request.allow_list);
  if (!credentials) {
    FIDO_LOG(ERROR) << "Failed to fetch credentials from CredentialStore";
    std::move(callback).Run(/*credentials=*/{},
                            device::FidoRequestHandlerBase::
                                RecognizedCredential::kNoRecognizedCredential);
    return;
  }
  std::vector<DiscoverableCredentialMetadata> result;
  // With `kWebAuthnMacPlatformAuthenticatorOptionalUv`, always report the
  // list of credentials, because the UI will show a confirmation prompt for
  // one randomly chosen credential and run through the same pre-select flow
  // as for empty allow lists.
  for (const auto& credential : *credentials) {
    result.emplace_back(AuthenticatorType::kTouchID, request.rp_id,
                        credential.credential_id,
                        credential.metadata.ToPublicKeyCredentialUserEntity());
  }
  std::move(callback).Run(
      std::move(result),
      credentials->empty()
          ? device::FidoRequestHandlerBase::RecognizedCredential::
                kNoRecognizedCredential
          : device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential);
}

void TouchIdAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                          MakeCredentialOptions options,
                                          MakeCredentialCallback callback) {
  DCHECK(!operation_);
  operation_ = std::make_unique<MakeCredentialOperation>(
      std::move(request), &credential_store_, std::move(callback));
  operation_->Run();
}

void TouchIdAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  DCHECK(!operation_);
  operation_ = std::make_unique<GetAssertionOperation>(
      std::move(request), &credential_store_, std::move(callback));
  operation_->Run();
}

void TouchIdAuthenticator::Cancel() {
  // If there is an operation pending, delete it, which will clean up any
  // pending callbacks, e.g. if the operation is waiting for a response from
  // the Touch ID prompt. Note that we cannot cancel the actual prompt once it
  // has been shown.
  operation_.reset();
}

AuthenticatorType TouchIdAuthenticator::GetType() const {
  return AuthenticatorType::kTouchID;
}

std::string TouchIdAuthenticator::GetId() const {
  return "TouchIdAuthenticator";
}

std::optional<FidoTransportProtocol>
TouchIdAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

namespace {

AuthenticatorSupportedOptions TouchIdAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  return options;
}

}  // namespace

const AuthenticatorSupportedOptions& TouchIdAuthenticator::Options() const {
  static const base::NoDestructor<AuthenticatorSupportedOptions> options(
      TouchIdAuthenticatorOptions());
  return *options;
}

void TouchIdAuthenticator::GetTouch(base::OnceClosure callback) {
  // If at any point request processing wants to collect a "touch" from this
  // authenticator, pretend that happens immediately because UI interaction
  // already happened to trigger this authenticator.
  std::move(callback).Run();
}

base::WeakPtr<FidoAuthenticator> TouchIdAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

TouchIdAuthenticator::TouchIdAuthenticator(std::string keychain_access_group,
                                           std::string metadata_secret)
    : credential_store_(
          {std::move(keychain_access_group), std::move(metadata_secret)}),
      weak_factory_(this) {}

}  // namespace device::fido::mac
