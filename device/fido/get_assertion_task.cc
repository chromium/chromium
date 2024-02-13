// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"
#include "device/fido/u2f_sign_operation.h"

namespace device {

namespace {

bool MayFallbackToU2fWithAppIdExtension(
    const FidoDevice& device,
    const CtapGetAssertionRequest& request) {
  bool ctap2_device_supports_u2f =
      device.device_info() &&
      base::Contains(device.device_info()->versions, ProtocolVersion::kU2f);
  return request.alternative_application_parameter &&
         ctap2_device_supports_u2f && !request.allow_list.empty();
}

// SetResponseCredential sets the credential information in |response|. If the
// allow list sent to the authenticator contained only a single entry then the
// authenticator may omit the chosen credential in the response and this
// function will fill it in. Otherwise, the credential chosen by the
// authenticator must be one of the ones requested in the allow list, unless the
// allow list was empty.
bool SetResponseCredential(
    AuthenticatorGetAssertionResponse* response,
    const std::vector<PublicKeyCredentialDescriptor>& allow_list) {
  if (response->credential) {
    if (!allow_list.empty() &&
        !base::Contains(allow_list, response->credential->id,
                        &PublicKeyCredentialDescriptor::id)) {
      return false;
    }

    return true;
  }

  if (allow_list.size() != 1) {
    return false;
  }

  response->credential = allow_list[0];
  return true;
}

// HasCredentialSpecificPRFInputs returns true if |options| specifies any PRF
// inputs that are specific to a credential ID.
bool HasCredentialSpecificPRFInputs(const CtapGetAssertionOptions& options) {
  const size_t num = options.prf_inputs.size();
  return num > 1 ||
         (num == 1 && options.prf_inputs[0].credential_id.has_value());
}

// GetDefaultPRFInput returns the default PRF input from |options|, if any.
const PRFInput* GetDefaultPRFInput(const CtapGetAssertionOptions& options) {
  if (options.prf_inputs.empty() ||
      options.prf_inputs[0].credential_id.has_value()) {
    return nullptr;
  }
  return &options.prf_inputs[0];
}

// GetPRFInputForCredential returns the PRF input specific to the given
// credential ID from |options|, or the default PRF input if there's nothing
// specific for |id|, or |nullptr| if there's not a default value.
const PRFInput* GetPRFInputForCredential(const CtapGetAssertionOptions& options,
                                         const std::vector<uint8_t>& id) {
  for (const auto& prf_input : options.prf_inputs) {
    if (prf_input.credential_id == id) {
      return &prf_input;
    }
  }
  return GetDefaultPRFInput(options);
}

}  // namespace

GetAssertionTask::GetAssertionTask(FidoDevice* device,
                                   CtapGetAssertionRequest request,
                                   CtapGetAssertionOptions options,
                                   GetAssertionTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      options_(std::move(options)),
      callback_(std::move(callback)) {
  // This code assumes that user-presence is requested in order to implement
  // possible U2F-fallback.
  DCHECK(request_.user_presence_required);

  // The UV parameter should have been made binary by this point because CTAP2
  // only takes a binary value.
  DCHECK_NE(request_.user_verification,
            UserVerificationRequirement::kPreferred);
}

GetAssertionTask::~GetAssertionTask() = default;

void GetAssertionTask::Cancel() {
  canceled_ = true;

  if (sign_operation_) {
    sign_operation_->Cancel();
  }
  if (dummy_register_operation_) {
    dummy_register_operation_->Cancel();
  }
}

// static
bool GetAssertionTask::StringFixupPredicate(
    const std::vector<const cbor::Value*>& path) {
  // This filters out all elements that are not string-keyed, direct children
  // of key 0x04, which is the `user` element of a getAssertion response.
  if (path.size() != 2 || !path[0]->is_unsigned() ||
      path[0]->GetUnsigned() != 4 || !path[1]->is_string()) {
    return false;
  }

  // Of those string-keyed children, only `name` and `displayName` may have
  // truncated UTF-8 in their values.
  const std::string& user_key = path[1]->GetString();
  return user_key == "name" || user_key == "displayName";
}

void GetAssertionTask::StartTask() {
  if (device()->supported_protocol() == ProtocolVersion::kCtap2) {
    GetAssertion();
  } else {
    // |device_info| should be present iff the device is CTAP2.
    // |MaybeRevertU2fFallbackAndInvokeCallback| uses this to restore the
    // protocol of CTAP2 devices once this task is complete.
    DCHECK_EQ(device()->supported_protocol() == ProtocolVersion::kCtap2,
              device()->device_info().has_value());
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fSign();
  }
}

CtapGetAssertionRequest GetAssertionTask::NextSilentRequest() {
  DCHECK(current_allow_list_batch_ < allow_list_batches_.size());
  CtapGetAssertionRequest request = request_;
  request.allow_list = allow_list_batches_.at(current_allow_list_batch_++);
  request.user_presence_required = false;
  request.user_verification = UserVerificationRequirement::kDiscouraged;
  return request;
}

void GetAssertionTask::GetAssertion() {
  if (request_.allow_list.empty()) {
    MaybeSetPRFParameters(&request_, GetDefaultPRFInput(options_));

    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), request_,
        base::BindOnce(&GetAssertionTask::HandleResponse,
                       weak_factory_.GetWeakPtr(), request_.allow_list),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        StringFixupPredicate);
    sign_operation_->Start();
    return;
  }

