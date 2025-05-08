// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/sd_jwt.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "crypto/random.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::NiceMock;

namespace content::sdjwt {

class SdJwtTest : public testing::Test {
 protected:
  SdJwtTest() = default;
  ~SdJwtTest() override = default;

  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(SdJwtTest, JwkParsing) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#appendix-A.5
  std::string json =
      R"({
      "kty": "EC",
      "crv": "P-256",
      "x": "b28d4MwZMjw8-00CG4xfnn9SLMVMM19SlqZpVb_uNtQ",
      "y": "Xv5zWwuoaTgdS6hV43yI6gBwTnjukmFQQnJ_kCxzqk8"
    })";

  auto jwk = Jwk::From(*base::JSONReader::ReadDict(json));
  EXPECT_TRUE(jwk);

  EXPECT_EQ(jwk->kty, "EC");
  EXPECT_EQ(jwk->crv, "P-256");
  EXPECT_EQ(jwk->x, "b28d4MwZMjw8-00CG4xfnn9SLMVMM19SlqZpVb_uNtQ");
  EXPECT_EQ(jwk->y, "Xv5zWwuoaTgdS6hV43yI6gBwTnjukmFQQnJ_kCxzqk8");
}

TEST_F(SdJwtTest, JWkSerializing) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#appendix-A.5

  Jwk jwk;

  jwk.kty = "EC";
  jwk.crv = "P-256";
  jwk.x = "b28d4MwZMjw8-00CG4xfnn9SLMVMM19SlqZpVb_uNtQ";
  jwk.y = "Xv5zWwuoaTgdS6hV43yI6gBwTnjukmFQQnJ_kCxzqk8";

  std::string json =
      "{"
      "\"crv\":\"P-256\","
      "\"kty\":\"EC\","
      "\"x\":\"b28d4MwZMjw8-00CG4xfnn9SLMVMM19SlqZpVb_uNtQ\","
      "\"y\":\"Xv5zWwuoaTgdS6hV43yI6gBwTnjukmFQQnJ_kCxzqk8\""
      "}";
  EXPECT_STREQ(jwk.Serialize()->c_str(), json.c_str());
}

TEST_F(SdJwtTest, DisclosureParsing) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4.2.1
  std::string json = R"(["_26bc4LT-ac6q2KI6cBW5es", "family_name", "Möbius"])";

  auto disclosure = Disclosure::From(base::JSONReader::Read(json)->GetList());
  EXPECT_TRUE(disclosure);

  EXPECT_EQ(disclosure->salt, Base64String("_26bc4LT-ac6q2KI6cBW5es"));
  EXPECT_EQ(disclosure->name, "family_name");
  EXPECT_EQ(disclosure->value, "Möbius");
}

TEST_F(SdJwtTest, DisclosureSerialization) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4.2.1

  Disclosure disclosure;
  disclosure.salt = Base64String("_26bc4LT-ac6q2KI6cBW5es");
  disclosure.name = "family_name";
  disclosure.value = "Möbius";

  Base64String base64 = disclosure.Serialize();

  // This value is different from what's in the spec, but that's because
  // our JSON serialization strips whitespaces between array elements
  // which causes the base64 encoding to not match.
  // https://github.com/openid/OpenID4VP/issues/325
  //
  // This base64 value decodes to:
  //
  // (["_26bc4LT-ac6q2KI6cBW5es","family_name","Möbius"])
  std::string expected =
      "WyJfMjZiYzRMVC1hYzZxMktJNmNCVzVlcyIsImZhbWlseV9uYW1lIiwiTcO2Yml1cyJd";

  EXPECT_STREQ(base64->c_str(), expected.c_str());
}

TEST_F(SdJwtTest, JwtParsing) {
  // Jwt's top level structure is a header/payload/signature tuple
  // separate by ".".
  std::string jwt = "aGVhZGVy.cGF5bG9hZA.signature";

  auto token = Jwt::From(*Jwt::Parse(jwt));
  EXPECT_TRUE(token);

  EXPECT_EQ(token->header, JSONString("header"));
  EXPECT_EQ(token->payload, JSONString("payload"));
  EXPECT_EQ(token->signature, Base64String("signature"));
}

