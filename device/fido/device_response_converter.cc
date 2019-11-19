// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/device_response_converter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_constants.h"
#include "device/fido/opaque_attestation_statement.h"

namespace device {

namespace {

constexpr size_t kResponseCodeLength = 1;

ProtocolVersion ConvertStringToProtocolVersion(base::StringPiece version) {
  if (version == kCtap2Version)
    return ProtocolVersion::kCtap2;
  if (version == kU2fVersion)
    return ProtocolVersion::kU2f;

  return ProtocolVersion::kUnknown;
}

// Converts a CBOR unsigned integer value to a uint32_t. The conversion is
// clamped at uint32_max.
uint32_t CBORUnsignedToUint32Safe(const cbor::Value& value) {
  DCHECK(value.is_unsigned());
  constexpr uint32_t uint32_max = std::numeric_limits<uint32_t>::max();
  const int64_t n = value.GetUnsigned();
  return n > uint32_max ? uint32_max : n;
}

}  // namespace

using CBOR = cbor::Value;

CtapDeviceResponseCode GetResponseCode(base::span<const uint8_t> buffer) {
  if (buffer.empty())
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;

  auto code = static_cast<CtapDeviceResponseCode>(buffer[0]);
  return base::Contains(GetCtapResponseCodeList(), code)
             ? code
             : CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
}

// Decodes byte array response from authenticator to CBOR value object and
// checks for correct encoding format.
base::Optional<AuthenticatorMakeCredentialResponse>
ReadCTAPMakeCredentialResponse(FidoTransportProtocol transport_used,
                               const base::Optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map())
    return base::nullopt;

  const auto& decoded_map = cbor->GetMap();
  auto it = decoded_map.find(CBOR(1));
  if (it == decoded_map.end() || !it->second.is_string())
    return base::nullopt;
  auto format = it->second.GetString();

  it = decoded_map.find(CBOR(2));
  if (it == decoded_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!authenticator_data)
    return base::nullopt;

  it = decoded_map.find(CBOR(3));
  if (it == decoded_map.end() || !it->second.is_map())
    return base::nullopt;

  return AuthenticatorMakeCredentialResponse(
      transport_used,
      AttestationObject(std::move(*authenticator_data),
                        std::make_unique<OpaqueAttestationStatement>(
                            format, it->second.Clone())));
}

base::Optional<AuthenticatorGetAssertionResponse> ReadCTAPGetAssertionResponse(
    const base::Optional<cbor::Value>& cbor) {
  if (!cbor || !cbor->is_map())
    return base::nullopt;

  auto& response_map = cbor->GetMap();

  auto it = response_map.find(CBOR(2));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto auth_data =
      AuthenticatorData::DecodeAuthenticatorData(it->second.GetBytestring());
  if (!auth_data)
    return base::nullopt;

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring())
    return base::nullopt;

  auto signature = it->second.GetBytestring();
  AuthenticatorGetAssertionResponse response(std::move(*auth_data),
                                             std::move(signature));

  it = response_map.find(CBOR(1));
  if (it != response_map.end()) {
    auto credential =
        PublicKeyCredentialDescriptor::CreateFromCBORValue(it->second);
    if (!credential)
      return base::nullopt;
    response.SetCredential(std::move(*credential));
  }

  it = response_map.find(CBOR(4));
  if (it != response_map.end()) {
    auto user = PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
    if (!user)
      return base::nullopt;
    response.SetUserEntity(std::move(*user));
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.SetNumCredentials(it->second.GetUnsigned());
  }

  return base::Optional<AuthenticatorGetAssertionResponse>(std::move(response));
}