  // Most authenticators can only process allowList parameters up to a certain
  // size. Batch the list into chunks according to what the device can handle
  // and filter out IDs that are too large to originate from this device.
  allow_list_batches_ =
      FilterAndBatchCredentialDescriptors(request_.allow_list, *device());
  DCHECK(!allow_list_batches_.empty());

  // If filtering eliminated all entries from the allowList, just collect a
  // dummy touch, then fail the request.
  if (allow_list_batches_.size() == 1 && allow_list_batches_[0].empty()) {
    dummy_register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), MakeCredentialTask::GetTouchRequest(device()),
        base::BindOnce(&GetAssertionTask::HandleDummyMakeCredentialComplete,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPMakeCredentialResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    dummy_register_operation_->Start();
    return;
  }

  // If the filtered allowList is small enough to be sent in a single request,
  // do so.
  if (allow_list_batches_.size() == 1 &&
      !MayFallbackToU2fWithAppIdExtension(*device(), request_) &&
      !HasCredentialSpecificPRFInputs(options_)) {
    CtapGetAssertionRequest request = request_;
    request.allow_list = allow_list_batches_.front();
    MaybeSetPRFParameters(&request, GetDefaultPRFInput(options_));

    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), std::move(request),
        base::BindOnce(&GetAssertionTask::HandleResponse,
                       weak_factory_.GetWeakPtr(), request.allow_list),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        StringFixupPredicate);
    sign_operation_->Start();
    return;
  }

  // If the filtered list is too large to be sent at once, or if an App ID might
  // need to be tested because the site used the appid extension, or if we might
  // need to send specific PRF inputs, probe the credential IDs silently.
  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), NextSilentRequest(),
          base::BindOnce(&GetAssertionTask::HandleResponseToSilentRequest,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&ReadCTAPGetAssertionResponse,
                         device()->DeviceTransport()),
          /*string_fixup_predicate=*/nullptr);
  sign_operation_->Start();
}

void GetAssertionTask::U2fSign() {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());

  sign_operation_ = std::make_unique<U2fSignOperation>(
      device(), request_,
      base::BindOnce(&GetAssertionTask::MaybeRevertU2fFallbackAndInvokeCallback,
                     weak_factory_.GetWeakPtr()));
  sign_operation_->Start();
}

void GetAssertionTask::HandleResponse(
    std::vector<PublicKeyCredentialDescriptor> allow_list,
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorGetAssertionResponse> response_data) {
  if (canceled_) {
    return;
  }

  if (response_code == CtapDeviceResponseCode::kCtap2ErrInvalidCredential) {
    // Some authenticators will return this error before waiting for a touch if
    // they don't recognise a credential. In other cases the result can be
    // returned immediately.
    // The request failed in a way that didn't request a touch. Simulate it.
    dummy_register_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
        device(), MakeCredentialTask::GetTouchRequest(device()),
        base::BindOnce(&GetAssertionTask::HandleDummyMakeCredentialComplete,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPMakeCredentialResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    dummy_register_operation_->Start();
    return;
  }

  if (response_code != CtapDeviceResponseCode::kSuccess) {
    std::move(callback_).Run(response_code, {});
    return;
  }

  if (response_data->user_selected && !allow_list.empty()) {
    // The userSelected signal is only valid if the request had an empty
    // allowList.
    return LogAndFail(
        "Assertion response has userSelected for non-empty allowList");
  }

  if (!SetResponseCredential(&response_data.value(), allow_list)) {
    return LogAndFail("Assertion response has invalid credential information");
  }

  uint8_t num_responses = response_data->num_credentials.value_or(1u);
  if (num_responses == 0 || (num_responses > 1 && !allow_list.empty())) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, {});
    return;
  }

  HandleNextResponse(num_responses, response_code, std::move(response_data));
}

