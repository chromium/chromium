// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/protocol/web_authn.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_u2f_device.h"

namespace content::protocol {

namespace {
static constexpr char kAuthenticatorNotFound[] =
    "Could not find a Virtual Authenticator matching the ID";
static constexpr char kCableNotSupportedOnU2f[] =
    "U2F only supports the \"usb\", \"ble\" and \"nfc\" transports";
static constexpr char kCouldNotCreateCredential[] =
    "An error occurred trying to create the credential";
static constexpr char kCouldNotStoreLargeBlob[] =
    "An error occurred trying to store the large blob";
static constexpr char kCredentialNotFound[] =
    "Could not find a credential matching the ID";
static constexpr char kDevToolsNotAttached[] =
    "The DevTools session is not attached to a frame";
static constexpr char kErrorCreatingAuthenticator[] =
    "An error occurred when trying to create the authenticator";
static constexpr char kHandleRequiredForResidentCredential[] =
    "The User Handle is required for Resident Credentials";
static constexpr char kInvalidCtapVersion[] =
    "Invalid CTAP version. Valid values are \"ctap2_0\" and \"ctap2_1\"";
static constexpr char kInvalidProtocol[] = "The protocol is not valid";
static constexpr char kInvalidTransport[] = "The transport is not valid";
static constexpr char kInvalidUserHandle[] =
    "The User Handle must have a maximum size of ";
static constexpr char kLargeBlobRequiresResidentKey[] =
    "Large blob requires resident key support";
static constexpr char kRequiresCtap2_1[] =
    "Specified options require a CTAP 2.1 authenticator";
static constexpr char kResidentCredentialNotSupported[] =
    "The Authenticator does not support Resident Credentials.";
static constexpr char kRpIdRequired[] =
    "The Relying Party ID is a required parameter";
static constexpr char kVirtualEnvironmentNotEnabled[] =
    "The Virtual Authenticator Environment has not been enabled for this "
    "session";

class GetCredentialCallbackAggregator
    : public base::RefCounted<GetCredentialCallbackAggregator> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit GetCredentialCallbackAggregator(
      std::unique_ptr<WebAuthn::Backend::GetCredentialsCallback> callback)
      : callback_(std::move(callback)) {}
  GetCredentialCallbackAggregator(const GetCredentialCallbackAggregator&) =
      delete;
  GetCredentialCallbackAggregator operator=(
      const GetCredentialCallbackAggregator&) = delete;

  void OnLargeBlob(std::unique_ptr<WebAuthn::Credential> credential,
                   const std::optional<std::vector<uint8_t>>& blob) {
    if (blob) {
      credential->SetLargeBlob(Binary::fromVector(*blob));
    }
    credentials_->emplace_back(std::move(credential));
  }

 private:
  friend class base::RefCounted<GetCredentialCallbackAggregator>;
  ~GetCredentialCallbackAggregator() {
    callback_->sendSuccess(std::move(credentials_));
  }

  std::unique_ptr<WebAuthn::Backend::GetCredentialsCallback> callback_;
  std::unique_ptr<Array<WebAuthn::Credential>> credentials_ =
      std::make_unique<Array<WebAuthn::Credential>>();
};

device::ProtocolVersion ConvertToProtocolVersion(std::string_view protocol) {
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::Ctap2)
    return device::ProtocolVersion::kCtap2;
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::U2f)
    return device::ProtocolVersion::kU2f;
  return device::ProtocolVersion::kUnknown;
}

std::optional<device::Ctap2Version> ConvertToCtap2Version(
    std::string_view version) {
  if (version == WebAuthn::Ctap2VersionEnum::Ctap2_0)
    return device::Ctap2Version::kCtap2_0;
  if (version == WebAuthn::Ctap2VersionEnum::Ctap2_1)
    return device::Ctap2Version::kCtap2_1;
  return std::nullopt;
}

std::vector<uint8_t> CopyBinaryToVector(const Binary& binary) {
  return std::vector<uint8_t>(binary.data(), binary.data() + binary.size());
}