TEST_F(SdJwtTest, JwtParsingInvalid) {
  EXPECT_FALSE(Jwt::Parse(""));
  EXPECT_FALSE(Jwt::Parse("YQ"));
  EXPECT_FALSE(Jwt::Parse("."));
  EXPECT_FALSE(Jwt::Parse("YQ."));
  EXPECT_FALSE(Jwt::Parse(".YQ"));
  EXPECT_FALSE(Jwt::Parse("YWE."));
  EXPECT_FALSE(Jwt::Parse(".YWE"));

  EXPECT_FALSE(Jwt::Parse(".."));
  EXPECT_FALSE(Jwt::Parse("YQ.YWE."));
  EXPECT_FALSE(Jwt::Parse("YQ..YWE"));
  EXPECT_FALSE(Jwt::Parse(".YQ.YWE"));
}

TEST_F(SdJwtTest, JwtSerializing) {
  Jwt token;
  token.header = JSONString("header");
  token.payload = JSONString("payload");
  token.signature = Base64String("signature");

  // Jwt's top level structure:
  // Base64UrlEncode(header) . Base64UrlEncode(payload)  . signature
  EXPECT_EQ(token.Serialize(), JSONString("aGVhZGVy.cGF5bG9hZA.signature"));
}

TEST_F(SdJwtTest, HeaderParsingAndSerializing) {
  Header header;
  header.alg = "foo";
  header.typ = "bar";

  EXPECT_EQ(header.ToJson(), JSONString(R"({"alg":"foo","typ":"bar"})"));
  // The serializion of the header is a base64 encoding of the JSON.
  EXPECT_EQ(header.Serialize(),
            Base64String("eyJhbGciOiJmb28iLCJ0eXAiOiJiYXIifQ"));

  // Test that we can go back from base64 to value.
  auto parsed =
      Header::From(*base::JSONReader::ReadDict(R"({"alg":"foo","typ":"bar"})"));
  EXPECT_TRUE(parsed);
  EXPECT_EQ(parsed->alg, "foo");
  EXPECT_EQ(parsed->typ, "bar");
}

TEST_F(SdJwtTest, PayloadParsingAndSerializing) {
  Payload payload;
  payload.sub = "foo";

  EXPECT_EQ(payload.ToJson(), JSONString(R"({"sub":"foo"})"));
  // The serializion of the header is a base64 encoding of the JSON.
  EXPECT_EQ(payload.Serialize(), Base64String("eyJzdWIiOiJmb28ifQ"));

  // Test that we can go back from base64 to value.
  auto parsed = Payload::From(*base::JSONReader::ReadDict(R"({"sub":"foo"})"));
  EXPECT_TRUE(parsed);
  EXPECT_EQ(parsed->sub, "foo");
}

