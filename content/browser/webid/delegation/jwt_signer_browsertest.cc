// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/jwt_signer.h"

#include "base/functional/callback.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::sdjwt {

namespace {

std::vector<std::uint8_t> Sha256(std::string_view data) {
  std::string str = crypto::SHA256HashString(data);
  std::vector<uint8_t> result(str.begin(), str.end());
  return result;
}

}  // namespace

class JwtSignerBrowserTest : public ContentBrowserTest {
 protected:
  JwtSignerBrowserTest() = default;
  ~JwtSignerBrowserTest() override = default;
};

std::pair<SdJwtKb, Jwk> CreateTestSdJwtKb(const std::string aud,
                                          const std::string nonce,
                                          int iat) {
  auto holder_private_key = crypto::keypair::PrivateKey::GenerateEcP256();
  auto jwk = ExportPublicKey(holder_private_key);

  auto issuer_private_key = crypto::keypair::PrivateKey::GenerateEcP256();

  Header header;
  header.typ = "jwt";
  header.alg = "ES256";

  Payload payload;
  payload.iss = "https://issuer.example";

  Disclosure name;
  name.salt = Disclosure::CreateSalt();
  name.name = "name";
  name.value = "Sam";

  payload._sd = {*name.Digest(base::BindRepeating(Sha256))};

  ConfirmationKey confirmation;
  confirmation.jwk = *jwk;
  payload.cnf = confirmation;

  Jwt issued;
  issued.header = *header.ToJson();
  issued.payload = *payload.ToJson();

  auto issuer_jwk = ExportPublicKey(issuer_private_key);

  issued.Sign(CreateJwtSigner(std::move(issuer_private_key)));

  auto disclosures = SdJwt::Disclose({{name.name, *name.ToJson()}}, {"name"});
  EXPECT_TRUE(disclosures);

  SdJwt presentation;
  presentation.jwt = issued;
  presentation.disclosures = *disclosures;

  std::optional<SdJwtKb> sd_jwt_kb =
      SdJwtKb::Create(presentation, aud, nonce, base::Time::FromTimeT(iat),
                      base::BindRepeating(Sha256),
                      CreateJwtSigner(std::move(holder_private_key)));

  return std::make_pair(*sd_jwt_kb, *issuer_jwk);
}

IN_PROC_BROWSER_TEST_F(JwtSignerBrowserTest, VerifyWithWebCrypto) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "fedcm/sd_jwt.html")));

  // Generate a test SD-JWT+KB presentation.
  auto token =
      CreateTestSdJwtKb("https://verifier.example", "__fake_nonce__", 1234);
  const std::string sdjwtkb = token.first.Serialize();
  const std::string key = *token.second.Serialize();

  // Load the token into a string
  ASSERT_TRUE(ExecJs(shell(), "var token = '" + sdjwtkb + "';"));

  // Load the key into an object
  ASSERT_TRUE(ExecJs(shell(), "var key = " + key + ";"));

  std::string verify = R"(
    main(
      token,
      key,
      'https://verifier.example',
      '__fake_nonce__'
    )
  )";

  // Verify the SD-JWT+KB.
  EXPECT_THAT(EvalJs(shell(), verify).TakeValue().TakeList(),
              testing::UnorderedElementsAre("Sam"));
}

}  // namespace content::sdjwt
