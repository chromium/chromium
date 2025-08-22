// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/cross_device_request_dispatcher.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_types.h"

namespace content::digital_credentials::cross_device {

namespace {

RemoteError ErrorStringToRemoteError(const std::string& error_str) {
  if (error_str == "USER_CANCELED") {
    return RemoteError::kUserCanceled;
  } else if (error_str == "DEVICE_ABORTED") {
    return RemoteError::kDeviceAborted;
  } else if (error_str == "NO_CREDENTIAL") {
    return RemoteError::kNoCredential;
  }
  return RemoteError::kOther;
}

std::string RequestTypeToString(RequestInfo::RequestType type) {
  switch (type) {
    case RequestInfo::RequestType::kGet:
      return "credential.get";
    case RequestInfo::RequestType::kCreate:
      return "credential.create";
  }
}

std::vector<uint8_t> RequestToJSONBytes(RequestInfo request_info) {
  base::Value::Dict digital;
  digital.Set("digital", std::move(request_info.request));

  base::Value::Dict toplevel;
  toplevel.Set("origin", request_info.rp_origin.Serialize());
  toplevel.Set("requestType", RequestTypeToString(request_info.request_type));
  toplevel.Set("request", std::move(digital));

  std::optional<std::string> json = base::WriteJson(toplevel);
  // WriteJson must not fail in this context.
  return std::vector<uint8_t>(json->begin(), json->end());
}

}  // namespace

RequestDispatcher::RequestDispatcher(
    std::unique_ptr<device::FidoDiscoveryBase> v1_discovery,
    std::unique_ptr<device::FidoDiscoveryBase> v2_discovery,
    RequestInfo request_info,
    CompletionCallback callback)
    : v1_discovery_(std::move(v1_discovery)),
      v2_discovery_(std::move(v2_discovery)),
      request_info_(std::move(request_info)),
      callback_(std::move(callback)) {
  FIDO_LOG(EVENT) << "Starting digital identity flow";
  v1_discovery_->set_observer(this);
  v2_discovery_->set_observer(this);
  v1_discovery_->Start();
  v2_discovery_->Start();
}

RequestDispatcher::~RequestDispatcher() = default;

void RequestDispatcher::AuthenticatorAdded(
    device::FidoDiscoveryBase* discovery,
    device::FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!callback_) {
    return;
  }
  authenticator->InitializeAuthenticator(
      base::BindOnce(&RequestDispatcher::OnAuthenticatorReady,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void RequestDispatcher::OnAuthenticatorReady(
    device::FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!callback_) {
    return;
  }
  device::cablev2::FidoTunnelDevice* tunnel_device =
      authenticator->GetTunnelDevice();
  if (!tunnel_device) {
    // Presumably all discovered FidoAuthenticators will be of the same type and
    // so there's no point in waiting for more.
    FIDO_LOG(ERROR) << "Non-tunnel device discovered";
    std::move(callback_).Run(
        base::unexpected(ProtocolError::kIncompatibleDevice));
    return;
  }
  if (!tunnel_device->features().contains(
          device::cablev2::Feature::kDigitialIdentities)) {
    FIDO_LOG(ERROR)
        << "Hybrid device doesn't advertise support for digital identities";
    std::move(callback_).Run(
        base::unexpected(ProtocolError::kIncompatibleDevice));
    return;
  }
  tunnel_device->DeviceTransactJSON(
      RequestToJSONBytes(std::move(request_info_)),
      base::BindOnce(&RequestDispatcher::OnComplete,
                     weak_factory_.GetWeakPtr()));
}

void RequestDispatcher::AuthenticatorRemoved(
    device::FidoDiscoveryBase* discovery,
    device::FidoAuthenticator* authenticator) {}

void RequestDispatcher::OnComplete(
    std::optional<std::vector<uint8_t>> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "No response for digital credential request";
    std::move(callback_).Run(base::unexpected(ProtocolError::kTransportError));
    return;
  }

  std::optional<base::Value::Dict> json = base::JSONReader::ReadDict(
      std::string_view(reinterpret_cast<const char*>(response->data()),
                       response->size()),
      base::JSON_PARSE_RFC);
  if (!json) {
    FIDO_LOG(ERROR) << "Invalid JSON response: " << base::HexEncode(*response);
    std::move(callback_).Run(base::unexpected(ProtocolError::kInvalidResponse));
    return;
  }

  std::string reserialized;
  base::JSONWriter::WriteWithOptions(
      *json, base::JsonOptions::OPTIONS_PRETTY_PRINT, &reserialized);
  FIDO_LOG(EVENT) << "-> " << reserialized;

  const base::Value::Dict* response_dict = json->FindDict("response");
  if (!response_dict) {
    FIDO_LOG(ERROR) << "no 'response' element in response";
    std::move(callback_).Run(base::unexpected(ProtocolError::kInvalidResponse));
    return;
  }

  const base::Value::Dict* digital = response_dict->FindDict("digital");
  if (!digital) {
    FIDO_LOG(ERROR) << "no 'digital' element in response";
    std::move(callback_).Run(base::unexpected(ProtocolError::kInvalidResponse));
    return;
  }

  const base::Value* error = digital->Find("error");
  if (error) {
    const std::string* error_str = error->GetIfString();
    if (!error_str) {
      FIDO_LOG(ERROR) << "error is not a string";
      std::move(callback_).Run(
          base::unexpected(ProtocolError::kInvalidResponse));
      return;
    }
    std::move(callback_).Run(
        base::unexpected(ErrorStringToRemoteError(*error_str)));
    return;
  }

  const base::Value* data = digital->Find("data");
  if (!data) {
    FIDO_LOG(ERROR) << "response missing both 'error' and 'data'";
    std::move(callback_).Run(base::unexpected(ProtocolError::kInvalidResponse));
    return;
  }

  // The CTAP protocol standards defines the format of the mobile devices
  // response contains a JSON object that has both a protocol and data. Mobile
  // devices are being migrated to support the CTAP standards. First, try to
  // read the proper format, otherwise, fallback to the legacy format.
  if (data->is_dict()) {
    const base::Value::Dict& data_dict = data->GetDict();
    const base::Value* wallet_data = data_dict.Find("data");
    if (wallet_data) {
      FIDO_LOG(EVENT) << "Standard format is received from the mobile device.";
      std::move(callback_).Run(
          Response(DigitalIdentityProvider::DigitalCredential(
              base::OptionalFromPtr(data_dict.FindString("protocol")),
              wallet_data->Clone())));
      return;
    }
  }
  FIDO_LOG(EVENT) << "No proper standard format is received from the mobile "
                     "device. Fallback to legacy format.";
  std::move(callback_).Run(Response(DigitalIdentityProvider::DigitalCredential(
      /*protocol=*/std::nullopt, data->Clone())));
}

}  // namespace content::digital_credentials::cross_device
