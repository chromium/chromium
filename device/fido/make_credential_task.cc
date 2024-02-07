// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_task.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "device/base/features.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/pin.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/u2f_register_operation.h"

namespace device {

namespace {

// CTAP 2.0 specifies[1] that once a PIN has been set on an authenticator, the
// PIN is required in order to make a credential. In some cases we don't want to
// prompt for a PIN and so use U2F to make the credential instead.
//
// [1]
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#authenticatorMakeCredential,
// step 6
bool CtapDeviceShouldUseU2fBecauseClientPinIsSet(
    const FidoDevice* device,
    const CtapMakeCredentialRequest& request) {
  if (!IsConvertibleToU2fRegisterCommand(request) ||
      ShouldPreferCTAP2EvenIfItNeedsAPIN(request)) {
    return false;
  }

  DCHECK_EQ(device->supported_protocol(), ProtocolVersion::kCtap2);

  // No need to fall back to U2F if CTAP2 registrations don't require UV.
  if (device->device_info()->options.make_cred_uv_not_required) {
    return false;
  }

  // Don't use U2F for requests that require UV or PIN which U2F doesn't
  // support. Note that |pin_auth| may also be set by GetTouchRequest(), but we
  // don't want those requests to use U2F either if CTAP is supported.
  if (request.user_verification == UserVerificationRequirement::kRequired ||
      request.pin_auth) {
    return false;
  }

  DCHECK(device && device->device_info());
  bool client_pin_set =
      device->device_info()->options.client_pin_availability ==
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  bool supports_u2f =
      base::Contains(device->device_info()->versions, ProtocolVersion::kU2f);
  return client_pin_set && supports_u2f;
}

// ConvertCTAPResponse returns the AuthenticatorMakeCredentialResponse for a
// given CTAP response message in |cbor|. It wraps
// ReadCTAPMakeCredentialResponse() and in addition fills in |is_resident_key|,
// which requires looking at the request and device.
std::optional<AuthenticatorMakeCredentialResponse> ConvertCTAPResponse(
    FidoDevice* device,
    bool resident_key_required,
    const std::optional<cbor::Value>& cbor) {
  DCHECK_EQ(device->supported_protocol(), ProtocolVersion::kCtap2);
  DCHECK(device->device_info());

  std::optional<AuthenticatorMakeCredentialResponse> response =
      ReadCTAPMakeCredentialResponse(device->DeviceTransport(), cbor);
  if (!response) {
    return std::nullopt;
  }

  // Fill in whether the created credential is client-side discoverable
  // (resident). CTAP 2.0 authenticators may decide to treat all credentials as
  // discoverable, so we need to omit the value unless a resident key was
  // required.
  DCHECK(!response->is_resident_key.has_value());
  if (resident_key_required) {
    response->is_resident_key = true;
  } else {
    const bool resident_key_supported =
        device->device_info()->options.supports_resident_key;
    const base::flat_set<Ctap2Version>& ctap2_versions =
        device->device_info()->ctap2_versions;
    DCHECK(!ctap2_versions.empty());
    const bool is_at_least_ctap2_1 = base::ranges::any_of(
        ctap2_versions,
        [](Ctap2Version v) { return v > Ctap2Version::kCtap2_0; });
    if (!resident_key_supported || is_at_least_ctap2_1) {
      response->is_resident_key = false;
    }
  }

  if (device->device_info() && device->device_info()->transports) {
    response->transports = *device->device_info()->transports;
  }

  return response;
}

}  // namespace

MakeCredentialTask::MakeCredentialTask(FidoDevice* device,
                                       CtapMakeCredentialRequest request,
                                       MakeCredentialOptions options,
                                       MakeCredentialTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      options_(std::move(options)),
      callback_(std::move(callback)) {
  // The UV parameter should have been made binary by this point because CTAP2
  // only takes a binary value.
  DCHECK_NE(request_.user_verification,
            UserVerificationRequirement::kPreferred);
}

MakeCredentialTask::~MakeCredentialTask() = default;

// static
CtapMakeCredentialRequest MakeCredentialTask::GetTouchRequest(
    const FidoDevice* device) {
  // We want to flash and wait for a touch. Newer versions of the CTAP2 spec
  // include a provision for blocking for a touch when an empty pinAuth is
  // specified, but devices exist that predate this part of the spec and also
  // the spec says that devices need only do that if they implement PIN support.
  // Therefore, in order to portably wait for a touch, a dummy credential is
  // created. This does assume that the device supports ECDSA P-256, however.
  PublicKeyCredentialUserEntity user({1} /* user ID */);
  // The user name is incorrectly marked as optional in the CTAP2 spec.
  user.name = "dummy";
  CtapMakeCredentialRequest req(
      "" /* client_data_json */, PublicKeyCredentialRpEntity(kDummyRpID),
      std::move(user),
      PublicKeyCredentialParams(
          {{CredentialType::kPublicKey,
            base::strict_cast<int>(CoseAlgorithmIdentifier::kEs256)}}));

  // If a device supports CTAP2 and has PIN support then setting an empty
  // pinAuth should trigger just a touch[1]. Our U2F code also understands
  // this convention.
  // [1]
  // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#using-pinToken-in-authenticatorGetAssertion
  if (device->supported_protocol() == ProtocolVersion::kU2f ||
      (device->device_info() &&
       device->device_info()->options.client_pin_availability !=
           AuthenticatorSupportedOptions::ClientPinAvailability::
               kNotSupported)) {
    req.pin_auth.emplace();
    req.pin_protocol = PINUVAuthProtocol::kV1;
  }

  DCHECK(IsConvertibleToU2fRegisterCommand(req));

  return req;
}

// static
bool MakeCredentialTask::WillUseCTAP2(const FidoDevice* device,
                                      const CtapMakeCredentialRequest& request,
                                      const MakeCredentialOptions& options) {
  return device->supported_protocol() == ProtocolVersion::kCtap2 &&
         !CtapDeviceShouldUseU2fBecauseClientPinIsSet(device, request);
}

void MakeCredentialTask::Cancel() {
  canceled_ = true;

  if (register_operation_) {
    register_operation_->Cancel();
  }
  if (silent_sign_operation_) {
    silent_sign_operation_->Cancel();
  }
}

void MakeCredentialTask::StartTask() {
  if (WillUseCTAP2(device(), request_, options_)) {
    MakeCredential();
  } else {
    // |device_info| should be present iff the device is CTAP2. This will be
    // used in |MaybeRevertU2fFallback| to restore the protocol of CTAP2 devices
    // once this task is complete.
    DCHECK_EQ(device()->supported_protocol() == ProtocolVersion::kCtap2,
              device()->device_info().has_value());
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fRegister();
  }
}

CtapGetAssertionRequest MakeCredentialTask::NextSilentRequest() {
  DCHECK(current_exclude_list_batch_ < exclude_list_batches_.size());
  CtapGetAssertionRequest request(request_.rp.id,
                                  /*client_data_json=*/"");

  request.allow_list = exclude_list_batches_.at(current_exclude_list_batch_);
  request.user_presence_required = false;
  request.user_verification = UserVerificationRequirement::kDiscouraged;

  // If a pinUvAuthToken was obtained for the original request, the silent
  // requests should carry one as well. This is to ensure that excluded
  // credentials with credProtect-level uvRequired can be matched.
  DCHECK_EQ(request_.pin_auth.has_value(),
            request_.pin_token_for_exclude_list_probing.has_value());
  if (request_.pin_token_for_exclude_list_probing) {
    std::tie(request.pin_protocol, request.pin_auth) =
        request_.pin_token_for_exclude_list_probing->PinAuth(
            request.client_data_hash);
  }

  return request;
}

void MakeCredentialTask::MakeCredential() {
  DCHECK_EQ(device()->supported_protocol(), ProtocolVersion::kCtap2);

  // Most authenticators can only process excludeList parameters up to a certain
  // size. Batch the list into chunks according to what the device can handle
  // and filter out IDs that are too large to originate from this device.
  exclude_list_batches_ =
      FilterAndBatchCredentialDescriptors(request_.exclude_list, *device());
  DCHECK(!exclude_list_batches_.empty());

  // If the filtered excludeList is small enough to be sent in a single request,
  // do so. (Note that the exclude list may be empty now, even if it wasn't
  // previously, due to filtering.)
  if (exclude_list_batches_.size() == 1 || device()->NoSilentRequests()) {
    auto request = request_;
    request.exclude_list = exclude_list_batches_.front();
    register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), std::move(request), std::move(callback_),
        base::BindOnce(&ConvertCTAPResponse, device(),
                       request_.resident_key_required),
        /*string_fixup_predicate=*/nullptr);
    register_operation_->Start();
    return;
  }

