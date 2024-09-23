// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/jwk.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/status.h"

// JSON Web Key Format (JWK) is defined by:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key
//
// A JWK is a simple JSON dictionary with the following members:
// - "kty" (Key Type) Parameter, REQUIRED
// - <kty-specific parameters, see below>, REQUIRED
// - "use" (Key Use) OPTIONAL
// - "key_ops" (Key Operations) OPTIONAL
// - "alg" (Algorithm) OPTIONAL
// - "ext" (Key Exportability), OPTIONAL
// (all other entries are ignored)
//
// The <kty-specific parameters> are defined by the JWA spec:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms

namespace webcrypto {

namespace {

// |kJwkEncUsage| and |kJwkSigUsage| are a superset of the possible meanings of
// JWK's {"use":"enc"}, and {"use":"sig"} respectively.
//
// TODO(crbug.com/40724054): Remove these masks,
// as they are not consistent with the Web Crypto
// processing model for JWK. In particular,
// intersecting the usages after processing the JWK
// means Chrome can fail with a Syntax error in cases
// where the spec describes a Data error.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt |
    blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey |
    blink::kWebCryptoKeyUsageDeriveKey | blink::kWebCryptoKeyUsageDeriveBits;
const blink::WebCryptoKeyUsageMask kJwkSigUsage =
    blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify;

// Checks that the "ext" member of the JWK is consistent with
// "expected_extractable".
Status VerifyExt(const JwkReader& jwk, bool expected_extractable) {
  // JWK "ext" (optional) --> extractable parameter
  bool jwk_ext_value = false;
  bool has_jwk_ext;
  Status status = jwk.GetOptionalBool("ext", &jwk_ext_value, &has_jwk_ext);
  if (status.IsError())
    return status;
  if (has_jwk_ext && expected_extractable && !jwk_ext_value)
    return Status::ErrorJwkExtInconsistent();
  return Status::Success();
}

struct JwkToWebCryptoUsageMapping {
  const char* const jwk_key_op;
  const blink::WebCryptoKeyUsage webcrypto_usage;
};

// Keep this ordered the same as WebCrypto's "recognized key usage
// values". While this is not required for spec compliance,
// it makes the ordering of key_ops match that of WebCrypto's Key.usages.
const JwkToWebCryptoUsageMapping kJwkWebCryptoUsageMap[] = {
    {"encrypt", blink::kWebCryptoKeyUsageEncrypt},
    {"decrypt", blink::kWebCryptoKeyUsageDecrypt},
    {"sign", blink::kWebCryptoKeyUsageSign},
    {"verify", blink::kWebCryptoKeyUsageVerify},
    {"deriveKey", blink::kWebCryptoKeyUsageDeriveKey},
    {"deriveBits", blink::kWebCryptoKeyUsageDeriveBits},
    {"wrapKey", blink::kWebCryptoKeyUsageWrapKey},
    {"unwrapKey", blink::kWebCryptoKeyUsageUnwrapKey}};

bool JwkKeyOpToWebCryptoUsage(std::string_view key_op,
                              blink::WebCryptoKeyUsage* usage) {
  for (const auto& crypto_usage_entry : kJwkWebCryptoUsageMap) {
    if (crypto_usage_entry.jwk_key_op == key_op) {
      *usage = crypto_usage_entry.webcrypto_usage;
      return true;
    }
  }
  return false;
}

// Creates a JWK key_ops list from a Web Crypto usage mask.
base::Value::List CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usages) {
  base::Value::List jwk_key_ops;
  for (const auto& crypto_usage_entry : kJwkWebCryptoUsageMap) {
    if (usages & crypto_usage_entry.webcrypto_usage)
      jwk_key_ops.Append(crypto_usage_entry.jwk_key_op);
  }
  return jwk_key_ops;
}

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
Status GetWebCryptoUsagesFromJwkKeyOps(const base::Value::List& key_ops,
                                       blink::WebCryptoKeyUsageMask* usages) {
  // This set keeps track of all unrecognized key_ops values.
  std::set<std::string> unrecognized_usages;

  *usages = 0;
  for (size_t i = 0; i < key_ops.size(); ++i) {
    const base::Value& key_op_value = key_ops[i];
    if (!key_op_value.is_string()) {
      return Status::ErrorJwkMemberWrongType(
          base::StringPrintf("key_ops[%d]", static_cast<int>(i)), "string");
    }

    std::string key_op = key_op_value.GetString();

    blink::WebCryptoKeyUsage usage;
    if (JwkKeyOpToWebCryptoUsage(key_op, &usage)) {
      // Ensure there are no duplicate usages.
      if (*usages & usage)
        return Status::ErrorJwkDuplicateKeyOps();
      *usages |= usage;
    }

    // Reaching here means the usage was unrecognized. Such usages are skipped
    // over, however they are kept track of in a set to ensure there were no
    // duplicates.
    if (!unrecognized_usages.insert(key_op).second)
      return Status::ErrorJwkDuplicateKeyOps();
  }
  return Status::Success();
}

// Checks that the usages ("use" and "key_ops") of the JWK is consistent with
// "expected_usages".
Status VerifyUsages(const JwkReader& jwk,
                    blink::WebCryptoKeyUsageMask expected_usages) {
  // JWK "key_ops" (optional) --> usages parameter
  const base::Value::List* jwk_key_ops_value = nullptr;
  bool has_jwk_key_ops;
  Status status =
      jwk.GetOptionalList("key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(*jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usages must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, expected_usages))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usages parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status = jwk.GetOptionalString("use", &jwk_use_value, &has_jwk_use);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_use_mask = 0;
  if (has_jwk_use) {
    if (jwk_use_value == "enc")
      jwk_use_mask = kJwkEncUsage;
    else if (jwk_use_value == "sig")
      jwk_use_mask = kJwkSigUsage;
    else
      return Status::ErrorJwkUnrecognizedUse();
    // The input usages must be a subset of jwk_use_mask.
    if (!ContainsKeyUsages(jwk_use_mask, expected_usages))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  return Status::Success();
}

}  // namespace

JwkReader::JwkReader() = default;

JwkReader::~JwkReader() = default;

Status JwkReader::Init(base::span<const uint8_t> bytes,
                       bool expected_extractable,
                       blink::WebCryptoKeyUsageMask expected_usages,
                       std::string_view expected_kty,
                       std::string_view expected_alg) {
  // Parse the incoming JWK JSON.
  std::string_view json_string(reinterpret_cast<const char*>(bytes.data()),
                               bytes.size());

  {
    // Limit the visibility for |value| as it is moved to |dict_| (via
    // |dict_value|) once it has been loaded successfully.
    std::optional<base::Value> dict = base::JSONReader::Read(json_string);

    if (!dict.has_value() || !dict->is_dict())
      return Status::ErrorJwkNotDictionary();

    dict_ = std::move(dict.value()).TakeDict();
  }

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string kty;
  Status status = GetString("kty", &kty);
  if (status.IsError())
    return status;

  if (kty != expected_kty)
    return Status::ErrorJwkUnexpectedKty(expected_kty);

  status = VerifyExt(*this, expected_extractable);
  if (status.IsError())
    return status;

  status = VerifyUsages(*this, expected_usages);
  if (status.IsError())
    return status;

  // Verify the algorithm if an expectation was provided.
  if (!expected_alg.empty()) {
    status = VerifyAlg(expected_alg);
    if (status.IsError())
      return status;
  }

  return Status::Success();
}

bool JwkReader::HasMember(std::string_view member_name) const {
  return dict_.contains(member_name);
}

Status JwkReader::GetString(std::string_view member_name,
                            std::string* result) const {
  const base::Value* value = dict_.Find(member_name);
  if (!value) {
    return Status::ErrorJwkMemberMissing(member_name);
  }
  if (!value->is_string()) {
    return Status::ErrorJwkMemberWrongType(member_name, "string");
  }
  *result = value->GetString();
  return Status::Success();
}

Status JwkReader::GetOptionalString(std::string_view member_name,
                                    std::string* result,
                                    bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = dict_.Find(member_name);
  if (!value) {
    return Status::Success();
  }

  if (!value->is_string()) {
    return Status::ErrorJwkMemberWrongType(member_name, "string");
  }

  *result = value->GetString();
  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetOptionalList(std::string_view member_name,
                                  const base::Value::List** result,
                                  bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = dict_.Find(member_name);
  if (!value) {
    return Status::Success();
  }

  if (!value->is_list()) {
    return Status::ErrorJwkMemberWrongType(member_name, "list");
  }

  *result = &value->GetList();
  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetBytes(std::string_view member_name,
                           std::vector<uint8_t>* result) const {
  std::string base64_string;
  Status status = GetString(member_name, &base64_string);
  if (status.IsError())
    return status;

  // The JSON web signature spec says that padding is omitted.
  // https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
  std::string result_str;
  if (!base::Base64UrlDecode(base64_string,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &result_str)) {
    return Status::ErrorJwkBase64Decode(member_name);
  }

  result->assign(result_str.begin(), result_str.end());
  return Status::Success();
}

Status JwkReader::GetBigInteger(std::string_view member_name,
                                std::vector<uint8_t>* result) const {
  Status status = GetBytes(member_name, result);
  if (status.IsError())
    return status;

  if (result->empty())
    return Status::ErrorJwkEmptyBigInteger(member_name);

  // The JWA spec says that "The octet sequence MUST utilize the minimum number
  // of octets to represent the value." This means there shouldn't be any
  // leading zeros.
  if (result->size() > 1 && (*result)[0] == 0)
    return Status::ErrorJwkBigIntegerHasLeadingZero(member_name);

  return Status::Success();
}

Status JwkReader::GetOptionalBool(std::string_view member_name,
                                  bool* result,
                                  bool* member_exists) const {
  *member_exists = false;
  const base::Value* value = dict_.Find(member_name);
  if (!value) {
    return Status::Success();
  }

  if (!value->is_bool()) {
    return Status::ErrorJwkMemberWrongType(member_name, "boolean");
  }

  *result = value->GetBool();
  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetAlg(std::string* alg, bool* has_alg) const {
  return GetOptionalString("alg", alg, has_alg);
}

Status JwkReader::VerifyAlg(std::string_view expected_alg) const {
  bool has_jwk_alg;
  std::string jwk_alg_value;
  Status status = GetAlg(&jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg && jwk_alg_value != expected_alg)
    return Status::ErrorJwkAlgorithmInconsistent();

  return Status::Success();
}

JwkWriter::JwkWriter(std::string_view algorithm,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usages,
                     std::string_view kty) {
  if (!algorithm.empty()) {
    dict_.Set("alg", algorithm);
  }
  dict_.Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(usages));
  dict_.Set("ext", extractable);
  dict_.Set("kty", kty);
}

void JwkWriter::SetString(std::string_view member_name,
                          std::string_view value) {
  dict_.Set(member_name, value);
}

void JwkWriter::SetBytes(std::string_view member_name,
                         base::span<const uint8_t> value) {
  // The JSON web signature spec says that padding is omitted.
  // https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
  std::string base64url_encoded;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(value.data()),
                       value.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64url_encoded);

  dict_.Set(member_name, std::move(base64url_encoded));
}

void JwkWriter::ToJson(std::vector<uint8_t>* utf8_bytes) const {
  std::string json;
  base::JSONWriter::Write(dict_, &json);
  utf8_bytes->assign(json.begin(), json.end());
}

Status GetWebCryptoUsagesFromJwkKeyOpsForTest(
    const base::Value::List& key_ops,
    blink::WebCryptoKeyUsageMask* usages) {
  return GetWebCryptoUsagesFromJwkKeyOps(key_ops, usages);
}

}  // namespace webcrypto
