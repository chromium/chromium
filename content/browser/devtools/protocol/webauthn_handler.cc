// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_u2f_device.h"

namespace content {
namespace protocol {

namespace {
static constexpr char kAuthenticatorNotFound[] =
    "Could not find a Virtual Authenticator matching the ID";
static constexpr char kCableNotSupportedOnU2f[] =
    "U2F only supports the \"usb\", \"ble\" and \"nfc\" transports";
static constexpr char kCouldNotCreateCredential[] =
    "An error occurred trying to create the credential";
static constexpr char kCredentialNotFound[] =
    "Could not find a credential matching the ID";
static constexpr char kDevToolsNotAttached[] =
    "The DevTools session is not attached to a frame";
static constexpr char kErrorCreatingAuthenticator[] =
    "An error occurred when trying to create the authenticator";
static constexpr char kHandleRequiredForResidentCredential[] =
    "The User Handle is required for Resident Credentials";
static constexpr char kInvalidProtocol[] = "The protocol is not valid";
static constexpr char kInvalidTransport[] = "The transport is not valid";
static constexpr char kInvalidUserHandle[] =
    "The User Handle must have a maximum size of ";
static constexpr char kResidentCredentialNotSupported[] =
    "The Authenticator does not support Resident Credentials.";
static constexpr char kRpIdRequired[] =
    "The Relying Party ID is a required parameter";
static constexpr char kVirtualEnvironmentNotEnabled[] =
    "The Virtual Authenticator Environment has not been enabled for this "
    "session";

device::ProtocolVersion ConvertToProtocolVersion(base::StringPiece protocol) {
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::Ctap2)
    return device::ProtocolVersion::kCtap2;
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::U2f)
    return device::ProtocolVersion::kU2f;
  return device::ProtocolVersion::kUnknown;
}

std::vector<uint8_t> CopyBinaryToVector(const Binary& binary) {
  return std::vector<uint8_t>(binary.data(), binary.data() + binary.size());
}

std::unique_ptr<WebAuthn::Credential> BuildCredentialFromRegistration(
    const std::pair<const std::vector<uint8_t>,
                    device::VirtualFidoDevice::RegistrationData>&
        registration) {
  auto credential =
      WebAuthn::Credential::Create()
          .SetCredentialId(Binary::fromVector(registration.first))
          .SetPrivateKey(Binary::fromVector(
              registration.second.private_key->GetPKCS8PrivateKey()))
          .SetSignCount(registration.second.counter)
          .SetIsResidentCredential(registration.second.is_resident)
          .Build();

  if (registration.second.rp)
    credential->SetRpId(registration.second.rp->id);
  if (registration.second.user) {
    credential->SetUserHandle(Binary::fromVector(registration.second.user->id));
  }

  return credential;
}

}  // namespace

WebAuthnHandler::WebAuthnHandler()
    : DevToolsDomainHandler(WebAuthn::Metainfo::domainName) {}

WebAuthnHandler::~WebAuthnHandler() = default;

void WebAuthnHandler::SetRenderer(int process_host_id,
                                  RenderFrameHostImpl* frame_host) {
  if (!frame_host) {
    Disable();
  }
  frame_host_ = frame_host;
}

void WebAuthnHandler::Wire(UberDispatcher* dispatcher) {
  WebAuthn::Dispatcher::wire(dispatcher, this);
}

Response WebAuthnHandler::Enable() {
  if (!frame_host_)
    return Response::ServerError(kDevToolsNotAttached);

  AuthenticatorEnvironmentImpl::GetInstance()->EnableVirtualAuthenticatorFor(
      frame_host_->frame_tree_node());
  return Response::Success();
}

Response WebAuthnHandler::Disable() {
  if (frame_host_) {
    AuthenticatorEnvironmentImpl::GetInstance()->DisableVirtualAuthenticatorFor(
        frame_host_->frame_tree_node());
  }
  return Response::Success();
}