TEST_F(SdJwtTest, JwtParsingRFC) {
  // Example from:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-5.1
  std::string jwt =
      "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogImV4YW1wbGUrc2Qtand0In0.eyJfc2QiOiBb"
      "IkNyUWU3UzVrcUJBSHQtbk1ZWGdjNmJkdDJTSDVhVFkxc1VfTS1QZ2tqUEkiLCAiSnpZ"
      "akg0c3ZsaUgwUjNQeUVNZmVadTZKdDY5dTVxZWhabzdGN0VQWWxTRSIsICJQb3JGYnBL"
      "dVZ1Nnh5bUphZ3ZrRnNGWEFiUm9jMkpHbEFVQTJCQTRvN2NJIiwgIlRHZjRvTGJnd2Q1"
      "SlFhSHlLVlFaVTlVZEdFMHc1cnREc3JaemZVYW9tTG8iLCAiWFFfM2tQS3QxWHlYN0tB"
      "TmtxVlI2eVoyVmE1TnJQSXZQWWJ5TXZSS0JNTSIsICJYekZyendzY002R242Q0pEYzZ2"
      "Vks4QmtNbmZHOHZPU0tmcFBJWmRBZmRFIiwgImdiT3NJNEVkcTJ4Mkt3LXc1d1BFemFr"
      "b2I5aFYxY1JEMEFUTjNvUUw5Sk0iLCAianN1OXlWdWx3UVFsaEZsTV8zSmx6TWFTRnpn"
      "bGhRRzBEcGZheVF3TFVLNCJdLCAiaXNzIjogImh0dHBzOi8vaXNzdWVyLmV4YW1wbGUu"
      "Y29tIiwgImlhdCI6IDE2ODMwMDAwMDAsICJleHAiOiAxODgzMDAwMDAwLCAic3ViIjog"
      "InVzZXJfNDIiLCAibmF0aW9uYWxpdGllcyI6IFt7Ii4uLiI6ICJwRm5kamtaX1ZDem15"
      "VGE2VWpsWm8zZGgta284YUlLUWM5RGxHemhhVllvIn0sIHsiLi4uIjogIjdDZjZKa1B1"
      "ZHJ5M2xjYndIZ2VaOGtoQXYxVTFPU2xlclAwVmtCSnJXWjAifV0sICJfc2RfYWxnIjog"
      "InNoYS0yNTYiLCAiY25mIjogeyJqd2siOiB7Imt0eSI6ICJFQyIsICJjcnYiOiAiUC0y"
      "NTYiLCAieCI6ICJUQ0FFUjE5WnZ1M09IRjRqNFc0dmZTVm9ISVAxSUxpbERsczd2Q2VH"
      "ZW1jIiwgInkiOiAiWnhqaVdXYlpNUUdIVldLVlE0aGJTSWlyc1ZmdWVjQ0U2dDRqVDlG"
      "MkhaUSJ9fX0.oQ0UNJB1E1agYouB1yfGXfYLyWueHhfMFuicSV-n_GLXHtX0XK99sfDD"
      "ERiWKukCUzadGTT4QbCwXe6JvVmZWw";

  auto token = Jwt::From(*Jwt::Parse(jwt));
  EXPECT_TRUE(token);

  EXPECT_EQ(token->header,
            JSONString(R"({"alg": "ES256", "typ": "example+sd-jwt"})"));
  EXPECT_EQ(token->signature,
            Base64String("oQ0UNJB1E1agYouB1yfGXfYLyWueHhfMFuicSV-n_"
                         "GLXHtX0XK99sfDDERiWKukCUzadGTT4QbCwXe6JvVmZWw"));

  std::string expected =
      R"({)"
      R"("_sd": [)"
      R"("CrQe7S5kqBAHt-nMYXgc6bdt2SH5aTY1sU_M-PgkjPI", )"
      R"("JzYjH4svliH0R3PyEMfeZu6Jt69u5qehZo7F7EPYlSE", )"
      R"("PorFbpKuVu6xymJagvkFsFXAbRoc2JGlAUA2BA4o7cI", )"
      R"("TGf4oLbgwd5JQaHyKVQZU9UdGE0w5rtDsrZzfUaomLo", )"
      R"("XQ_3kPKt1XyX7KANkqVR6yZ2Va5NrPIvPYbyMvRKBMM", )"
      R"("XzFrzwscM6Gn6CJDc6vVK8BkMnfG8vOSKfpPIZdAfdE", )"
      R"("gbOsI4Edq2x2Kw-w5wPEzakob9hV1cRD0ATN3oQL9JM", )"
      R"("jsu9yVulwQQlhFlM_3JlzMaSFzglhQG0DpfayQwLUK4")"
      R"(], )"
      R"("iss": "https://issuer.example.com", )"
      R"("iat": 1683000000, )"
      R"("exp": 1883000000, )"
      R"("sub": "user_42", )"
      R"("nationalities": [)"
      R"({"...": "pFndjkZ_VCzmyTa6UjlZo3dh-ko8aIKQc9DlGzhaVYo"}, )"
      R"({"...": "7Cf6JkPudry3lcbwHgeZ8khAv1U1OSlerP0VkBJrWZ0"}], )"
      R"("_sd_alg": "sha-256", )"
      R"("cnf": {"jwk": {"kty": "EC", "crv": "P-256", )"
      R"("x": "TCAER19Zvu3OHF4j4W4vfSVoHIP1ILilDls7vCeGemc", )"
      R"("y": "ZxjiWWbZMQGHVWKVQ4hbSIirsVfuecCE6t4jT9F2HZQ"}})"
      R"(})";

  EXPECT_STREQ(token->payload.value().c_str(), expected.c_str());

  auto header =
      Header::From(*base::JSONReader::ReadDict(token->header.value()));
  EXPECT_TRUE(header);

  EXPECT_EQ(header->typ, "example+sd-jwt");
  EXPECT_EQ(header->alg, "ES256");

  auto payload =
      Payload::From(*base::JSONReader::ReadDict(token->payload.value()));
  EXPECT_TRUE(payload);

  EXPECT_EQ(payload->iss, "https://issuer.example.com");
  EXPECT_EQ(payload->sub, "user_42");
  EXPECT_EQ(payload->iat, base::Time::FromTimeT(1683000000));
  EXPECT_EQ(payload->_sd_alg, "sha-256");

  EXPECT_TRUE(payload->cnf);
  EXPECT_EQ(payload->cnf->jwk.kty, "EC");
  EXPECT_EQ(payload->cnf->jwk.crv, "P-256");
  EXPECT_EQ(payload->cnf->jwk.x, "TCAER19Zvu3OHF4j4W4vfSVoHIP1ILilDls7vCeGemc");
  EXPECT_EQ(payload->cnf->jwk.y, "ZxjiWWbZMQGHVWKVQ4hbSIirsVfuecCE6t4jT9F2HZQ");
}

