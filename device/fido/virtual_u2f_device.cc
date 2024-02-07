// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_u2f_device.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "crypto/ec_private_key.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

using fido_parsing_utils::Append;

namespace {

// First byte of registration response is 0x05 for historical reasons
// not detailed in the spec.
constexpr uint8_t kU2fRegistrationResponseHeader = 0x05;

// Returns an error response with the given status.
std::optional<std::vector<uint8_t>> ErrorStatus(
    apdu::ApduResponse::Status status) {
  return apdu::ApduResponse(std::vector<uint8_t>(), status)
      .GetEncodedResponse();
}

}  // namespace

// VirtualU2fDevice ----------------------------------------------------------

// static
bool VirtualU2fDevice::IsTransportSupported(FidoTransportProtocol transport) {
  return base::Contains(base::flat_set<FidoTransportProtocol>(
                            {FidoTransportProtocol::kUsbHumanInterfaceDevice,
                             FidoTransportProtocol::kBluetoothLowEnergy,
                             FidoTransportProtocol::kNearFieldCommunication}),
                        transport);
}

VirtualU2fDevice::VirtualU2fDevice() = default;

VirtualU2fDevice::VirtualU2fDevice(scoped_refptr<State> state)
    : VirtualFidoDevice(std::move(state)) {
  DCHECK(IsTransportSupported(mutable_state()->transport));
}

VirtualU2fDevice::~VirtualU2fDevice() = default;

// Cancel operation is not supported on U2F devices.
void VirtualU2fDevice::Cancel(CancelToken) {}

FidoDevice::CancelToken VirtualU2fDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback cb) {
  // Note, here we are using the code-under-test in this fake.
  auto parsed_command = apdu::ApduCommand::CreateFromMessage(command);

  // If malformed U2F request is received, respond with error immediately.
  if (!parsed_command) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(cb),
            ErrorStatus(apdu::ApduResponse::Status::SW_INS_NOT_SUPPORTED)));
    return 0;
  }

  if (mutable_state()->simulate_invalid_response) {
    std::vector<uint8_t> nonsense = {1, 2, 3};
    auto response = apdu::ApduResponse(std::move(nonsense),
                                       apdu::ApduResponse::Status::SW_NO_ERROR)
                        .GetEncodedResponse();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::move(response)));
    return 0;
  }

  std::optional<std::vector<uint8_t>> response;

  switch (parsed_command->ins()) {
    // Version request is defined by the U2F spec, but is never used in
    // production code.
    case base::strict_cast<uint8_t>(U2fApduInstruction::kVersion):
      break;
    case base::strict_cast<uint8_t>(U2fApduInstruction::kRegister):
      response = DoRegister(parsed_command->ins(), parsed_command->p1(),
                            parsed_command->p2(), parsed_command->data());
      break;
    case base::strict_cast<uint8_t>(U2fApduInstruction::kSign):
      response = DoSign(parsed_command->ins(), parsed_command->p1(),
                        parsed_command->p2(), parsed_command->data());
      break;
    default:
      response = ErrorStatus(apdu::ApduResponse::Status::SW_INS_NOT_SUPPORTED);
  }

  if (response) {
    // Call |callback| via the |MessageLoop| because |AuthenticatorImpl| doesn't
    // support callback hairpinning.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::move(response)));
  }
  return 0;
}

