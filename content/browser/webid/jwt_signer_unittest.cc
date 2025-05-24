// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/jwt_signer.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/browser/webid/sd_jwt.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::NiceMock;

namespace content::sdjwt {

namespace {
std::vector<uint8_t> TestSha256(std::string_view data) {
  std::string str = crypto::SHA256HashString(data);
  std::vector<uint8_t> result(str.begin(), str.end());
  return result;
}
}  // namespace

class JwtSignerTest : public testing::Test {
 protected:
  JwtSignerTest() = default;
  ~JwtSignerTest() override = default;

  void SetUp() override {}

  void TearDown() override {}
};

void Verify(const std::vector<uint8_t>& public_key,
            const base::span<const uint8_t>& signature,
            const std::string& message) {
  const size_t kMaxBytesPerBN = 32;
  EXPECT_EQ(signature.size(), 2 * kMaxBytesPerBN);
  base::span<const uint8_t> r_bytes = signature.first(kMaxBytesPerBN);
  base::span<const uint8_t> s_bytes = signature.subspan(kMaxBytesPerBN);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  EXPECT_TRUE(ecdsa_sig);

  EXPECT_TRUE(BN_bin2bn(r_bytes.data(), r_bytes.size(), ecdsa_sig->r));
  EXPECT_TRUE(BN_bin2bn(s_bytes.data(), s_bytes.size(), ecdsa_sig->s));

  uint8_t* der;
  size_t der_len;
  EXPECT_TRUE(ECDSA_SIG_to_bytes(&der, &der_len, ecdsa_sig.get()));

  // Frees memory allocated by `ECDSA_SIG_to_bytes()`.
  bssl::UniquePtr<uint8_t> delete_signature(der);

  // SAFETY: `ECDSA_SIG_to_bytes()` uses a C-style API to allocate a new buffer.
  auto signature_span = UNSAFE_BUFFERS(base::span<uint8_t>(der, der_len));
  auto der_signature = base::ToVector(signature_span);

  crypto::SignatureVerifier verifier;
  EXPECT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                                  der_signature, public_key));

  verifier.VerifyUpdate(base::as_byte_span(message));
  EXPECT_TRUE(verifier.VerifyFinal());
}

TEST_F(JwtSignerTest, JwtSigner) {
  auto private_key = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> public_key;
  EXPECT_TRUE(private_key->ExportPublicKey(&public_key));

  const std::string message = "hello wold";
  auto signer = CreateJwtSigner(std::move(private_key));
  auto signature = std::move(signer).Run(message);

  Verify(public_key, base::as_byte_span(*signature), message);
}

TEST_F(JwtSignerTest, CreateJwt) {
  auto private_key = crypto::ECPrivateKey::Create();
  EXPECT_TRUE(private_key);

  std::vector<uint8_t> public_key;
  EXPECT_TRUE(private_key->ExportPublicKey(&public_key));

  Header header;
  header.typ = "jwt";
  header.alg = "ES256";

  Payload payload;
  payload.iss = "https://issuer.example";
  payload.sub = "goto@google.com";

  Jwt issued;
  issued.header = JSONString(header.Serialize()->value());
  issued.payload = JSONString(payload.Serialize()->value());

  auto success = issued.Sign(CreateJwtSigner(std::move(private_key)));

  EXPECT_TRUE(success);

  auto signature = base::Base64UrlDecode(
      issued.signature.value(), base::Base64UrlDecodePolicy::IGNORE_PADDING);

  EXPECT_TRUE(signature);

  std::string header_base64;
  base::Base64UrlEncode(issued.header.value(),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &header_base64);

  std::string payload_base64;
  base::Base64UrlEncode(issued.payload.value(),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &payload_base64);

  std::string message = header_base64 + "." + payload_base64;
  Verify(public_key, base::as_byte_span(*signature), message);
}

TEST_F(JwtSignerTest, CreateSdJwtKb) {
  auto holder_private_key = crypto::ECPrivateKey::Create();
  auto jwk = ExportPublicKey(*holder_private_key);

  auto issuer_private_key = crypto::ECPrivateKey::Create();

  Header header;
  header.typ = "jwt";
  header.alg = "ES256";

  Payload payload;
  payload.iss = "https://issuer.example";

  Disclosure name;
  name.salt = Disclosure::CreateSalt();
  name.name = "name";
  name.value = "Sam";

  payload._sd = {*name.Digest(base::BindRepeating(TestSha256))};

  ConfirmationKey confirmation;
  confirmation.jwk = *jwk;
  payload.cnf = confirmation;

  auto issuer_json = ExportPublicKey(*issuer_private_key);

  EXPECT_TRUE(issuer_json);
  EXPECT_TRUE(issuer_json->Serialize());

  Jwt issued;
  issued.header = JSONString(header.Serialize()->value());
  issued.payload = JSONString(payload.Serialize()->value());
  auto signer = CreateJwtSigner(std::move(issuer_private_key));
  auto success = issued.Sign(std::move(signer));

  EXPECT_TRUE(success);

  auto presentation = SdJwt::Disclose(
      {{name.name, JSONString(name.Serialize().value())}}, {"name"});
  EXPECT_TRUE(presentation);

  SdJwt sd_jwt;
  sd_jwt.jwt = issued;
  sd_jwt.disclosures = *presentation;

  std::optional<SdJwtKb> sd_jwt_kb = SdJwtKb::Create(
      sd_jwt, "https://verifier.example", "__fake_nonce__",
      base::Time::FromTimeT(1234), base::BindRepeating(TestSha256),
      CreateJwtSigner(std::move(holder_private_key)));

  EXPECT_TRUE(sd_jwt_kb);
}

}  // namespace content::sdjwt