void GetAssertionTask::HandleNextResponse(
    uint8_t num_responses,
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorGetAssertionResponse> response_data) {
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    std::move(callback_).Run(response_code, {});
  }

  // Extract any hmac-secret or prf response.
  const std::optional<cbor::Value>& extensions_cbor =
      response_data->authenticator_data.extensions();
  if (extensions_cbor) {
    // Parsing has already checked that |extensions_cbor| is a map.
    const cbor::Value::MapValue& extensions = extensions_cbor->GetMap();
    auto it = extensions.find(cbor::Value(kExtensionHmacSecret));
    if (it != extensions.end()) {
      if (!hmac_secret_request_ || !it->second.is_bytestring()) {
        return LogAndFail("Unexpected or invalid hmac-secret extension");
      }
      if (response_data->hmac_secret.has_value()) {
        return LogAndFail(
            "Assertion response has both hmac-secret and prf extensions");
      }
      std::optional<std::vector<uint8_t>> plaintext =
          hmac_secret_request_->Decrypt(it->second.GetBytestring());
      if (!plaintext) {
        return LogAndFail("Failed to decrypt hmac-secret extension");
      }
      response_data->hmac_secret = std::move(plaintext.value());
    }
  }

  responses_.emplace_back(std::move(*response_data));

  if (responses_.size() < num_responses) {
    // Read the next response.
    next_assertion_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetNextAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), CtapGetNextAssertionRequest(),
        base::BindOnce(&GetAssertionTask::HandleNextResponse,
                       weak_factory_.GetWeakPtr(), num_responses),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        GetAssertionTask::StringFixupPredicate);
    next_assertion_operation_->Start();
    return;
  }
  std::move(callback_).Run(response_code, std::move(responses_));
}

void GetAssertionTask::HandleResponseToSilentRequest(
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorGetAssertionResponse> response_data) {
  DCHECK(request_.allow_list.size() > 0);
  DCHECK(allow_list_batches_.size() > 0);
  DCHECK(0 < current_allow_list_batch_ &&
         current_allow_list_batch_ <= allow_list_batches_.size());

  if (canceled_) {
    return;
  }

  // One credential from the previous batch was recognized by the device. As
  // this authentication was a silent authentication (i.e. user touch was not
  // provided), try again with only that credential, user presence enforced and
  // with the original user verification configuration.
  if (response_code == CtapDeviceResponseCode::kSuccess &&
      SetResponseCredential(
          &response_data.value(),
          allow_list_batches_.at(current_allow_list_batch_ - 1))) {
    CtapGetAssertionRequest request = request_;
    const PublicKeyCredentialDescriptor& matching_credential =
        *response_data->credential;
    request.allow_list = {matching_credential};
    MaybeSetPRFParameters(
        &request, GetPRFInputForCredential(options_, matching_credential.id));

    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), std::move(request),
        base::BindOnce(&GetAssertionTask::HandleResponse,
                       weak_factory_.GetWeakPtr(), request.allow_list),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    sign_operation_->Start();
    return;
  }

  // Credential was not recognized or an error occurred. Probe the next
  // credential.
  if (current_allow_list_batch_ < allow_list_batches_.size()) {
    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), NextSilentRequest(),
        base::BindOnce(&GetAssertionTask::HandleResponseToSilentRequest,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&ReadCTAPGetAssertionResponse,
                       device()->DeviceTransport()),
        /*string_fixup_predicate=*/nullptr);
    sign_operation_->Start();
    return;
  }

  // None of the credentials were recognized. Fall back to U2F or collect a
  // dummy touch.
  if (MayFallbackToU2fWithAppIdExtension(*device(), request_)) {
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fSign();
    return;
  }
  dummy_register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), MakeCredentialTask::GetTouchRequest(device()),
      base::BindOnce(&GetAssertionTask::HandleDummyMakeCredentialComplete,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ReadCTAPMakeCredentialResponse,
                     device()->DeviceTransport()),
      /*string_fixup_predicate=*/nullptr);
  dummy_register_operation_->Start();
}

void GetAssertionTask::HandleDummyMakeCredentialComplete(
    CtapDeviceResponseCode response_code,
    std::optional<AuthenticatorMakeCredentialResponse> response_data) {
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, {});
}

void GetAssertionTask::MaybeSetPRFParameters(CtapGetAssertionRequest* request,
                                             const PRFInput* maybe_inputs) {
  if (maybe_inputs == nullptr) {
    return;
  }

  hmac_secret_request_ = std::make_unique<pin::HMACSecretRequest>(
      *request->pin_protocol, *options_.pin_key_agreement, maybe_inputs->salt1,
      maybe_inputs->salt2);
  request->hmac_secret.emplace(hmac_secret_request_->public_key_x962,
                               hmac_secret_request_->encrypted_salts,
                               hmac_secret_request_->salts_auth,
                               // The correct PIN protocol will be inserted
                               // automatically when needed.
                               /*pin_protocol=*/std::nullopt);
}

void GetAssertionTask::MaybeRevertU2fFallbackAndInvokeCallback(
    CtapDeviceResponseCode status,
    std::optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  if (device()->device_info()) {
    // This was actually a CTAP2 device, but the protocol version was set to U2F
    // in order to execute a sign command.
    device()->set_supported_protocol(ProtocolVersion::kCtap2);
  }
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(callback_).Run(status, {});
    return;
  }
  responses_.emplace_back(std::move(*response));
  std::move(callback_).Run(status, std::move(responses_));
}

void GetAssertionTask::LogAndFail(const char* error) {
  FIDO_LOG(DEBUG) << error;
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther, {});
}

}  // namespace device
