// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sd_jwt.h"

#include <map>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "crypto/random.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::sdjwt {

namespace {

std::optional<std::string> Base64UrlDecode(const std::string_view& base64) {
  std::string str;
  if (!base::Base64UrlDecode(
          base64, base::Base64UrlDecodePolicy::IGNORE_PADDING, &str)) {
    return std::nullopt;
  }

  return str;
}

Base64String Base64UrlEncode(const std::string_view& str) {
  std::string base64;
  base::Base64UrlEncode(str, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64);
  return Base64String(base64);
}

}  // namespace

Jwk::Jwk() = default;
Jwk::~Jwk() = default;
Jwk::Jwk(const Jwk& other) = default;

// static
std::optional<Jwk> Jwk::From(const base::Value::Dict& dict) {
  Jwk result;

  auto* kty = dict.FindString("kty");
  if (!kty) {
    return std::nullopt;
  }
  result.kty = *kty;

  if (result.kty == "EC") {
    auto* crv = dict.FindString("crv");
    if (!crv) {
      return std::nullopt;
    }
    result.crv = *crv;

    auto* x = dict.FindString("x");
    if (!x) {
      return std::nullopt;
    }
    result.x = *x;

    auto* y = dict.FindString("y");
    if (!y) {
      return std::nullopt;
    }
    result.y = *y;
  } else if (result.kty == "RSA") {
    auto* n = dict.FindString("n");
    if (!n) {
      return std::nullopt;
    }
    result.n = *n;

    auto* e = dict.FindString("e");
    if (!e) {
      return std::nullopt;
    }
    result.e = *e;
  } else {
    return std::nullopt;
  }

  // The "d" parameters is an optional parameter and unavailable in public keys.
  auto* d = dict.FindString("d");
  if (d) {
    result.d = *d;
  }

  auto* alg = dict.FindString("alg");
  if (alg) {
    result.alg = *alg;
  }

  return result;
}

base::Value::Dict Jwk::ToDict() const {
  base::Value::Dict result;

  result.Set("kty", kty);
  if (kty == "EC") {
    result.Set("crv", crv);
    result.Set("x", x);
    result.Set("y", y);
  } else if (kty == "RSA") {
    result.Set("n", n);
    result.Set("e", e);
  } else {
    NOTREACHED() << "Unsupported key type: " << kty;
  }

  // Private parameter d is optional.
  if (!d.empty()) {
    result.Set("d", d);
  }

  if (!alg.empty()) {
    result.Set("alg", alg);
  }
  return result;
}

std::optional<std::string> Jwk::Serialize() const {
  return base::WriteJson(ToDict());
}

Disclosure::Disclosure() = default;
Disclosure::~Disclosure() = default;
Disclosure::Disclosure(const Disclosure& other) = default;

// static
std::optional<Disclosure> Disclosure::From(const base::Value::List& list) {
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-disclosures
  if (list.size() != 3) {
    return std::nullopt;
  }

  if (!list[0].is_string()) {
    return std::nullopt;
  }

  if (!list[1].is_string()) {
    return std::nullopt;
  }

  if (!list[2].is_string()) {
    return std::nullopt;
  }

  Disclosure result;
  result.salt = Base64String(list[0].GetString());
  result.name = list[1].GetString();
  result.value = list[2].GetString();
  return result;
}

std::optional<JSONString> Disclosure::ToJson() const {
  base::Value::List list;

  list.Append(salt.value());
  list.Append(name);
  list.Append(value);

  auto result = base::WriteJson(list);

  if (!result) {
    return std::nullopt;
  }

  return JSONString(*result);
}

Base64String Disclosure::Serialize() const {
  return Base64UrlEncode(ToJson()->value());
}