base::Optional<AuthenticatorGetInfoResponse> ReadCTAPGetInfoResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() <= kResponseCodeLength ||
      GetResponseCode(buffer) != CtapDeviceResponseCode::kSuccess)
    return base::nullopt;

  cbor::Reader::DecoderError error;
  base::Optional<CBOR> decoded_response =
      cbor::Reader::Read(buffer.subspan(1), &error);

  if (!decoded_response) {
    FIDO_LOG(ERROR) << "-> (CBOR parse error from GetInfo response '"
                    << cbor::Reader::ErrorCodeToString(error)
                    << "' from raw message "
                    << base::HexEncode(buffer.data(), buffer.size()) << ")";
    return base::nullopt;
  }

  if (!decoded_response->is_map())
    return base::nullopt;

  FIDO_LOG(DEBUG) << "-> " << cbor::DiagnosticWriter::Write(*decoded_response);
  const auto& response_map = decoded_response->GetMap();

  auto it = response_map.find(CBOR(1));
  if (it == response_map.end() || !it->second.is_array()) {
    return base::nullopt;
  }

  base::flat_set<ProtocolVersion> protocol_versions;
  base::flat_set<base::StringPiece> advertised_protocols;
  for (const auto& version : it->second.GetArray()) {
    if (!version.is_string())
      return base::nullopt;
    const std::string& version_string = version.GetString();

    if (!advertised_protocols.insert(version_string).second) {
      // Duplicate versions are not allowed.
      return base::nullopt;
    }

    auto protocol = ConvertStringToProtocolVersion(version_string);
    if (protocol == ProtocolVersion::kUnknown) {
      FIDO_LOG(DEBUG) << "Unexpected protocol version received.";
      continue;
    }

    if (!protocol_versions.insert(protocol).second) {
      // A duplicate value will have already caused an error therefore hitting
      // this suggests that |ConvertStringToProtocolVersion| is non-injective.
      NOTREACHED();
      return base::nullopt;
    }
  }

  if (protocol_versions.empty())
    return base::nullopt;

  it = response_map.find(CBOR(3));
  if (it == response_map.end() || !it->second.is_bytestring() ||
      it->second.GetBytestring().size() != kAaguidLength) {
    return base::nullopt;
  }

  AuthenticatorGetInfoResponse response(
      std::move(protocol_versions),
      base::make_span<kAaguidLength>(it->second.GetBytestring()));

  AuthenticatorSupportedOptions options;
  it = response_map.find(CBOR(2));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<std::string> extensions;
    for (const auto& extension : it->second.GetArray()) {
      if (!extension.is_string())
        return base::nullopt;

      const std::string& extension_str = extension.GetString();
      if (extension_str == kExtensionCredProtect) {
        options.supports_cred_protect = true;
      }
      extensions.push_back(extension_str);
    }
    response.extensions = std::move(extensions);
  }

  it = response_map.find(CBOR(4));
  if (it != response_map.end()) {
    if (!it->second.is_map())
      return base::nullopt;

    const auto& option_map = it->second.GetMap();
    auto option_map_it = option_map.find(CBOR(kPlatformDeviceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.is_platform_device = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kResidentKeyMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.supports_resident_key = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kUserPresenceMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      options.supports_user_presence = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kUserVerificationMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      if (option_map_it->second.GetBool()) {
        options.user_verification_availability = AuthenticatorSupportedOptions::
            UserVerificationAvailability::kSupportedAndConfigured;
      } else {
        options.user_verification_availability = AuthenticatorSupportedOptions::
            UserVerificationAvailability::kSupportedButNotConfigured;
      }
    }

    option_map_it = option_map.find(CBOR(kClientPinMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool())
        return base::nullopt;

      if (option_map_it->second.GetBool()) {
        options.client_pin_availability = AuthenticatorSupportedOptions::
            ClientPinAvailability::kSupportedAndPinSet;
      } else {
        options.client_pin_availability = AuthenticatorSupportedOptions::
            ClientPinAvailability::kSupportedButPinNotSet;
      }
    }

    option_map_it = option_map.find(CBOR(kCredentialManagementMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return base::nullopt;
      }
      options.supports_credential_management = option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kCredentialManagementPreviewMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return base::nullopt;
      }
      options.supports_credential_management_preview =
          option_map_it->second.GetBool();
    }

    option_map_it = option_map.find(CBOR(kBioEnrollmentMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return base::nullopt;
      }
      using Availability =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability;

      options.bio_enrollment_availability =
          option_map_it->second.GetBool()
              ? Availability::kSupportedAndProvisioned
              : Availability::kSupportedButUnprovisioned;
    }

    option_map_it = option_map.find(CBOR(kBioEnrollmentPreviewMapKey));
    if (option_map_it != option_map.end()) {
      if (!option_map_it->second.is_bool()) {
        return base::nullopt;
      }
      using Availability =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability;

      options.bio_enrollment_availability_preview =
          option_map_it->second.GetBool()
              ? Availability::kSupportedAndProvisioned
              : Availability::kSupportedButUnprovisioned;
    }

    response.options = std::move(options);
  }

  it = response_map.find(CBOR(5));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.max_msg_size = CBORUnsignedToUint32Safe(it->second);
  }

  it = response_map.find(CBOR(6));
  if (it != response_map.end()) {
    if (!it->second.is_array())
      return base::nullopt;

    std::vector<uint8_t> supported_pin_protocols;
    for (const auto& protocol : it->second.GetArray()) {
      if (!protocol.is_unsigned())
        return base::nullopt;

      supported_pin_protocols.push_back(protocol.GetUnsigned());
    }
    response.pin_protocols = std::move(supported_pin_protocols);
  }

  it = response_map.find(CBOR(7));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.max_credential_count_in_list =
        CBORUnsignedToUint32Safe(it->second);
  }

  it = response_map.find(CBOR(8));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned())
      return base::nullopt;

    response.max_credential_id_length = CBORUnsignedToUint32Safe(it->second);
  }

  return base::Optional<AuthenticatorGetInfoResponse>(std::move(response));
}