TEST_F(SdJwtTest, SdJwtParsingAndSerializing) {
  // SdJwt's top level structure is a jwt followed by ~ disclosures.
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4
  std::string jwt =
      "aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~";

  auto token = SdJwt::From(*SdJwt::Parse(jwt));
  EXPECT_TRUE(token);

  EXPECT_EQ(token->jwt.header, JSONString("header"));
  EXPECT_EQ(token->jwt.payload, JSONString("payload"));
  EXPECT_EQ(token->jwt.signature, Base64String("signature"));
  EXPECT_EQ(token->disclosures.size(), 2ul);
  EXPECT_EQ(token->disclosures[0], JSONString("disclosure1"));
  EXPECT_EQ(token->disclosures[1], JSONString("disclosure2"));

  // Asserts that we can serialize the token again.
  token->jwt.header = JSONString("new-header");
  EXPECT_EQ(
      token->Serialize(),
      "bmV3LWhlYWRlcg.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~");
}

TEST_F(SdJwtTest, SdJwtParsingCornerCases) {
  // Valid: no disclosures
  EXPECT_TRUE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~"));

  // Invalid JWT
  EXPECT_FALSE(SdJwt::Parse("~"));
  EXPECT_FALSE(SdJwt::Parse(".~"));
  EXPECT_FALSE(SdJwt::Parse("..~"));
  EXPECT_FALSE(SdJwt::Parse(".cGF5bG9hZA.signature~"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy..signature~"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.~"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy..signature~"));
  EXPECT_FALSE(SdJwt::Parse(".cGF5bG9hZA.signature~"));

  // Invalid: needs trailing ~
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE"));
  EXPECT_FALSE(SdJwt::Parse(
      "aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI"));
  EXPECT_FALSE(
      SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~"
                   "ZGlzY2xvc3VyZTI~ZGlzY2xvc3VyZTM"));

  // Invalid: disclosure can't be empty
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~~"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~~"));
  EXPECT_FALSE(SdJwt::Parse(
      "aGVhZGVy.cGF5bG9hZA.signature~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~~"));
  EXPECT_FALSE(SdJwt::Parse("aGVhZGVy.cGF5bG9hZA.signature~~~"));
}