std::unique_ptr<WebAuthn::Credential> BuildCredentialFromRegistration(
    base::span<const uint8_t> id,
    const device::VirtualFidoDevice::RegistrationData* registration) {
  auto credential = WebAuthn::Credential::Create()
                        .SetCredentialId(Binary::fromSpan(id))
                        .SetPrivateKey(Binary::fromVector(
                            registration->private_key->GetPKCS8PrivateKey()))
                        .SetSignCount(registration->counter)
                        .SetIsResidentCredential(registration->is_resident)
                        .SetBackupEligibility(registration->backup_eligible)
                        .SetBackupState(registration->backup_state)
                        .Build();

  if (registration->rp)
    credential->SetRpId(registration->rp->id);
  if (registration->user) {
    credential->SetUserHandle(Binary::fromVector(registration->user->id));
    if (registration->user->name) {
      credential->SetUserName(*registration->user->name);
    }
    if (registration->user->display_name) {
      credential->SetUserDisplayName(*registration->user->display_name);
    }
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
  frontend_ = std::make_unique<WebAuthn::Frontend>(dispatcher->channel());
  WebAuthn::Dispatcher::wire(dispatcher, this);
}

Response WebAuthnHandler::Enable(Maybe<bool> enable_ui) {
  if (!frame_host_)
    return Response::ServerError(kDevToolsNotAttached);

  AuthenticatorEnvironment::GetInstance()->EnableVirtualAuthenticatorFor(
      frame_host_->frame_tree_node(),
      enable_ui.value_or(/*default_value=*/false));
  return Response::Success();
}

Response WebAuthnHandler::Disable() {
  if (frame_host_) {
    AuthenticatorEnvironment::GetInstance()->DisableVirtualAuthenticatorFor(
        frame_host_->frame_tree_node());
  }
  return Response::Success();
}

Response WebAuthnHandler::AddVirtualAuthenticator(
    std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
    String* out_authenticator_id) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
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

  auto ctap2_version = ConvertToCtap2Version(
      options->GetCtap2Version(WebAuthn::Ctap2VersionEnum::Ctap2_0));
  if (!ctap2_version)
    return Response::InvalidParams(kInvalidCtapVersion);

  bool has_large_blob = options->GetHasLargeBlob(/*defaultValue=*/false);
  bool has_cred_blob = options->GetHasCredBlob(/*defaultValue=*/false);
  bool has_min_pin_length = options->GetHasMinPinLength(/*defaultValue=*/false);
  bool has_prf = options->GetHasPrf(/*defaultValue=*/false);
  bool has_resident_key = options->GetHasResidentKey(/*defaultValue=*/false);

  if (has_large_blob && !has_resident_key)
    return Response::InvalidParams(kLargeBlobRequiresResidentKey);

  if ((protocol != device::ProtocolVersion::kCtap2 ||
       ctap2_version < device::Ctap2Version::kCtap2_1) &&
      (has_large_blob || has_cred_blob || has_min_pin_length)) {
    return Response::InvalidParams(kRequiresCtap2_1);
  }

  auto virt_auth_options =
      blink::test::mojom::VirtualAuthenticatorOptions::New();
  virt_auth_options->protocol = protocol;
  virt_auth_options->transport = *transport;

  switch (protocol) {
    case device::ProtocolVersion::kU2f:
      virt_auth_options->attachment =
          device::AuthenticatorAttachment::kCrossPlatform;
      break;
    case device::ProtocolVersion::kCtap2:
      virt_auth_options->ctap2_version = *ctap2_version;
      virt_auth_options->attachment =
          transport == device::FidoTransportProtocol::kInternal
              ? device::AuthenticatorAttachment::kPlatform
              : device::AuthenticatorAttachment::kCrossPlatform;
      virt_auth_options->has_resident_key = has_resident_key;
      virt_auth_options->has_user_verification =
          options->GetHasUserVerification(/*defaultValue=*/false);
      virt_auth_options->has_large_blob = has_large_blob;
      virt_auth_options->has_cred_blob = has_cred_blob;
      virt_auth_options->has_min_pin_length = has_min_pin_length;
      virt_auth_options->has_prf = has_prf;
      virt_auth_options->default_backup_eligibility =
          options->GetDefaultBackupEligibility(/*defaultValue=*/false);
      virt_auth_options->default_backup_state =
          options->GetDefaultBackupState(/*defaultValue=*/false);
      break;
    case device::ProtocolVersion::kUnknown:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  VirtualAuthenticator* const authenticator =
      authenticator_manager->AddAuthenticatorAndReturnNonOwningPointer(
          *virt_auth_options);
  if (!authenticator)
    return Response::ServerError(kErrorCreatingAuthenticator);

  authenticator->SetUserPresence(
      options->GetAutomaticPresenceSimulation(/*defaultValue=*/true));
  authenticator->set_user_verified(
      options->GetIsUserVerified(/*defaultValue=*/false));
  observations_.AddObservation(authenticator);

  *out_authenticator_id = authenticator->unique_id();
  return Response::Success();
}

Response WebAuthnHandler::RemoveVirtualAuthenticator(
    const String& authenticator_id) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  if (!authenticator_manager->RemoveAuthenticator(authenticator_id))
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::Success();
}