static base::Optional<std::string> FixInvalidUTF8String(
    base::span<const uint8_t> utf8_bytes) {
  // CTAP2 devices must store at least 64 bytes of any string.
  if (utf8_bytes.size() < 64) {
    FIDO_LOG(ERROR) << "Not accepting invalid UTF-8 string because it's only "
                    << utf8_bytes.size() << " bytes long";
    return base::nullopt;
  }

  base::StreamingUtf8Validator validator;
  base::StreamingUtf8Validator::State state;
  size_t longest_valid_prefix_len = 0;

  for (size_t i = 0; i < utf8_bytes.size(); i++) {
    state =
        validator.AddBytes(reinterpret_cast<const char*>(&utf8_bytes[i]), 1);
    switch (state) {
      case base::StreamingUtf8Validator::VALID_ENDPOINT:
        longest_valid_prefix_len = i + 1;
        break;

      case base::StreamingUtf8Validator::INVALID:
        return base::nullopt;

      case base::StreamingUtf8Validator::VALID_MIDPOINT:
        break;
    }
  }

  switch (state) {
    case base::StreamingUtf8Validator::VALID_ENDPOINT:
      // |base::IsStringUTF8|, which the CBOR code uses, is stricter than
      // |StreamingUtf8Validator| in that the former rejects ranges of code
      // points that should never appear. Therefore, if this case occurs, the
      // string is structurally valid as UTF-8, but includes invalid code points
      // and thus we reject it.
      return base::nullopt;

    case base::StreamingUtf8Validator::INVALID:
      // This shouldn't happen because we should return immediately if
      // |INVALID| occurs.
      NOTREACHED();
      return base::nullopt;

    case base::StreamingUtf8Validator::VALID_MIDPOINT: {
      // This string has been truncated. This is the case that we expect to
      // have to handle since CTAP2 devices are permitted to truncate strings
      // without reference to UTF-8.
      const std::string candidate(
          reinterpret_cast<const char*>(utf8_bytes.data()),
          longest_valid_prefix_len);
      // Check that the result is now acceptable to |IsStringUTF8|, which is
      // stricter than |StreamingUtf8Validator|. Without this, a string could
      // have both contained invalid code-points /and/ been truncated, and this
      // function would only have corrected the latter issue.
      if (base::IsStringUTF8(candidate)) {
        return candidate;
      }
      return base::nullopt;
    }
  }
}

typedef bool (*PathPredicate)(const std::vector<const cbor::Value*>&);