base::WeakPtr<FidoDevice> VirtualU2fDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::optional<std::vector<uint8_t>> VirtualU2fDevice::DoRegister(
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    base::span<const uint8_t> data) {
  if (data.size() != 64) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_LENGTH);
  }

  if (!SimulatePress()) {
    return std::nullopt;
  }

  auto challenge_param = data.first<32>();
  auto application_parameter = data.last<32>();

  // Create key to register.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::unique_ptr<PrivateKey> private_key(PrivateKey::FreshP256Key());
  std::vector<uint8_t> x962 = private_key->GetX962PublicKey();

  if (mutable_state()->u2f_invalid_public_key) {
    // Flip a bit in the x-coordinate, which will push the point off the curve.
    x962[10] ^= 1;
  }

  // Our key handles are simple hashes of the public key.
  const auto key_handle = crypto::SHA256Hash(x962);

  // Data to be signed.
  std::vector<uint8_t> sign_buffer;
  sign_buffer.reserve(1 + application_parameter.size() +
                      challenge_param.size() + key_handle.size() + x962.size());
  sign_buffer.push_back(0x00);
  Append(&sign_buffer, application_parameter);
  Append(&sign_buffer, challenge_param);
  Append(&sign_buffer, key_handle);
  Append(&sign_buffer, x962);

  // Sign with attestation key.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::vector<uint8_t> sig;
  std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
  bool status = Sign(attestation_private_key.get(), sign_buffer, &sig);
  DCHECK(status);

  // The spec says that the other bits of P1 should be zero. However, Chrome
  // sends Test User Presence (0x03) so we ignore those bits.
  bool individual_attestation_requested = p1 & kP1IndividualAttestation;
  const auto attestation_cert = GenerateAttestationCertificate(
      individual_attestation_requested, /*include_transports=*/true);
  if (!attestation_cert)
    return ErrorStatus(apdu::ApduResponse::Status::SW_INS_NOT_SUPPORTED);

  // U2F response data.
  std::vector<uint8_t> response;
  response.reserve(1 + x962.size() + 1 + key_handle.size() +
                   attestation_cert->size() + sig.size());
  response.push_back(kU2fRegistrationResponseHeader);
  Append(&response, base::as_bytes(base::make_span(x962)));
  response.push_back(key_handle.size());
  Append(&response, key_handle);
  Append(&response, *attestation_cert);
  Append(&response, sig);

  RegistrationData registration_data(
      std::move(private_key), application_parameter, 1 /* signature counter */);
  registration_data.is_u2f = true;
  StoreNewKey(key_handle, std::move(registration_data));
  return apdu::ApduResponse(std::move(response),
                            apdu::ApduResponse::Status::SW_NO_ERROR)
      .GetEncodedResponse();
}

std::optional<std::vector<uint8_t>> VirtualU2fDevice::DoSign(
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    base::span<const uint8_t> data) {
  if (!(p1 == kP1CheckOnly || p1 == kP1TupRequiredConsumed ||
        p1 == kP1IndividualAttestation) ||
      p2 != 0) {
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_DATA);
  }

  if (!SimulatePress()) {
    return std::nullopt;
  }

  if (data.size() < 32 + 32 + 1)
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_LENGTH);

  auto challenge_param = data.first<32>();
  auto application_parameter = data.subspan<32, 32>();
  size_t key_handle_length = data[64];
  if (data.size() != 32 + 32 + 1 + key_handle_length)
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_LENGTH);

  auto key_handle = data.last(key_handle_length);
  auto* registration = FindRegistrationData(key_handle, application_parameter);
  if (!registration)
    return ErrorStatus(apdu::ApduResponse::Status::SW_WRONG_DATA);

  ++registration->counter;

  // First create the part of the response that gets signed over.
  std::vector<uint8_t> response;
  response.push_back(0x01);  // Always pretend we got a touch.
  response.push_back(registration->counter >> 24);
  response.push_back(registration->counter >> 16);
  response.push_back(registration->counter >> 8);
  response.push_back(registration->counter);

  std::vector<uint8_t> sign_buffer;
  sign_buffer.reserve(application_parameter.size() + response.size() +
                      challenge_param.size());
  Append(&sign_buffer, application_parameter);
  Append(&sign_buffer, response);
  Append(&sign_buffer, challenge_param);

  // Sign with credential key.
  std::vector<uint8_t> sig = registration->private_key->Sign(sign_buffer);

  if (mutable_state()->u2f_invalid_signature) {
    // Flip a bit in the ASN.1 header to make the signature structurally
    // invalid.
    sig[0] ^= 1;
  }

  // Add signature for full response.
  Append(&response, sig);

  mutable_state()->NotifyAssertion(std::make_pair(key_handle, registration));
  return apdu::ApduResponse(std::move(response),
                            apdu::ApduResponse::Status::SW_NO_ERROR)
      .GetEncodedResponse();
}

}  // namespace device