std::optional<Base64String> Disclosure::Digest(Hasher hasher) const {
  Base64String disclosure = Serialize();
  std::string result;
  base::Base64UrlEncode(hasher.Run(disclosure.value()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &result);
  return Base64String(result);
}

// static
Base64String Disclosure::CreateSalt() {
  const size_t salt_size = 32;
  std::array<uint8_t, salt_size> salt_bytes;
  crypto::RandBytes(salt_bytes);
  std::string salt;
  base::Base64UrlEncode(salt_bytes, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &salt);
  return Base64String(salt);
}

std::optional<SdJwt> SdJwt::From(const base::Value::List& list) {
  if (list.size() != 2) {
    return std::nullopt;
  }

  if (!list[0].is_list() || !list[1].is_list()) {
    return std::nullopt;
  }

  auto jwt = Jwt::From(list[0].GetList());
  if (!jwt) {
    return std::nullopt;
  }

  std::vector<JSONString> disclosures;

  for (auto& disclosure : list[1].GetList()) {
    if (!disclosure.is_string()) {
      return std::nullopt;
    }
    disclosures.push_back(JSONString(disclosure.GetString()));
  }

  SdJwt result;
  result.jwt = *jwt;
  result.disclosures = disclosures;

  return result;
}

// static
std::optional<base::Value::List> SdJwt::Parse(const std::string_view& sdjwt) {
  // First, split the token into the issued JWT and the disclosures.
  auto pair = base::SplitStringOnce(sdjwt, "~");
  if (!pair) {
    return std::nullopt;
  }

  auto jwt = Jwt::Parse(pair->first);
  if (!jwt) {
    return std::nullopt;
  }

  base::Value::List disclosures;

  if (!pair->second.empty()) {
    if (pair->second.back() != '~') {
      return std::nullopt;
    }

    auto list = pair->second.substr(0, pair->second.length() - 1);

    auto parts = base::SplitString(list, "~", base::KEEP_WHITESPACE,
                                   base::SPLIT_WANT_ALL);
    if (parts.empty()) {
      return std::nullopt;
    }

    for (auto disclosure : parts) {
      if (disclosure.empty()) {
        return std::nullopt;
      }

      auto json = Base64UrlDecode(disclosure);
      if (!json) {
        // Failed to decode base64url.
        return std::nullopt;
      }
      disclosures.Append(*json);
    }
  }

  base::Value::List result;
  result.Append(std::move(*jwt));
  result.Append(std::move(disclosures));

  return result;
}

Header::Header() = default;
Header::~Header() = default;
Header::Header(const Header& other) = default;

// static
std::optional<Header> Header::From(const base::Value::Dict& json) {
  Header result;

  auto* typ = json.FindString("typ");
  if (!typ) {
    return std::nullopt;
  }
  result.typ = *typ;

  auto* alg = json.FindString("alg");
  if (!alg) {
    return std::nullopt;
  }
  result.alg = *alg;

  auto* jwk = json.FindDict("jwk");
  if (jwk) {
    // JWK is an optional parameter.
    result.jwk = Jwk::From(*jwk);
  }

  return result;
}

std::optional<JSONString> Header::ToJson() const {
  base::Value::Dict header_dict;

  header_dict.Set("typ", typ);
  header_dict.Set("alg", alg);

  if (jwk) {
    header_dict.Set("jwk", jwk->ToDict());
  }

  auto result = base::WriteJson(header_dict);

  if (!result) {
    return std::nullopt;
  }

  return JSONString(*result);
}

std::optional<Base64String> Header::Serialize() const {
  auto header_json = ToJson();
  if (!header_json) {
    return std::nullopt;
  }
  return Base64UrlEncode(header_json->value());
}

ConfirmationKey::ConfirmationKey() = default;
ConfirmationKey::~ConfirmationKey() = default;
ConfirmationKey::ConfirmationKey(const ConfirmationKey& other) = default;

Payload::Payload() = default;
Payload::~Payload() = default;
Payload::Payload(const Payload& other) = default;

// static
std::optional<Payload> Payload::From(const base::Value::Dict& json) {
  Payload result;

  auto* aud = json.FindString("aud");
  if (aud) {
    result.aud = *aud;
  }

  // We use doubles and cast for longs for "iat", so that this
  // can still work past 2038.
  auto iat = json.FindDouble("iat");
  if (iat) {
    result.iat = base::Time::FromSecondsSinceUnixEpoch(*iat);
  }

  auto exp = json.FindInt("exp");
  if (exp) {
    result.exp = base::Time::FromTimeT(*exp);
  }

  auto* iss = json.FindString("iss");
  if (iss) {
    result.iss = *iss;
  }

  auto* sub = json.FindString("sub");
  if (sub) {
    result.sub = *sub;
  }

  auto* nonce = json.FindString("nonce");
  if (nonce) {
    result.nonce = *nonce;
  }

  auto* vct = json.FindString("vct");
  if (vct) {
    result.vct = *vct;
  }

  auto* cnf = json.FindDictByDottedPath("cnf.jwk");
  if (cnf) {
    auto jwk = Jwk::From(*cnf);
    if (jwk) {
      ConfirmationKey key;
      key.jwk = *jwk;
      result.cnf = key;
    }
  }

  auto* sd_hash = json.FindString("sd_hash");
  if (sd_hash) {
    result.sd_hash = Base64String(*sd_hash);
  }

  auto* _sd_alg = json.FindString("_sd_alg");
  if (_sd_alg) {
    result._sd_alg = *_sd_alg;
  }

  if (json.FindList("_sd")) {
    for (const base::Value& el : *json.FindList("_sd")) {
      if (!el.is_string()) {
        return std::nullopt;
      }
      result._sd.push_back(Base64String(el.GetString()));
    }
  }

  auto* email = json.FindString("email");
  if (email) {
    result.email = *email;
  }

  return result;
}

std::optional<JSONString> Payload::ToJson() const {
  base::Value::Dict payload_dict;

  if (!iss.empty()) {
    payload_dict.Set("iss", iss);
  }

  if (!aud.empty()) {
    payload_dict.Set("aud", aud);
  }

  if (!sub.empty()) {
    payload_dict.Set("sub", sub);
  }

  if (cnf) {
    base::Value::Dict jwk;

    base::Value::Dict cnf_dict;
    cnf_dict.Set("jwk", cnf->jwk.ToDict());
    payload_dict.Set("cnf", std::move(cnf_dict));
  }

  if (!nonce.empty()) {
    payload_dict.Set("nonce", nonce);
  }

  if (!vct.empty()) {
    payload_dict.Set("vct", vct);
  }

  if (iat) {
    payload_dict.Set("iat", static_cast<int>(iat->ToTimeT()));
  }

  if (exp) {
    payload_dict.Set("exp", static_cast<int>(exp->ToTimeT()));
  }

  if (!sd_hash.value().empty()) {
    payload_dict.Set("sd_hash", sd_hash.value());
  }

  if (_sd.size() > 0) {
    base::Value::List list;
    for (const auto& disclosure : _sd) {
      list.Append(disclosure.value());
    }
    payload_dict.Set("_sd", std::move(list));
  }

  if (!_sd_alg.empty()) {
    payload_dict.Set("_sd_alg", _sd_alg);
  }

  if (!email.empty()) {
    payload_dict.Set("email", email);
  }

  auto result = base::WriteJson(payload_dict);

  if (!result) {
    return std::nullopt;
  }

  return JSONString(*result);
}

std::optional<Base64String> Payload::Serialize() const {
  auto payload_json = ToJson();
  if (!payload_json) {
    return std::nullopt;
  }
  return Base64UrlEncode(payload_json->value());
}

Jwt::Jwt() = default;
Jwt::~Jwt() = default;
Jwt::Jwt(const Jwt& other) = default;

JSONString Jwt::Serialize() const {
  std::string result;
  result += Base64UrlEncode(header.value()).value();
  result += ".";
  result += Base64UrlEncode(payload.value()).value();
  result += ".";
  result += signature.value();
  return JSONString(result);
}

// static
std::optional<Jwt> Jwt::From(const base::Value::List& list) {
  if (list.size() != 3) {
    return std::nullopt;
  }

  if (!list[0].is_string() || !list[1].is_string() || !list[2].is_string()) {
    return std::nullopt;
  }

  Jwt result;
  result.header = JSONString(list[0].GetString());
  result.payload = JSONString(list[1].GetString());
  result.signature = Base64String(list[2].GetString());

  return result;
}

// static
std::optional<base::Value::List> Jwt::Parse(const std::string_view& jwt) {
  // TODO: implement the validations described here:
  // https://www.rfc-editor.org/rfc/rfc7519.html#section-7.2

  auto parts = base::SplitStringPiece(jwt, ".", base::KEEP_WHITESPACE,
                                      base::SPLIT_WANT_ALL);

  if (parts.size() != 3 || parts[0].empty() || parts[1].empty() ||
      parts[2].empty()) {
    return std::nullopt;
  }

  auto header = Base64UrlDecode(parts[0]);
  if (!header) {
    return std::nullopt;
  }

  base::Value::List result;
  result.Append(*header);

  auto payload = Base64UrlDecode(parts[1]);
  if (!payload) {
    return std::nullopt;
  }

  result.Append(*payload);
  result.Append(parts[2]);

  return result;
}

bool Jwt::Sign(Signer signer) {
  std::string message = Base64UrlEncode(header.value()).value() + "." +
                        Base64UrlEncode(payload.value()).value();

  auto sig = std::move(signer).Run(message);
  if (!sig) {
    return false;
  }

  base::Base64UrlEncode(*sig, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &signature.value());

  return true;
}

SdJwt::SdJwt() = default;
SdJwt::~SdJwt() = default;
SdJwt::SdJwt(const SdJwt& other) = default;

std::string SdJwt::Serialize() const {
  std::string result;
  result += jwt.Serialize().value();

  result += "~";

  for (const JSONString& disclosure : disclosures) {
    result += Base64UrlEncode(disclosure.value()).value();
    result += "~";
  }

  return result;
}

std::string SdJwtKb::Serialize() const {
  std::string result;
  result += sd_jwt.Serialize();
  result += kb_jwt.Serialize().value();

  return result;
}

SdJwtKb::SdJwtKb() = default;
SdJwtKb::~SdJwtKb() = default;
SdJwtKb::SdJwtKb(const SdJwtKb& other) = default;

// static
std::optional<std::vector<JSONString>> SdJwt::Disclose(
    const std::vector<std::pair<std::string, JSONString>>& disclosures,
    const std::vector<std::string>& selector) {
  // Implements the selective disclosure:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-disclosing-to-a-verifier

  std::map<std::string, JSONString> disclosures_by_name;
  for (const std::pair<std::string, JSONString>& disclosure : disclosures) {
    disclosures_by_name[disclosure.first] = disclosure.second;
  }

  std::vector<JSONString> result;
  for (const std::string& name : selector) {
    if (disclosures_by_name.count(name)) {
      result.push_back(disclosures_by_name[name]);
    } else {
      return std::nullopt;
    }
  }

  return result;
}

// static
std::optional<SdJwtKb> SdJwtKb::Create(const SdJwt& presentation,
                                       const std::string& aud,
                                       const std::string& nonce,
                                       const base::Time& iat,
                                       Hasher hasher,
                                       Signer signer) {
  std::string serialization = presentation.Serialize();

  std::string hash;
  base::Base64UrlEncode(hasher.Run(serialization),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &hash);

  Header header;
  header.typ = "kb+jwt";
  header.alg = "ES256";

  Payload payload;
  payload.aud = aud;
  payload.nonce = nonce;
  payload.iat = iat;
  payload.sd_hash = Base64String(hash);

  Jwt kb_jwt;
  auto header_json = header.ToJson();
  if (!header_json) {
    return std::nullopt;
  }

  kb_jwt.header = *header_json;

  auto payload_json = payload.ToJson();
  if (!payload_json) {
    return std::nullopt;
  }

  kb_jwt.payload = *payload_json;
  bool success = kb_jwt.Sign(std::move(signer));

  if (!success) {
    return std::nullopt;
  }

  SdJwtKb sd_jwt_kb;
  sd_jwt_kb.sd_jwt = presentation;
  sd_jwt_kb.kb_jwt = kb_jwt;

  return sd_jwt_kb;
}

std::optional<SdJwtKb> SdJwtKb::Parse(const std::string_view& sdjwtkb) {
  SdJwtKb result;

  auto pair = base::RSplitStringOnce(sdjwtkb, "~");
  if (!pair) {
    return std::nullopt;
  }

  // The first part of the string is separate by "~", but then separator
  // is also part of the SD-JWT, so we concatenate it back.
  auto list = SdJwt::Parse(std::string(pair->first) + "~");

  if (!list) {
    // Poorly formed string.
    return std::nullopt;
  }

  auto sd_jwt = SdJwt::From(*list);
  if (!sd_jwt) {
    return std::nullopt;
  }

  result.sd_jwt = *sd_jwt;

  auto jwt = Jwt::Parse(pair->second);
  if (!jwt) {
    return std::nullopt;
  }

  auto kb = Jwt::From(*jwt);
  if (!kb) {
    return std::nullopt;
  }

  result.kb_jwt = *kb;

  return result;
}

}  // namespace content::sdjwt