TEST_F(SdJwtTest, SelectiveDisclosure) {
  Disclosure name;
  name.salt = Base64String("fake-salt1");
  name.name = "name";
  name.value = "Sam";

  Disclosure email;
  email.salt = Base64String("fake-salt2");
  email.name = "email";
  email.value = "goto@email.com";

  // Only present "name", keep "email" private.
  auto presentation = SdJwt::Disclose(
      {
          {email.name, *email.ToJson()},
          {name.name, *name.ToJson()},
      },
      {"name"});

  EXPECT_TRUE(presentation);

  // Asserts that we got only 1 disclosure ...
  EXPECT_EQ(presentation->size(), 1ul);

  // ... and that it was selected correctly:
  EXPECT_EQ((*presentation)[0],
            JSONString("[\"fake-salt1\",\"name\",\"Sam\"]"));
}

TEST_F(SdJwtTest, SdJwtKbParsingAndSerializing) {
  // SdJwtKb's top level structure is a jwt followed by ~ disclosures
  // followed by a key binding JWT.
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#name-sd-jwt-and-sd-jwtkb-data-fo
  std::string bin =
      "aGVhZGVy.cGF5bG9hZA.iss_signature"
      "~ZGlzY2xvc3VyZTE"
      "~ZGlzY2xvc3VyZTI"
      "~"
      "aGVhZGVy.cGF5bG9hZA.kb_signature";

  auto token = SdJwtKb::Parse(bin);
  EXPECT_TRUE(token);

  EXPECT_EQ(token->sd_jwt.jwt.header, JSONString("header"));
  EXPECT_EQ(token->sd_jwt.jwt.payload, JSONString("payload"));
  EXPECT_EQ(token->sd_jwt.jwt.signature, Base64String("iss_signature"));
  EXPECT_EQ(token->sd_jwt.disclosures.size(), 2ul);
  EXPECT_EQ(token->sd_jwt.disclosures[0], JSONString("disclosure1"));
  EXPECT_EQ(token->sd_jwt.disclosures[1], JSONString("disclosure2"));
  EXPECT_EQ(token->kb_jwt.header, JSONString("header"));
  EXPECT_EQ(token->kb_jwt.payload, JSONString("payload"));
  EXPECT_EQ(token->kb_jwt.signature, Base64String("kb_signature"));

  // Asserts that we can serialize the token again.
  token->sd_jwt.jwt.header = JSONString("new-header");
  EXPECT_EQ(
      token->Serialize(),
      "bmV3LWhlYWRlcg.cGF5bG9hZA.iss_signature~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~"
      "aGVhZGVy.cGF5bG9hZA.kb_signature");
}

TEST_F(SdJwtTest, SdJwtKbParsingCornerCases) {
  EXPECT_TRUE(
      SdJwtKb::Parse("aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~"
                     "aGVhZGVy.cGF5bG9hZA.kbs"));

  // No KB JWT.
  EXPECT_FALSE(SdJwtKb::Parse(
      "aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~"));

  // Ends in ~
  EXPECT_FALSE(
      SdJwtKb::Parse("aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~"
                     "aGVhZGVy.cGF5bG9hZA.kbs~"));

  // KB JWT missing payload and signature
  EXPECT_FALSE(
      SdJwtKb::Parse("aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~."
                     "cGF5bG9hZA.kbs"));
  // KB JWT with empty header and payload
  EXPECT_FALSE(SdJwtKb::Parse(
      "aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~aGVhZGVy..kbs"));
  // KB JWT with empty signature
  EXPECT_FALSE(
      SdJwtKb::Parse("aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~"
                     "aGVhZGVy.cGF5bG9hZA."));
  // KB JWT with empty header, payload and signature
  EXPECT_FALSE(SdJwtKb::Parse(
      "aGVhZGVy.cGF5bG9hZA.is~ZGlzY2xvc3VyZTE~ZGlzY2xvc3VyZTI~.."));
}

std::optional<std::vector<std::uint8_t>> TestSigner(
    const std::string_view& message) {
  std::string str = "Signed(" + std::string(message) + ")";
  std::vector<uint8_t> result(str.begin(), str.end());
  return result;
}

std::vector<std::uint8_t> TestSha256(std::string_view data) {
  std::string str = "Sha256(" + std::string(data) + ")";
  std::vector<uint8_t> result(str.begin(), str.end());
  return result;
}

