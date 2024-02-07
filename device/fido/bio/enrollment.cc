// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment.h"

#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

static void SetPinAuth(BioEnrollmentRequest* request,
                       const pin::TokenResponse& token) {
  request->modality = BioEnrollmentModality::kFingerprint;

  std::vector<uint8_t> pin_auth;
  if (request->params)
    pin_auth = *cbor::Writer::Write(cbor::Value(request->params.value()));

  if (request->subcommand)
    pin_auth.insert(pin_auth.begin(), static_cast<int>(*request->subcommand));

  pin_auth.insert(pin_auth.begin(), static_cast<int>(*request->modality));

  std::tie(request->pin_protocol, request->pin_auth) =
      token.PinAuth(std::move(pin_auth));
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetModality(Version version) {
  BioEnrollmentRequest request(version);
  request.get_modality = true;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetSensorInfo(Version version) {
  BioEnrollmentRequest request(version);
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kGetFingerprintSensorInfo;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollBegin(
    Version version,
    const pin::TokenResponse& token) {
  BioEnrollmentRequest request(version);
  request.subcommand = BioEnrollmentSubCommand::kEnrollBegin;
  SetPinAuth(&request, token);
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollNextSample(
    Version version,
    const pin::TokenResponse& token,
    std::vector<uint8_t> template_id) {
  BioEnrollmentRequest request(version);
  request.subcommand = BioEnrollmentSubCommand::kEnrollCaptureNextSample;
  request.params = cbor::Value::MapValue();
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId),
      cbor::Value(template_id));
  SetPinAuth(&request, token);
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForCancel(Version version) {
  BioEnrollmentRequest request(version);
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kCancelCurrentEnrollment;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnumerate(
    Version version,
    const pin::TokenResponse& token) {
  BioEnrollmentRequest request(version);
  request.subcommand = BioEnrollmentSubCommand::kEnumerateEnrollments;
  SetPinAuth(&request, token);
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForRename(
    Version version,
    const pin::TokenResponse& token,
    std::vector<uint8_t> id,
    std::string name) {
  BioEnrollmentRequest request(version);
  request.subcommand = BioEnrollmentSubCommand::kSetFriendlyName;
  request.params = cbor::Value::MapValue();
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId),
      std::move(id));
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateFriendlyName),
      std::move(name));
  SetPinAuth(&request, token);
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForDelete(
    Version version,
    const pin::TokenResponse& token,
    std::vector<uint8_t> id) {
  BioEnrollmentRequest request(version);
  request.subcommand = BioEnrollmentSubCommand::kRemoveEnrollment;
  request.params = cbor::Value::MapValue();
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId),
      std::move(id));
  SetPinAuth(&request, token);
  return request;
}

BioEnrollmentRequest::BioEnrollmentRequest(BioEnrollmentRequest&&) = default;
BioEnrollmentRequest& BioEnrollmentRequest::operator=(BioEnrollmentRequest&&) =
    default;
BioEnrollmentRequest::BioEnrollmentRequest(Version v) : version(v) {}
BioEnrollmentRequest::~BioEnrollmentRequest() = default;

