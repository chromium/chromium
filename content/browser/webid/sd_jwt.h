// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_SD_JWT_H_
#define CONTENT_BROWSER_WEBID_SD_JWT_H_

#include <optional>
#include <vector>

#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "content/common/content_export.h"

/**
  This a SD-JWT library for holders.

  It has the necessary data structures and algorithms for:

  - Key material preparation
  - Parsing
  - Selective disclosure
  - Key binding

  Jwks represent a public or private key as described in RFC7517.
  This specific implementation focus on elliptic curves at the
  moment, but could be extended to support other key types.

  A public/private key generator can use a Jwk as an output and
  later serialize it to JSON:

  Jwk key = GenerateKeyPair();
  auto json = key.Serialize();

  The serialized json can be passed to a issuer to bind it to
  a JWT.

  Jwts represent a JSON Web Token as described in RFC7519.
  This specific implementation focus on specific fields that are
  useful for SD-JWT presentation, such as cnf, _sd, _sd_alg and
  sd_hash.

  Because JSON parsing isn't reversible with serialization
  (i.e. JSON.serialize(JSON.parse("{foo: true}")) == "{foo:true}"),
  when Jwts are parsed from their encoding, the original JSON
  encoding of the header and the payload are kept, so that they
  can (a) be interpreted but also (b) be reverted back to the Jwt's
  original value. Reverting back to the original value is
  important because they are signed and need to be verified.

  So, a Jwt gets parsed into a JSON header and JSON payload,
  which represent a string containing JSON, and a Base64
  signature, which represents a string containing a base64url
  encoded blob.

  Jwt jwt = Jwt::From(Jwt::Parse(encoding))

  You can get the encoded Jwt back by calling Jwt.Serialize().

  To interpret the contents of the Jwt header and payload,
  you can use a JSON parser to open them.

  Header header = Header::From(
      *base::JSONReader::ReadDict(jwt.header))
  Payload payload = Payload::From(
      *base::JSONReader::ReadDict(jwt.payload))

  Accordingly, an SdJwt represents an SD-JWT in a similar way.

  An SdJwt gets parsed into the issued Jwt as well as into the list
  of selective disclosures. Like a Jwt, the selective disclosures
  are represented as JSON so that they can be serialized back to
  their original values.

  SdJwt sd_jwt = SdJwt::From(SdJwt::Parse(encoding))

  An SdJwt can be used to select which disclosures to disclose:

  auto selection = SdJwt::Disclose(disclosures, {"name", "email"})

  From the disclosures, the SdJwt, a desired audience and a nonce,
  we can finally create a SD-JWT+KB:

  auto sd_jwt_kb = SdJwtKb::Create(...);

  Finally, the holder can use sd_jwt_kb.Serialize() as a SD-JWT+KB
  presentation.

*/
namespace content::sdjwt {

// An Elliptic Curve Jwk representation.
// https://datatracker.ietf.org/doc/html/rfc7517#section-3
// Could be extended in the future to cover other Jwk key types.
struct CONTENT_EXPORT Jwk {
  std::string kty;
  std::string crv;
  std::string x;
  std::string y;
  std::string d;

  Jwk();
  ~Jwk();
  Jwk(const Jwk& other);

  static std::optional<Jwk> From(const base::Value::Dict& json);

  std::optional<std::string> Serialize() const;
};

// A string that can be parsed as application/json.
using JSONString = base::StrongAlias<class JSONStringTag, std::string>;
// A string that is base64url encoded.
using Base64String = base::StrongAlias<class Base64StringTag, std::string>;

// https://datatracker.ietf.org/doc/html/rfc7519#section-5
struct CONTENT_EXPORT Header {
  // https://datatracker.ietf.org/doc/html/rfc7519#section-5.1
  std::string typ;
  std::string alg;

  Header();
  ~Header();
  Header(const Header& other);

  static std::optional<Header> From(const base::Value::Dict& json);

  std::optional<JSONString> ToJson() const;
  std::optional<Base64String> Serialize() const;
};

// This struct holds the JWK in the "cnf" [1] parameter in the
// JWT that the issuer signs that bindings it to the Holder's
// public key [2, 3].
//
// [1] https://datatracker.ietf.org/doc/html/rfc7800#section-3.1
// [2]
// https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-optional-key-binding
// [3]
// https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#key_binding
struct CONTENT_EXPORT ConfirmationKey {
  Jwk jwk;