TEST_F(SdJwtTest, SdJwtKb_Bind) {
  Disclosure name;
  name.salt = Base64String("fake-salt1");
  name.name = "name";
  name.value = "Sam";

  Disclosure email;
  email.salt = Base64String("fake-salt2");
  email.name = "email";
  email.value = "goto@email.com";

  Jwk key;

  auto disclosures = SdJwt::Disclose(
      {
          {name.name, *name.ToJson()},
          {email.name, *email.ToJson()},
      },
      {"name"});

  EXPECT_TRUE(disclosures);

  Jwt issued;
  issued.header = JSONString("header");
  issued.payload = JSONString("payload");
  issued.signature = Base64String("signature");

  SdJwt presentation;
  presentation.jwt = issued;
  presentation.disclosures = *disclosures;

  auto sdjwtkb = SdJwtKb::Create(presentation, "https://verifier.example",
                                 "__fake_nonce__", base::Time::FromTimeT(1234),
                                 base::BindRepeating(TestSha256),
                                 base::BindRepeating(TestSigner));

  EXPECT_TRUE(sdjwtkb);

  auto kb = sdjwtkb->kb_jwt;

  // Checks KB headers:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4.3
  auto header = Header::From(*base::JSONReader::ReadDict(kb.header.value()));
  EXPECT_TRUE(header);
  // typ MUST be "kb+jwt".
  EXPECT_EQ(header->typ, "kb+jwt");
  EXPECT_EQ(header->alg, "ES256");

  auto payload = Payload::From(*base::JSONReader::ReadDict(kb.payload.value()));
  EXPECT_TRUE(payload);
  // aud is required.
  EXPECT_EQ(payload->aud, "https://verifier.example");
  // nonce is required.
  EXPECT_EQ(payload->nonce, "__fake_nonce__");
  // iat is required.
  EXPECT_EQ(payload->iat, base::Time::FromTimeT(1234));

  // sd_hash is required.

  // Checks for how the hash was constructed:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4.3.1
  EXPECT_EQ(
      payload->sd_hash,
      Base64String(
          "U2hhMjU2KGFHVmhaR1Z5LmNHRjViRzloWkEuc2lnbmF0dXJlfld5Sm1ZV3RsTFh"
          "OaGJIUXhJaXdpYm1GdFpTSXNJbE5oYlNKZH4p"));

  // Checks that the signature was constructed correctly too:
  // https://www.ietf.org/archive/id/draft-ietf-oauth-selective-disclosure-jwt-13.html#section-4.3.1
  //
  // This value is a base64 encode string of the following header:
  //
  //  {
  //    "alg": "ES256",
  //    "typ": "kb+jwt"
  //  }
  //
  // and payload:
  //
  //  {
  //    "aud": "https://verifier.example",
  //    "iat": 1234,
  //    "nonce": "__fake_nonce__",
  //    "sd_hash":
  //      "Sha256(Base64(header).Base64(payload).signature~
  //           Base64(["fake-salt1","name","Sam"])~)"
  //  }
  //
  std::string base64;
  base::Base64UrlEncode(
      "Signed(eyJhbGciOiJFUzI1NiIsInR5cCI6ImtiK2p3dCJ9."
      "eyJhdWQiOiJodHRwczovL3ZlcmlmaWVyLmV4YW1wbGUiLCJpYXQiOjEyMzQsIm5"
      "vbmNlIjoiX19mYWtlX25vbmNlX18iLCJzZF9oYXNoIjoiVTJoaE1qVTJLR0ZIVm"
      "1oYVIxWjVMbU5IUmpWaVJ6bG9Xa0V1YzJsbmJtRjBkWEpsZmxkNVNtMVpWM1JzV"
      "EZoT2FHSklVWGhKYVhkcFltMUdkRnBUU1hOSmJFNW9ZbE5LWkg0cCJ9)",
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64);

  EXPECT_STREQ(kb.signature->c_str(), base64.c_str());
}

}  // namespace content::sdjwt