  // If the filtered list is too large to be sent at once then probe the
  // credential IDs silently.
  silent_sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), NextSilentRequest(),
          base::BindOnce(&MakeCredentialTask::HandleResponseToSilentSignRequest,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&ReadCTAPGetAssertionResponse,
                         device()->DeviceTransport()),
          /*string_fixup_predicate=*/nullptr);
  silent_sign_operation_->Start();
}

void MakeCredentialTask::HandleResponseToSilentSignRequest(
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorGetAssertionResponse> response_data) {
  if (canceled_) {
    return;
  }

  // The authenticator recognized a credential from previous exclude list batch.
  // Send the actual request with only that exclude list batch to collect a
  // touch and and the CTAP2_ERR_CREDENTIAL_EXCLUDED error code.
  if (response_code == CtapDeviceResponseCode::kSuccess) {
    CtapMakeCredentialRequest request = request_;
    request.exclude_list =
        exclude_list_batches_.at(current_exclude_list_batch_);
    register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), std::move(request), std::move(callback_),
        base::BindOnce(&ConvertCTAPResponse, device(),
                       request_.resident_key_required),
        /*string_fixup_predicate=*/nullptr);
    register_operation_->Start();
    return;
  }

  // The authenticator returned an unexpected error. Collect a touch to take the
  // authenticator out of the set of active devices.
  if (!FidoDevice::IsStatusForUnrecognisedCredentialID(response_code)) {
    register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), GetTouchRequest(device()),
        base::BindOnce(&MakeCredentialTask::HandleResponseToDummyTouch,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ConvertCTAPResponse, device(),
                       /*resident_key_required=*/false),
        /*string_fixup_predicate=*/nullptr);
    register_operation_->Start();
    return;
  }

  // The authenticator didn't recognize any credential from the previous exclude
  // list batch. Try the next batch, if there is one.
  current_exclude_list_batch_++;

  if (current_exclude_list_batch_ < exclude_list_batches_.size()) {
    silent_sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentRequest(),
        base::BindOnce(&MakeCredentialTask::HandleResponseToSilentSignRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    silent_sign_operation_->Start();
    return;
  }

  // None of the credentials from the exclude list were recognized. The actual
  // register request may proceed but without the exclude list present in case
  // it exceeds the device's size limit.
  CtapMakeCredentialRequest request = request_;
  request.exclude_list = {};
  register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), std::move(request), std::move(callback_),
      base::BindOnce(&ConvertCTAPResponse, device(),
                     request_.resident_key_required),
      /*string_fixup_predicate=*/nullptr);
  register_operation_->Start();
}