Response WebAuthnHandler::AddVirtualAuthenticator(
    std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
    String* out_authenticator_id) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironmentImpl::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  auto transport =
      device::ConvertToFidoTransportProtocol(options->GetTransport());
  if (!transport)
    return Response::InvalidParams(kInvalidTransport);

  auto protocol = ConvertToProtocolVersion(options->GetProtocol());
  if (protocol == device::ProtocolVersion::kUnknown)
    return Response::InvalidParams(kInvalidProtocol);

  if (protocol == device::ProtocolVersion::kU2f &&
      !device::VirtualU2fDevice::IsTransportSupported(*transport)) {
    return Response::InvalidParams(kCableNotSupportedOnU2f);
  }

  VirtualAuthenticator* authenticator = nullptr;
  switch (protocol) {
    case device::ProtocolVersion::kU2f:
      authenticator = authenticator_manager->CreateU2FAuthenticator(*transport);
      break;
    case device::ProtocolVersion::kCtap2:
      authenticator = authenticator_manager->CreateCTAP2Authenticator(
          device::Ctap2Version::kCtap2_0, *transport,
          transport == device::FidoTransportProtocol::kInternal
              ? device::AuthenticatorAttachment::kPlatform
              : device::AuthenticatorAttachment::kCrossPlatform,
          options->GetHasResidentKey(/*default=*/false),
          options->GetHasUserVerification(/*default=*/false));
      break;
    case device::ProtocolVersion::kUnknown:
      NOTREACHED();
      break;
  }
  if (!authenticator)
    return Response::ServerError(kErrorCreatingAuthenticator);

  authenticator->SetUserPresence(
      options->GetAutomaticPresenceSimulation(true /* default */));
  authenticator->set_user_verified(
      options->GetIsUserVerified(/*default=*/false));

  *out_authenticator_id = authenticator->unique_id();
  return Response::Success();
}

Response WebAuthnHandler::RemoveVirtualAuthenticator(
    const String& authenticator_id) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironmentImpl::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  if (!authenticator_manager->RemoveAuthenticator(authenticator_id))
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::Success();
}

Response WebAuthnHandler::AddCredential(
    const String& authenticator_id,
    std::unique_ptr<WebAuthn::Credential> credential) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  Binary user_handle = credential->GetUserHandle(Binary());
  if (credential->HasUserHandle() &&
      user_handle.size() > device::kUserHandleMaxLength) {
    return Response::InvalidParams(
        kInvalidUserHandle +
        base::NumberToString(device::kUserHandleMaxLength));
  }

  if (!credential->HasRpId())
    return Response::InvalidParams(kRpIdRequired);

  bool credential_created;
  if (credential->GetIsResidentCredential()) {
    if (!authenticator->has_resident_key())
      return Response::InvalidParams(kResidentCredentialNotSupported);

    if (!credential->HasUserHandle())
      return Response::InvalidParams(kHandleRequiredForResidentCredential);

    credential_created = authenticator->AddResidentRegistration(
        CopyBinaryToVector(credential->GetCredentialId()),
        credential->GetRpId(""),
        CopyBinaryToVector(credential->GetPrivateKey()),
        credential->GetSignCount(), CopyBinaryToVector(user_handle));
  } else {
    credential_created = authenticator->AddRegistration(
        CopyBinaryToVector(credential->GetCredentialId()),
        credential->GetRpId(""),
        CopyBinaryToVector(credential->GetPrivateKey()),
        credential->GetSignCount());
  }

  if (!credential_created)
    return Response::ServerError(kCouldNotCreateCredential);

  return Response::Success();
}

Response WebAuthnHandler::GetCredential(
    const String& authenticator_id,
    const Binary& credential_id,
    std::unique_ptr<WebAuthn::Credential>* out_credential) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  auto registration =
      authenticator->registrations().find(CopyBinaryToVector(credential_id));
  if (registration == authenticator->registrations().end())
    return Response::InvalidParams(kCredentialNotFound);

  *out_credential = BuildCredentialFromRegistration(*registration);
  return Response::Success();
}

Response WebAuthnHandler::GetCredentials(
    const String& authenticator_id,
    std::unique_ptr<Array<WebAuthn::Credential>>* out_credentials) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  *out_credentials = std::make_unique<Array<WebAuthn::Credential>>();
  for (const auto& registration : authenticator->registrations()) {
    (*out_credentials)
        ->emplace_back(BuildCredentialFromRegistration(registration));
  }
  return Response::Success();
}

Response WebAuthnHandler::RemoveCredential(const String& authenticator_id,
                                           const Binary& credential_id) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  if (!authenticator->RemoveRegistration(CopyBinaryToVector(credential_id)))
    return Response::InvalidParams(kCredentialNotFound);

  return Response::Success();
}

Response WebAuthnHandler::ClearCredentials(const String& authenticator_id) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  authenticator->ClearRegistrations();
  return Response::Success();
}

Response WebAuthnHandler::SetUserVerified(const String& authenticator_id,
                                          bool is_user_verified) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  authenticator->set_user_verified(is_user_verified);
  return Response::Success();
}

Response WebAuthnHandler::SetAutomaticPresenceSimulation(
    const String& authenticator_id,
    bool enabled) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return response;

  authenticator->SetUserPresence(enabled);
  return Response::Success();
}

Response WebAuthnHandler::FindAuthenticator(
    const String& id,
    VirtualAuthenticator** out_authenticator) {
  *out_authenticator = nullptr;
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironmentImpl::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  *out_authenticator = authenticator_manager->GetAuthenticator(id);
  if (!*out_authenticator)
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::Success();
}

}  // namespace protocol
}  // namespace content
