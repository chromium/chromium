// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "device/fido/cros/authenticator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/opaque_attestation_statement.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace device {

ChromeOSAuthenticator::ChromeOSAuthenticator(
    base::RepeatingCallback<uint32_t()> generate_request_id_callback)
    : generate_request_id_callback_(std::move(generate_request_id_callback)),
      weak_factory_(this) {}

ChromeOSAuthenticator::~ChromeOSAuthenticator() {}

std::string ChromeOSAuthenticator::GetId() const {
  return "ChromeOSAuthenticator";
}

base::string16 ChromeOSAuthenticator::GetDisplayName() const {
  return base::string16(base::ASCIIToUTF16("ChromeOS Authenticator"));
}

namespace {

// DBus timeout for method calls that doesn't involve user interaction.
constexpr int kShortTimeoutMs = 3000;

AuthenticatorSupportedOptions ChromeOSAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device = true;
  // TODO(yichengli): change supports_resident_key to true once it's supported.
  options.supports_resident_key = false;
  // Even if the user has no fingerprints enrolled, we will have password
  // as fallback.
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  return options;
}

}  // namespace

const base::Optional<AuthenticatorSupportedOptions>&
ChromeOSAuthenticator::Options() const {
  static const base::Optional<AuthenticatorSupportedOptions> options =
      ChromeOSAuthenticatorOptions();
  return options;
}

base::Optional<FidoTransportProtocol>
ChromeOSAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

void ChromeOSAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void ChromeOSAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                           MakeCredentialCallback callback) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  if (!u2f_proxy) {
    FIDO_LOG(ERROR) << "Couldn't get u2f proxy";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  u2f::MakeCredentialRequest req;
  // Requests with UserPresence get upgraded to UserVerification unless
  // verification is explicitly discouraged.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged)
          ? u2f::VERIFICATION_USER_PRESENCE
          : u2f::VERIFICATION_USER_VERIFICATION);
  req.set_rp_id(request.rp.id);
  req.set_user_id(std::string(request.user.id.begin(), request.user.id.end()));
  if (request.user.display_name.has_value())
    req.set_user_display_name(request.user.display_name.value());
  req.set_resident_credential(request.resident_key_required);
  DCHECK(generate_request_id_callback_);
  DCHECK_EQ(current_request_id_, 0u);
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.exclude_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_excluded_credential_id(std::string(id.begin(), id.end()));
  }

  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FMakeCredential);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(req);

  // Use infinite timeout because Cancel() will be called when the request
  // times out.
  u2f_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
      base::BindOnce(&ChromeOSAuthenticator::OnMakeCredentialResp,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void ChromeOSAuthenticator::OnMakeCredentialResp(
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback,
    dbus::Response* dbus_response,
    dbus::ErrorResponse* error) {
  if (error) {
    FIDO_LOG(ERROR) << "MakeCredential dbus call failed with error: "
                    << error->ToString();
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  dbus::MessageReader reader(dbus_response);
  u2f::MakeCredentialResponse resp;
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    FIDO_LOG(ERROR) << "Failed to parse reply for call to MakeCredential";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  FIDO_LOG(DEBUG) << "Make credential status: " << resp.status();
  if (resp.status() !=
      u2f::MakeCredentialResponse_MakeCredentialStatus_SUCCESS) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                            base::nullopt);
    return;
  }

  base::Optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(resp.authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  base::Optional<cbor::Value> statement_map = cbor::Reader::Read(
      base::as_bytes(base::make_span(resp.attestation_statement())));
  if (!statement_map ||
      statement_map.value().type() != cbor::Value::Type::MAP) {
    FIDO_LOG(ERROR) << "Attestation statement is not a CBOR map.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
  }
  auto statement = std::make_unique<OpaqueAttestationStatement>(
      resp.attestation_format(), std::move(*statement_map));

  AuthenticatorMakeCredentialResponse response(
      FidoTransportProtocol::kInternal,
      AttestationObject(std::move(*authenticator_data), std::move(statement)));

  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(response));
}

void ChromeOSAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                         CtapGetAssertionOptions options,
                                         GetAssertionCallback callback) {
  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(dbus_options);
  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  if (!u2f_proxy) {
    FIDO_LOG(ERROR) << "Couldn't get u2f proxy";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  u2f::GetAssertionRequest req;
  // Requests with UserPresence get upgraded to UserVerification unless
  // verification is explicitly discouraged.
  req.set_verification_type(
      (request.user_verification == UserVerificationRequirement::kDiscouraged)
          ? u2f::VERIFICATION_USER_PRESENCE
          : u2f::VERIFICATION_USER_VERIFICATION);
  req.set_rp_id(request.rp_id);
  req.set_client_data_hash(std::string(request.client_data_hash.begin(),
                                       request.client_data_hash.end()));
  DCHECK(generate_request_id_callback_);
  DCHECK_EQ(current_request_id_, 0u);
  current_request_id_ = generate_request_id_callback_.Run();
  req.set_request_id(current_request_id_);

  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_allowed_credential_id(std::string(id.begin(), id.end()));
  }

  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FGetAssertion);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(req);

  // Use infinite timeout because Cancel() will be called when the request
  // times out.
  u2f_proxy->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
      base::BindOnce(&ChromeOSAuthenticator::OnGetAssertionResp,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void ChromeOSAuthenticator::OnGetAssertionResp(CtapGetAssertionRequest request,
                                               GetAssertionCallback callback,
                                               dbus::Response* dbus_response,
                                               dbus::ErrorResponse* error) {
  if (error) {
    FIDO_LOG(ERROR) << "GetAssertion dbus call failed with error: "
                    << error->ToString();
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  dbus::MessageReader reader(dbus_response);
  u2f::GetAssertionResponse resp;
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    FIDO_LOG(ERROR) << "Failed to parse reply for call to GetAssertion";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  FIDO_LOG(DEBUG) << "GetAssertion status: " << resp.status();
  if (resp.status() != u2f::GetAssertionResponse_GetAssertionStatus_SUCCESS ||
      resp.assertion_size() < 1) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                            base::nullopt);
    return;
  }

  u2f::Assertion assertion = resp.assertion(0);

  base::Optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          base::as_bytes(base::make_span(assertion.authenticator_data())));
  if (!authenticator_data) {
    FIDO_LOG(ERROR) << "Authenticator data corrupted.";
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                            base::nullopt);
    return;
  }

  std::vector<uint8_t> signature(assertion.signature().begin(),
                                 assertion.signature().end());
  AuthenticatorGetAssertionResponse response(std::move(*authenticator_data),
                                             std::move(signature));
  const std::string& credential_id = assertion.credential_id();
  response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      std::vector<uint8_t>(credential_id.begin(), credential_id.end())));
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess,
                          std::move(response));
}

bool ChromeOSAuthenticator::HasCredentialForGetAssertionRequest(
    const CtapGetAssertionRequest& request) {
  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(dbus_options);
  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  if (!u2f_proxy) {
    FIDO_LOG(ERROR) << "Couldn't get u2f proxy";
    return false;
  }

  u2f::HasCredentialsRequest req;
  req.set_rp_id(request.rp_id);
  for (const PublicKeyCredentialDescriptor& descriptor : request.allow_list) {
    const std::vector<uint8_t>& id = descriptor.id();
    req.add_credential_id(std::string(id.begin(), id.end()));
  }

  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FHasCredentials);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(req);

  std::unique_ptr<dbus::Response> dbus_response =
      u2f_proxy->CallMethodAndBlock(&method_call, kShortTimeoutMs);

  if (!dbus_response) {
    FIDO_LOG(ERROR) << "HasCredentials dbus call had no response or timed out";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  u2f::HasCredentialsResponse resp;
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    FIDO_LOG(ERROR) << "Failed to parse reply for call to HasCredentials";
    return false;
  }

  return resp.status() ==
             u2f::HasCredentialsResponse_HasCredentialsStatus_SUCCESS &&
         resp.credential_id().size() > 0;
}

void ChromeOSAuthenticator::Cancel() {
  if (current_request_id_ == 0u)
    return;

  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(dbus_options);
  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  if (!u2f_proxy) {
    FIDO_LOG(ERROR) << "Couldn't get u2f proxy, cannot cancel request";
    return;
  }

  u2f::CancelWebAuthnFlowRequest req;
  req.set_request_id(current_request_id_);
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FCancelWebAuthnFlow);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(req);

  // This needs to be non-blocking since canceling the flow involves Ash.
  u2f_proxy->CallMethod(&method_call, kShortTimeoutMs,
                        base::BindOnce(&ChromeOSAuthenticator::OnCancelResp,
                                       weak_factory_.GetWeakPtr()));
}

void ChromeOSAuthenticator::OnCancelResp(dbus::Response* dbus_response) {
  if (!dbus_response) {
    FIDO_LOG(ERROR)
        << "CancelWebAuthnFlow dbus call had no response or timed out";
    return;
  }

  dbus::MessageReader reader(dbus_response);
  u2f::CancelWebAuthnFlowResponse resp;
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    FIDO_LOG(ERROR) << "Failed to parse reply for call to CancelWebAuthnFlow";
    return;
  }

  if (!resp.canceled()) {
    FIDO_LOG(ERROR) << "Failed to cancel WebAuthn request with id "
                    << current_request_id_;
  }

  current_request_id_ = 0u;
}

// static
bool ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailableBlocking() {
  dbus::Bus::Options dbus_options;
  dbus_options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(dbus_options);
  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  if (!u2f_proxy) {
    FIDO_LOG(DEBUG) << "Couldn't get u2f proxy";
    return false;
  }

  u2f::IsUvpaaRequest req;
  dbus::MethodCall method_call(u2f::kU2FInterface, u2f::kU2FIsUvpaa);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(req);

  std::unique_ptr<dbus::Response> dbus_response =
      u2f_proxy->CallMethodAndBlock(&method_call, kShortTimeoutMs);

  if (!dbus_response) {
    FIDO_LOG(DEBUG) << "IsUvpaa dbus call had no response or timed out";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  u2f::IsUvpaaResponse resp;
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    FIDO_LOG(ERROR) << "Failed to parse reply for call to IsUvpaa";
    return false;
  }

  return resp.available();
}

bool ChromeOSAuthenticator::IsInPairingMode() const {
  return false;
}

bool ChromeOSAuthenticator::IsPaired() const {
  return false;
}

bool ChromeOSAuthenticator::RequiresBlePairingPin() const {
  return false;
}

bool ChromeOSAuthenticator::IsChromeOSAuthenticator() const {
  return true;
}

base::WeakPtr<FidoAuthenticator> ChromeOSAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