void MakeCredentialTask::HandleResponseToDummyTouch(
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorMakeCredentialResponse> response_data) {
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                           std::nullopt);
}

void MakeCredentialTask::U2fRegister() {
  if (!IsConvertibleToU2fRegisterCommand(request_)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             std::nullopt);
    return;
  }

  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  register_operation_ = std::make_unique<U2fRegisterOperation>(
      device(), std::move(request_),
      base::BindOnce(&MakeCredentialTask::MaybeRevertU2fFallback,
                     weak_factory_.GetWeakPtr()));
  register_operation_->Start();
}

void MakeCredentialTask::MaybeRevertU2fFallback(
    CtapDeviceResponseCode status,
    std::optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  if (device()->device_info()) {
    // This was actually a CTAP2 device, but the protocol version was set to U2F
    // because it had a PIN set and so, in order to make a credential, the U2F
    // interface was used.
    device()->set_supported_protocol(ProtocolVersion::kCtap2);
  }

  DCHECK(!response || *response->is_resident_key == false);

  std::move(callback_).Run(status, std::move(response));
}

std::vector<std::vector<PublicKeyCredentialDescriptor>>
FilterAndBatchCredentialDescriptors(
    const std::vector<PublicKeyCredentialDescriptor>& in,
    const FidoDevice& device) {
  DCHECK_EQ(device.supported_protocol(), ProtocolVersion::kCtap2);
  DCHECK(device.device_info().has_value());

  if (device.NoSilentRequests()) {
    // caBLE devices might not support silent probing, so just put everything
    // into one batch that can will be sent in a non-probing request.
    return {in};
  }

  const auto& device_info = *device.device_info();

  // Note that |max_credential_id_length| of 0 is interpreted as unbounded.
  size_t max_credential_id_length =
      device_info.max_credential_id_length.value_or(0);

  // Protect against devices that claim to have a maximum list length of 0, or
  // to know the maximum list length but not know the maximum size of an
  // individual credential ID.
  size_t max_credential_count_in_list =
      max_credential_id_length > 0
          ? std::max(device_info.max_credential_count_in_list.value_or(1), 1u)
          : 1;

  std::vector<std::vector<PublicKeyCredentialDescriptor>> result;
  result.emplace_back();

  for (const PublicKeyCredentialDescriptor& credential : in) {
    if (0 < max_credential_id_length &&
        max_credential_id_length < credential.id.size()) {
      continue;
    }
    if (result.back().size() == max_credential_count_in_list) {
      result.emplace_back();
    }
    result.back().push_back(credential);
  }

  return result;
}

}  // namespace device