  ConfirmationKey();
  ~ConfirmationKey();
  ConfirmationKey(const ConfirmationKey& other);
};

// https://datatracker.ietf.org/doc/html/rfc7519#section-4
struct CONTENT_EXPORT Payload {
  // https://datatracker.ietf.org/doc/html/rfc7519#section-4.1.1
  std::string iss;
  // https://datatracker.ietf.org/doc/html/rfc7519#section-4.1.3
  std::string aud;
  // https://datatracker.ietf.org/doc/html/rfc7519#section-4.1.2
  std::string sub;
  // // https://datatracker.ietf.org/doc/html/rfc7800#section-3.1
  std::optional<ConfirmationKey> cnf;
  // https://datatracker.ietf.org/doc/html/rfc7519#section-4.1.6
  std::optional<base::Time> iat;
  // https://datatracker.ietf.org/doc/html/rfc7519#section-4.1.4
  std::optional<base::Time> exp;

  std::string nonce;

  // https://datatracker.ietf.org/doc/html/draft-ietf-oauth-sd-jwt-vc-04#section-3.2.2.1.1
  std::string vct;

  // Used in the Issued JWT
  std::vector<Base64String> _sd;
  std::string _sd_alg;

  // Used in the Key Binding JWT
  Base64String sd_hash;

  Payload();
  ~Payload();
  Payload(const Payload& other);

  static std::optional<Payload> From(const base::Value::Dict& json);

  std::optional<JSONString> ToJson() const;
  std::optional<Base64String> Serialize() const;
};

/**
 * A Signer takes a string and returns a binary signature.
 * It is introduced so that tests can be written without an actual
 * implementation of the crytographic signing algorithm.
 */
typedef base::OnceCallback<std::optional<std::vector<uint8_t>>(
    const std::string_view&)>
    Signer;

// https://datatracker.ietf.org/doc/html/rfc7519
struct CONTENT_EXPORT Jwt {
  JSONString header;
  JSONString payload;
  Base64String signature;

  Jwt();
  ~Jwt();
  Jwt(const Jwt& other);

  bool Sign(Signer signer);

  static std::optional<Jwt> From(const base::Value::List& json);
  static std::optional<base::Value::List> Parse(const std::string_view& jwt);
  JSONString Serialize() const;
};

/**
 * A Hasher takes a string and computes a SHA256 hash of the string.
 * It is introduced so that this library can be tested without an
 * actual implementation of hashing.
 */
typedef base::RepeatingCallback<std::vector<std::uint8_t>(std::string_view)>
    Hasher;

/**
 * Disclosure represents an SD-JWT disclosure.
 * https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-disclosures
 *
 * You can parse from JSON to interpret it with:
 *
 * auto disclosure = Disclosure::From(
 *     base::JSONReader::Read(json)->GetList());
 *
 * CreateSalt() and Digest() are exposed so that tests
 * can create disclosures as issuers.
 */
struct CONTENT_EXPORT Disclosure {
  Base64String salt;
  std::string name;
  std::string value;

  Disclosure();
  ~Disclosure();
  Disclosure(const Disclosure& other);

  static std::optional<Disclosure> From(const base::Value::List& list);

  // Creates a random value with the following requirements:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-entropy-of-the-salt
  static Base64String CreateSalt();

  Base64String Serialize() const;
  std::optional<JSONString> ToJson() const;
  std::optional<Base64String> Digest(Hasher hasher) const;
};

// https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html
struct CONTENT_EXPORT SdJwt {
  Jwt jwt;
  std::vector<JSONString> disclosures;

  SdJwt();
  ~SdJwt();
  SdJwt(const SdJwt& other);

  static std::optional<SdJwt> From(const base::Value::List& json);
  static std::optional<base::Value::List> Parse(const std::string_view& sdjwt);

  static std::optional<std::vector<JSONString>> Disclose(
      const std::vector<std::pair<std::string, JSONString>>& disclosures,
      const std::vector<std::string>& selector);

  std::string Serialize() const;
};

// https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-sd-jwt-and-sd-jwtkb-data-fo
struct CONTENT_EXPORT SdJwtKb {
  SdJwt sd_jwt;
  Jwt kb_jwt;

  SdJwtKb();
  ~SdJwtKb();
  SdJwtKb(const SdJwtKb& other);

  static std::optional<SdJwtKb> Parse(const std::string_view& sdjwtkb);

  static std::optional<SdJwtKb> Create(const SdJwt& presentation,
                                       const std::string& aud,
                                       const std::string& nonce,
                                       const base::Time& iat,
                                       Hasher hasher,
                                       Signer signer);

  std::string Serialize() const;
};

}  // namespace content::sdjwt

#endif  // CONTENT_BROWSER_WEBID_SD_JWT_H_