// static
std::optional<BioEnrollmentResponse> BioEnrollmentResponse::Parse(
    const std::optional<cbor::Value>& cbor_response) {
  BioEnrollmentResponse response;

  if (!cbor_response || !cbor_response->is_map()) {
    return response;
  }

  const auto& response_map = cbor_response->GetMap();

  // modality
  auto it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kModality)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    response.modality =
        ToBioEnrollmentEnum<BioEnrollmentModality>(it->second.GetUnsigned());
    if (!response.modality) {
      return std::nullopt;
    }
  }

  // fingerprint kind
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    response.fingerprint_kind =
        ToBioEnrollmentEnum<BioEnrollmentFingerprintKind>(
            it->second.GetUnsigned());
    if (!response.fingerprint_kind) {
      return std::nullopt;
    }
  }

  // max captures required for enroll
  it = response_map.find(cbor::Value(static_cast<int>(
      BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<uint8_t>::max()) {
      return std::nullopt;
    }
    response.max_samples_for_enroll = it->second.GetUnsigned();
  }

  // template id
  it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kTemplateId)));
  if (it != response_map.end()) {
    if (!it->second.is_bytestring()) {
      return std::nullopt;
    }
    response.template_id = it->second.GetBytestring();
  }

  // last enroll sample status
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return std::nullopt;
    }
    response.last_status = ToBioEnrollmentEnum<BioEnrollmentSampleStatus>(
        it->second.GetUnsigned());
    if (!response.last_status) {
      return std::nullopt;
    }
  }

  // remaining samples
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<uint8_t>::max()) {
      return std::nullopt;
    }
    response.remaining_samples = it->second.GetUnsigned();
  }

  // enumerated template infos
  it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kTemplateInfos)));
  if (it != response_map.end()) {
    if (!it->second.is_array()) {
      return std::nullopt;
    }

    std::map<std::vector<uint8_t>, std::string> template_infos;
    for (const auto& bio_template : it->second.GetArray()) {
      if (!bio_template.is_map()) {
        return std::nullopt;
      }
      const cbor::Value::MapValue& template_map = bio_template.GetMap();

      // id (required)
      auto template_it = template_map.find(cbor::Value(
          static_cast<int>(BioEnrollmentTemplateInfoParam::kTemplateId)));
      if (template_it == template_map.end() ||
          !template_it->second.is_bytestring()) {
        return std::nullopt;
      }
      std::vector<uint8_t> id = template_it->second.GetBytestring();
      if (template_infos.find(id) != template_infos.end()) {
        // Found an existing ID, invalid response.
        return std::nullopt;
      }

      // name (optional)
      std::string name;
      template_it = template_map.find(cbor::Value(static_cast<int>(
          BioEnrollmentTemplateInfoParam::kTemplateFriendlyName)));
      if (template_it != template_map.end()) {
        if (!template_it->second.is_string()) {
          return std::nullopt;
        }
        name = template_it->second.GetString();
      }
      template_infos[std::move(id)] = std::move(name);
    }
    response.template_infos = std::move(template_infos);
  }

  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kMaxTemplateFriendlyName)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<uint32_t>::max()) {
      return std::nullopt;
    }
    response.max_template_friendly_name = it->second.GetUnsigned();
  }

  return std::move(response);
}

BioEnrollmentResponse::BioEnrollmentResponse() = default;
BioEnrollmentResponse::BioEnrollmentResponse(BioEnrollmentResponse&&) = default;
BioEnrollmentResponse::~BioEnrollmentResponse() = default;

bool BioEnrollmentResponse::operator==(const BioEnrollmentResponse& r) const {
  return modality == r.modality && fingerprint_kind == r.fingerprint_kind &&
         max_samples_for_enroll == r.max_samples_for_enroll &&
         template_id == r.template_id && last_status == r.last_status &&
         remaining_samples == r.remaining_samples &&
         template_infos == r.template_infos;
}

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const BioEnrollmentRequest& request) {
  cbor::Value::MapValue map;

  using Key = BioEnrollmentRequestKey;

  if (request.modality) {
    map.emplace(static_cast<int>(Key::kModality),
                static_cast<int>(*request.modality));
  }

  if (request.subcommand) {
    map.emplace(static_cast<int>(Key::kSubCommand),
                static_cast<int>(*request.subcommand));
  }

  if (request.params) {
    map.emplace(static_cast<int>(Key::kSubCommandParams), *request.params);
  }

  if (request.pin_protocol) {
    map.emplace(static_cast<int>(Key::kPinProtocol),
                static_cast<uint8_t>(*request.pin_protocol));
  }

  if (request.pin_auth) {
    map.emplace(static_cast<int>(Key::kPinAuth), *request.pin_auth);
  }

  if (request.get_modality) {
    map.emplace(static_cast<int>(Key::kGetModality), *request.get_modality);
  }

  return {request.version == BioEnrollmentRequest::Version::kDefault
              ? CtapRequestCommand::kAuthenticatorBioEnrollment
              : CtapRequestCommand::kAuthenticatorBioEnrollmentPreview,
          cbor::Value(std::move(map))};
}

}  // namespace device
