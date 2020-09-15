// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/client_data.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/device_event_log/device_event_log.h"
#include "url/gurl.h"

namespace device {

namespace {

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
}

// ToJSONString encodes |in| as a JSON string, using the specific escaping rules
// required by https://github.com/w3c/webauthn/pull/1375.
std::string ToJSONString(base::StringPiece in) {
  std::string ret;
  ret.reserve(in.size() + 2);
  ret.push_back('"');

  const char* const in_bytes = in.data();
  // ICU uses |int32_t| for lengths.
  const int32_t length = base::checked_cast<int32_t>(in.size());
  int32_t offset = 0;

  while (offset < length) {
    const int32_t prior_offset = offset;
    // Input strings must be valid UTF-8.
    uint32_t codepoint;
    CHECK(base::ReadUnicodeCharacter(in_bytes, length, &offset, &codepoint));
    // offset is updated by |ReadUnicodeCharacter| to index the last byte of the
    // codepoint. Increment it to index the first byte of the next codepoint for
    // the subsequent iteration.
    offset++;

    if (codepoint == 0x20 || codepoint == 0x21 ||
        (codepoint >= 0x23 && codepoint <= 0x5b) || codepoint >= 0x5d) {
      ret.append(&in_bytes[prior_offset], &in_bytes[offset]);
    } else if (codepoint == 0x22) {
      ret.append("\\\"");
    } else if (codepoint == 0x5c) {
      ret.append("\\\\");
    } else {
      static const char hextable[17] = "0123456789abcdef";
      ret.append("\\u00");
      ret.push_back(hextable[codepoint >> 4]);
      ret.push_back(hextable[codepoint & 15]);
    }
  }

  ret.push_back('"');
  return ret;
}

}  // namespace

std::string SerializeCollectedClientDataToJson(
    const std::string& type,
    const std::string& origin,
    base::span<const uint8_t> challenge,
    bool is_cross_origin,
    bool use_legacy_u2f_type_key /* = false */) {
  std::string ret;
  ret.reserve(128);

  if (use_legacy_u2f_type_key) {
    ret.append(R"({"typ":)");
  } else {
    ret.append(R"({"type":)");
  }
  ret.append(ToJSONString(type));

  ret.append(R"(,"challenge":)");
  ret.append(ToJSONString(Base64UrlEncode(challenge)));

  ret.append(R"(,"origin":)");
  ret.append(ToJSONString(origin));

  if (is_cross_origin) {
    ret.append(R"(,"crossOrigin":true)");
  } else {
    ret.append(R"(,"crossOrigin":false)");
  }

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-ietf-tls-grease
    ret.append(R"(,"other_keys_can_be_added_here":")");
    ret.append(
        "do not compare clientDataJSON against a template. See "
        "https://goo.gl/yabPex\"");
  }

  ret.append("}");
  return ret;
}

// static
base::Optional<AndroidClientDataExtensionInput>
AndroidClientDataExtensionInput::Parse(const cbor::Value& value) {
  if (!value.is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& map = value.GetMap();
  if (map.size() != 3) {
    return base::nullopt;
  }
  AndroidClientDataExtensionInput ext;
  for (const auto& pair : map) {
    if (!pair.first.is_integer()) {
      return base::nullopt;
    }
    switch (pair.first.GetInteger()) {
      case 1:
        if (!pair.second.is_string()) {
          return base::nullopt;
        }
        ext.type = pair.second.GetString();
        break;
      case 2:
        if (!pair.second.is_string()) {
          return base::nullopt;
        }
        ext.origin = url::Origin::Create(GURL(pair.second.GetString()));
        if (ext.origin.opaque() ||
            ext.origin.Serialize() != pair.second.GetString()) {
          return base::nullopt;
        }
        break;
      case 3:
        if (!pair.second.is_bytestring()) {
          return base::nullopt;
        }
        ext.challenge = pair.second.GetBytestring();
        break;
      default:
        return base::nullopt;
    }
  }
  return ext;
}

AndroidClientDataExtensionInput::AndroidClientDataExtensionInput() = default;
AndroidClientDataExtensionInput::AndroidClientDataExtensionInput(
    std::string type_,
    url::Origin origin_,
    std::vector<uint8_t> challenge_)
    : type(type_), origin(origin_), challenge(challenge_) {}
AndroidClientDataExtensionInput::AndroidClientDataExtensionInput(
    const AndroidClientDataExtensionInput&) = default;
AndroidClientDataExtensionInput::AndroidClientDataExtensionInput(
    AndroidClientDataExtensionInput&&) = default;

AndroidClientDataExtensionInput& AndroidClientDataExtensionInput::operator=(
    const AndroidClientDataExtensionInput&) = default;
AndroidClientDataExtensionInput& AndroidClientDataExtensionInput::operator=(
    AndroidClientDataExtensionInput&&) = default;

AndroidClientDataExtensionInput::~AndroidClientDataExtensionInput() = default;

cbor::Value AsCBOR(const AndroidClientDataExtensionInput& ext) {
  cbor::Value::MapValue map;
  map[cbor::Value(1)] = cbor::Value(ext.type);
  map[cbor::Value(2)] = cbor::Value(ext.origin.Serialize());
  map[cbor::Value(3)] = cbor::Value(ext.challenge);
  return cbor::Value(map);
}

bool IsValidAndroidClientDataJSON(
    const device::AndroidClientDataExtensionInput& extension_input,
    base::StringPiece android_client_data_json) {
  base::Optional<base::Value> client_data =
      base::JSONReader::Read(android_client_data_json);
  if (!client_data || !client_data->is_dict()) {
    FIDO_LOG(ERROR) << "Invalid androidClientData extension: "
                    << android_client_data_json;
    return false;
  }
  const base::DictionaryValue& client_data_dict =
      base::Value::AsDictionaryValue(*client_data);
  std::string type;
  std::string challenge;
  std::string origin;
  std::string android_package_name;
  if (client_data_dict.size() != 4 ||
      !client_data_dict.GetString("type", &type) ||
      type != extension_input.type ||
      !client_data_dict.GetString("challenge", &challenge) ||
      challenge != Base64UrlEncode(extension_input.challenge) ||
      !client_data_dict.GetString("origin", &origin) ||
      origin != extension_input.origin.Serialize() ||
      !client_data_dict.GetString("androidPackageName",
                                  &android_package_name)) {
    FIDO_LOG(ERROR) << "Invalid androidClientData extension: "
                    << android_client_data_json;
    return false;
  }
  return true;
}

}  // namespace device
