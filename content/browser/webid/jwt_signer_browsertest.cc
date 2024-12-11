// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/jwt_signer.h"

#include "base/functional/callback.h"
#include "content/browser/webid/sd_jwt.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "crypto/ec_private_key.h"
#include "crypto/sha2.h"
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

  payload._sd = {*name.Digest(base::BindRepeating(Sha256))};

  ConfirmationKey confirmation;
  confirmation.jwk = *jwk;
  payload.cnf = confirmation;

  Jwt issued;
  issued.header = *header.ToJson();
  issued.payload = *payload.ToJson();

  auto issuer_jwk = ExportPublicKey(*issuer_private_key);

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

const std::string verifier = R"(
    // Tests that the BoringSSL implementation of the
    // JwtSigner interoperates with a WebCrypto verifier.

    async function check(token, key) {
      console.log("checking sdjwtkb");

      // Can we parse the SD-JWT+KB?
      const [issued, disclosures, kb] = parse(token);

      const signed = await verify(issued, key);

      // Does the signature of the issued JWT match?
      if (!signed) {
        console.log("signature doesn't match");
        return false;
      }

      console.log("signatures match");

      const payload = JSON.parse(base64decode(issued.payload));

      console.log("verifying. bound?");
      const bound = await verify(kb, payload.cnf.jwk);

      console.log(bound);

      // Does the signature of the key binding JWT match?
      if (!bound) {
        return false;
      }

      const binding = JSON.parse(base64decode(kb.payload));

      console.log("aud checks out?");

      // Was the presentation intended for me?
      if (binding.aud != "https://verifier.example") {
        return false;
      }

      console.log("nonce checks out?");

      // Was my challenge signed?
      if (binding.nonce != "__fake_nonce__") {
        return false;
      }

      console.log("issued recently?");

      // Was this issued recently?
      if (!binding.iat) {
        return false;
      }

      console.log("lets look at disclosures!");

      for (const disclosure of disclosures) {
        console.log("Parsaing a disclosure");
        console.log(disclosure);
        const serialization = base64UrlEncode(JSON.stringify(disclosure));
        const digest = await sha256(serialization);

        // Was the disclosure included in the digest?
        if (!payload._sd.includes(digest)) {
          return false;
        }
      }

      console.log("done!");

      // Ok, everything checks out.
      return true;
    }

    function main(token, key) {
      return check(token, key);
    }

    function jwt(str) {
      const header = str.substring(0, str.indexOf("."));
      str = str.substring(header.length + 1);

      const payload = str.substring(0, str.indexOf("."));
      str = str.substring(payload.length + 1);

      return {
        header: header,
        payload: payload,
        signature: str
      }
    }

    function parse(token) {
      let str = token;

      const first = str.substring(0, str.indexOf("~"));
      str = str.substring(first.length + 1);

      const issued = jwt(first);

      const disclosures = [];
      while (str.indexOf("~") > 0) {
        const disclosure = str.substring(0, str.indexOf("~"));
        str = str.substring(disclosure.length + 1);
        console.log("Parsing disclosure " + disclosure);
        disclosures.push(JSON.parse(base64decode(disclosure)));
      }

      const kb = jwt(str);

      return [issued, disclosures, kb];
    }

    async function verify(jwt, jwk) {
      const {header, payload, signature} = jwt;
      const bufSignature = base64ToArrayBuffer(stripurlencoding(signature));

      const data = header + "." + payload;
      const bufData = textToArrayBuffer(data);

      const algo = {
        name: "ECDSA",
        namedCurve: "P-256", // secp256r1
      };
      const hash = {name: "SHA-256"};
      const signAlgo = {...algo, hash};

      const key = await crypto.subtle.importKey("jwk", jwk, {
        name: "ECDSA",
        namedCurve: "P-256",
      }, true, ["verify"]);

      return await crypto.subtle.verify(
         signAlgo, key, bufSignature, bufData);
    }

    function stripurlencoding(b64) {
      return b64.replace(/_/g, '/').replace(/-/g, '+');
    }

    function base64ToArrayBuffer(b64) {
      var byteString = atob(b64);
      var byteArray = new Uint8Array(byteString.length);
      for (var i = 0; i < byteString.length; i++) {
        byteArray[i] = byteString.charCodeAt(i);
      }
      return byteArray.buffer;
    }

    function textToArrayBuffer(str) {
      var buf = unescape(encodeURIComponent(str)) // 2 bytes for each char
      var bufView = new Uint8Array(buf.length)
      for (var i=0; i < buf.length; i++) {
        bufView[i] = buf.charCodeAt(i)
      }
      return bufView
    }

    function base64decode(base64) {
      return atob(base64.replace(/_/g, '/').replace(/-/g, '+'));
    }

    function urlEncode(str) {
      return str.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
    }

    function base64UrlEncode(str) {
      const base64Encoded = btoa(str);
      return urlEncode(base64Encoded);
    }

    async function sha256(message) {
      const encoder = new TextEncoder();
      const data = encoder.encode(message);
      const hash = await window.crypto.subtle.digest("SHA-256", data);
      const hashArray = Array.from(new Uint8Array(hash));
      return base64UrlEncode(String.fromCharCode(...hashArray));
    }
  )";

IN_PROC_BROWSER_TEST_F(JwtSignerBrowserTest, VerifyWithWebCrypto) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));

  // Load the verifier.
  EXPECT_EQ(nullptr, EvalJs(shell(), verifier));

  // Generate a test SD-JWT+KB presentation.
  auto token =
      CreateTestSdJwtKb("https://verifier.example", "__fake_nonce__", 1234);
  const std::string sdjwtkb = token.first.Serialize();
  const std::string key = *token.second.Serialize();

  // Verify the SD-JWT+KB.
  EXPECT_EQ(true, EvalJs(shell(), "main('" + sdjwtkb + "', " + key + ")"));
}

}  // namespace content::sdjwt