Response WebAuthnHandler::SetResponseOverrideBits(
    const String& authenticator_id,
    Maybe<bool> is_bogus_signature,
    Maybe<bool> is_bad_uv,
    Maybe<bool> is_bad_up) {
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess())
    return Response::InvalidParams(kAuthenticatorNotFound);

  authenticator->set_bogus_signature(
      is_bogus_signature.value_or(/*default_value=*/false));
  authenticator->set_bad_uv_bit(is_bad_uv.value_or(/*default_value=*/false));
  authenticator->set_bad_up_bit(is_bad_up.value_or(/*default_value=*/false));
  return Response::Success();
}

void WebAuthnHandler::AddCredential(
    const String& authenticator_id,
    std::unique_ptr<WebAuthn::Credential> credential,
    std::unique_ptr<AddCredentialCallback> callback) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  Binary user_handle = credential->GetUserHandle(Binary());
  if (credential->HasUserHandle() &&
      user_handle.size() > device::kUserHandleMaxLength) {
    callback->sendFailure(Response::InvalidParams(
        kInvalidUserHandle +
        base::NumberToString(device::kUserHandleMaxLength)));
    return;
  }

  if (!credential->HasRpId()) {
    callback->sendFailure(Response::InvalidParams(kRpIdRequired));
    return;
  }
  if (credential->HasLargeBlob() && !credential->GetIsResidentCredential()) {
    callback->sendFailure(
        Response::InvalidParams(kLargeBlobRequiresResidentKey));
    return;
  }

  bool credential_created;
  std::vector<uint8_t> credential_id =
      CopyBinaryToVector(credential->GetCredentialId());
  if (credential->GetIsResidentCredential()) {
    if (!authenticator->has_resident_key()) {
      callback->sendFailure(
          Response::InvalidParams(kResidentCredentialNotSupported));
      return;
    }

    if (!credential->HasUserHandle()) {
      callback->sendFailure(
          Response::InvalidParams(kHandleRequiredForResidentCredential));
      return;
    }

    credential_created = authenticator->AddResidentRegistration(
        credential_id, credential->GetRpId(""), credential->GetPrivateKey(),
        credential->GetSignCount(), CopyBinaryToVector(user_handle),
        credential->GetUserName(""), credential->GetUserDisplayName(""));
  } else {
    credential_created = authenticator->AddRegistration(
        credential_id, credential->GetRpId(""), credential->GetPrivateKey(),
        credential->GetSignCount());
  }

  if (!credential_created) {
    callback->sendFailure(Response::ServerError(kCouldNotCreateCredential));
    return;
  }

  if (credential->HasLargeBlob()) {
    authenticator->SetLargeBlob(
        credential_id, CopyBinaryToVector(credential->GetLargeBlob({})),
        base::BindOnce(
            [](std::unique_ptr<AddCredentialCallback> callback, bool success) {
              if (!success) {
                callback->sendFailure(
                    Response::ServerError(kCouldNotStoreLargeBlob));
                return;
              }
              callback->sendSuccess();
            },
            std::move(callback)));
    return;
  }

  // VirtualFidoDevice takes care of setting BE & BS flags to the default
  // authenticator values whenever a new credential is created. Only override
  // the values if the client specified them.
  if (credential->HasBackupEligibility()) {
    authenticator->SetBackupEligibility(
        credential_id, credential->GetBackupEligibility(/*unused*/ false));
  }
  if (credential->HasBackupState()) {
    authenticator->SetBackupState(credential_id,
                                  credential->GetBackupState(/*unused*/ false));
  }

  callback->sendSuccess();
}