static base::Optional<cbor::Value> FixInvalidUTF8Value(
    const cbor::Value& v,
    std::vector<const cbor::Value*>* path,
    PathPredicate predicate) {
  switch (v.type()) {
    case cbor::Value::Type::INVALID_UTF8: {
      if (!predicate(*path)) {
        return base::nullopt;
      }
      base::Optional<std::string> maybe_fixed(
          FixInvalidUTF8String(v.GetInvalidUTF8()));
      if (!maybe_fixed) {
        return base::nullopt;
      }
      return cbor::Value(*maybe_fixed);
    }

    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE:
    case cbor::Value::Type::BYTE_STRING:
    case cbor::Value::Type::STRING:
    case cbor::Value::Type::TAG:
    case cbor::Value::Type::SIMPLE_VALUE:
    case cbor::Value::Type::NONE:
      return v.Clone();

    case cbor::Value::Type::ARRAY: {
      const cbor::Value::ArrayValue& old_array = v.GetArray();
      cbor::Value::ArrayValue new_array;
      new_array.reserve(old_array.size());

      for (const auto& child : old_array) {
        base::Optional<cbor::Value> maybe_fixed =
            FixInvalidUTF8Value(child, path, predicate);
        if (!maybe_fixed) {
          return base::nullopt;
        }
        new_array.emplace_back(std::move(*maybe_fixed));
      }

      return cbor::Value(new_array);
    }

    case cbor::Value::Type::MAP: {
      const cbor::Value::MapValue& old_map = v.GetMap();
      cbor::Value::MapValue new_map;
      new_map.reserve(old_map.size());

      for (const auto& it : old_map) {
        switch (it.first.type()) {
          case cbor::Value::Type::INVALID_UTF8:
            // Invalid strings in map keys are not supported.
            return base::nullopt;

          case cbor::Value::Type::UNSIGNED:
          case cbor::Value::Type::NEGATIVE:
          case cbor::Value::Type::STRING:
            break;

          default:
            // Other types are not permitted as map keys in CTAP2.
            return base::nullopt;
        }

        path->push_back(&it.first);
        base::Optional<cbor::Value> maybe_fixed =
            FixInvalidUTF8Value(it.second, path, predicate);
        path->pop_back();
        if (!maybe_fixed) {
          return base::nullopt;
        }

        new_map.emplace(it.first.Clone(), std::move(*maybe_fixed));
      }

      return cbor::Value(new_map);
    }
  }
}

// ContainsInvalidUTF8 returns true if any element of |v| (recursively) contains
// a string with invalid UTF-8. It bases this determination purely on the type
// of the nodes and doesn't actually check the contents of the strings
// themselves.
static bool ContainsInvalidUTF8(const cbor::Value& v) {
  switch (v.type()) {
    case cbor::Value::Type::INVALID_UTF8:
      return true;

    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE:
    case cbor::Value::Type::BYTE_STRING:
    case cbor::Value::Type::STRING:
    case cbor::Value::Type::TAG:
    case cbor::Value::Type::SIMPLE_VALUE:
    case cbor::Value::Type::NONE:
      return false;

    case cbor::Value::Type::ARRAY: {
      const cbor::Value::ArrayValue& array = v.GetArray();
      for (const auto& child : array) {
        if (ContainsInvalidUTF8(child)) {
          return true;
        }
      }

      return false;
    }

    case cbor::Value::Type::MAP: {
      const cbor::Value::MapValue& map = v.GetMap();
      for (const auto& it : map) {
        if (ContainsInvalidUTF8(it.first) || ContainsInvalidUTF8(it.second)) {
          return true;
        }
      }

      return false;
    }
  }
}

base::Optional<cbor::Value> FixInvalidUTF8(cbor::Value in,
                                           PathPredicate predicate) {
  if (!ContainsInvalidUTF8(in)) {
    // Common case that everything is fine.
    return in;
  }

  std::vector<const cbor::Value*> path;
  return FixInvalidUTF8Value(in, &path, predicate);
}

}  // namespace device