void WebAuthnHandler::GetCredential(
    const String& authenticator_id,
    const Binary& credential_id,
    std::unique_ptr<GetCredentialCallback> callback) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  auto registration =
      authenticator->registrations().find(CopyBinaryToVector(credential_id));
  if (registration == authenticator->registrations().end()) {
    callback->sendFailure(Response::InvalidParams(kCredentialNotFound));
    return;
  }

  authenticator->GetLargeBlob(
      registration->first,
      base::BindOnce(
          [](std::unique_ptr<WebAuthn::Credential> registration,
             std::unique_ptr<GetCredentialCallback> callback,
             const std::optional<std::vector<uint8_t>>& blob) {
            if (blob) {
              registration->SetLargeBlob(Binary::fromVector(*blob));
            }
            callback->sendSuccess(std::move(registration));
          },
          BuildCredentialFromRegistration(base::make_span(registration->first),
                                          &registration->second),
          std::move(callback)));
}

void WebAuthnHandler::GetCredentials(
    const String& authenticator_id,
    std::unique_ptr<GetCredentialsCallback> callback) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  auto aggregator = base::MakeRefCounted<GetCredentialCallbackAggregator>(
      std::move(callback));
  for (const auto& registration : authenticator->registrations()) {
    authenticator->GetLargeBlob(
        registration.first,
        base::BindOnce(
            &GetCredentialCallbackAggregator::OnLargeBlob, aggregator,
            BuildCredentialFromRegistration(base::make_span(registration.first),
                                            &registration.second)));
  }
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

Response WebAuthnHandler::SetCredentialProperties(
    const String& authenticator_id,
    const Binary& in_credential_id,
    Maybe<bool> backup_eligibility,
    Maybe<bool> backup_state) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.IsSuccess()) {
    return response;
  }

  std::vector<uint8_t> credential_id = CopyBinaryToVector(in_credential_id);
  auto registration = authenticator->registrations().find(credential_id);
  if (registration == authenticator->registrations().end()) {
    return Response::InvalidParams(kCredentialNotFound);
  }

  if (backup_eligibility.has_value()) {
    authenticator->SetBackupEligibility(credential_id,
                                        backup_eligibility.value());
  }
  if (backup_state.has_value()) {
    authenticator->SetBackupState(credential_id, backup_state.value());
  }
  return Response::Success();
}

Response WebAuthnHandler::FindAuthenticator(
    const String& id,
    VirtualAuthenticator** out_authenticator) {
  *out_authenticator = nullptr;
  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_host_->frame_tree_node());
  if (!authenticator_manager)
    return Response::ServerError(kVirtualEnvironmentNotEnabled);

  *out_authenticator = authenticator_manager->GetAuthenticator(id);
  if (!*out_authenticator)
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::Success();
}

void WebAuthnHandler::OnCredentialCreated(
    VirtualAuthenticator* authenticator,
    const device::VirtualFidoDevice::Credential& credential) {
  frontend_->CredentialAdded(
      authenticator->unique_id(),
      BuildCredentialFromRegistration(credential.first, credential.second));
}

void WebAuthnHandler::OnCredentialDeleted(
    VirtualAuthenticator* authenticator,
    base::span<const uint8_t> credential_id) {
  frontend_->CredentialDeleted(authenticator->unique_id(),
                               Binary::fromSpan(credential_id));
}

void WebAuthnHandler::OnCredentialUpdated(
    VirtualAuthenticator* authenticator,
    const device::VirtualFidoDevice::Credential& credential) {
  frontend_->CredentialUpdated(
      authenticator->unique_id(),
      BuildCredentialFromRegistration(credential.first, credential.second));
}

void WebAuthnHandler::OnAssertion(
    VirtualAuthenticator* authenticator,
    const device::VirtualFidoDevice::Credential& credential) {
  frontend_->CredentialAsserted(
      authenticator->unique_id(),
      BuildCredentialFromRegistration(credential.first, credential.second));
}

void WebAuthnHandler::OnAuthenticatorWillBeDestroyed(
    VirtualAuthenticator* authenticator) {
  observations_.RemoveObservation(authenticator);
}

}  // namespace content::protocol
