// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an RSA-OAEP algorithm
blink::WebCryptoAlgorithm CreateRsaOaepAlgorithm(
    const std::vector<uint8_t>& label) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdRsaOaep,
      new blink::WebCryptoRsaOaepParams(!label.empty(), label));
}

std::string Base64EncodeUrlSafe(const std::vector<uint8_t>& input) {
  // The JSON web signature spec says that padding is omitted.
  // https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
  std::string base64url_encoded;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(input.data()),
                       input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64url_encoded);
  return base64url_encoded;
}

base::Value::Dict CreatePublicKeyJwkDict(
    base::flat_map<std::string, std::string> extra_properties) {
  base::Value::Dict jwk;
  jwk.Set("kty", "RSA");
  jwk.Set("n", Base64EncodeUrlSafe(HexStringToBytes(kPublicKeyModulusHex)));
  jwk.Set("e", Base64EncodeUrlSafe(HexStringToBytes(kPublicKeyExponentHex)));
  for (const auto& prop : extra_properties) {
    jwk.Set(prop.first, prop.second);
  }
  return jwk;
}

class WebCryptoRsaOaepTest : public WebCryptoTestBase {};

// Import a PKCS#8 private key that uses RSAPrivateKey with the
// id-rsaEncryption OID.
TEST_F(WebCryptoRsaOaepTest, ImportPkcs8WithRsaEncryption) {
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaOaep,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageDecrypt, &private_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithNoAlg) {
  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          CreatePublicKeyJwkDict({}),
          CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep,
                                         blink::kWebCryptoAlgorithmIdSha1),
          true, blink::kWebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMatchingAlg) {
  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          CreatePublicKeyJwkDict({{"alg", "RSA-OAEP"}}),
          CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep,
                                         blink::kWebCryptoAlgorithmIdSha1),
          true, blink::kWebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMismatchedAlgFails) {
  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::ErrorJwkAlgorithmInconsistent(),
      ImportKeyJwkFromDict(
          CreatePublicKeyJwkDict({{"alg", "RSA-OAEP-512"}}),
          CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep,
                                         blink::kWebCryptoAlgorithmIdSha1),
          true, blink::kWebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ImportPublicJwkWithMismatchedTypeFails) {
  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::ErrorJwkUnexpectedKty("RSA"),
      ImportKeyJwkFromDict(
          CreatePublicKeyJwkDict({{"kty", "oct"}, {"alg", "RSA-OAEP"}}),
          CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep,
                                         blink::kWebCryptoAlgorithmIdSha1),
          true, blink::kWebCryptoKeyUsageEncrypt, &public_key));
}

TEST_F(WebCryptoRsaOaepTest, ExportPublicJwk) {
  struct TestData {
    blink::WebCryptoAlgorithmId hash_alg;
    const char* expected_jwk_alg;
  } kTestData[] = {{blink::kWebCryptoAlgorithmIdSha1, "RSA-OAEP"},
                   {blink::kWebCryptoAlgorithmIdSha256, "RSA-OAEP-256"},
                   {blink::kWebCryptoAlgorithmIdSha384, "RSA-OAEP-384"},
                   {blink::kWebCryptoAlgorithmIdSha512, "RSA-OAEP-512"}};
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.expected_jwk_alg);

    // Import the key in a known-good format
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKeyJwkFromDict(
                  CreatePublicKeyJwkDict({{"alg", test_data.expected_jwk_alg}}),
                  CreateRsaHashedImportAlgorithm(
                      blink::kWebCryptoAlgorithmIdRsaOaep, test_data.hash_alg),
                  true, blink::kWebCryptoKeyUsageEncrypt, &public_key));

    // Now export the key as JWK and verify its contents
    std::vector<uint8_t> jwk_data;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &jwk_data));
    EXPECT_TRUE(VerifyPublicJwk(jwk_data, test_data.expected_jwk_alg,
                                kPublicKeyModulusHex, kPublicKeyExponentHex,
                                blink::kWebCryptoKeyUsageEncrypt));
  }
}

struct RsaOaepKnownAnswer {
  const char* pubkey;
  const char* privkey;
  blink::WebCryptoAlgorithmId hash;
  const char* label;
  const char* ciphertext;
  const char* plaintext;
};

const RsaOaepKnownAnswer kRsaOaepKnownAnswers[] = {
    // Tests for RSA-OAEP Encrypt/Decrypt without a label (the empty string).
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha1, "",
     "443FA1354F1DBC5C007A019CACCF18A3E6F986D7513D787F13DED463239F1833D6E33BFA8"
     "AF034C198B0D903F202F543FEF38FAF54018EB7794B4FE638CA4D87C564B845C0695D7E70"
     "C5D4464BEFF05225E747A6CA096A5E5E634002836D3CCE35713F082B20DCE7BB51324E4C8"
     "9E202E3568E9F403ED864DE751BBE63E09680",
     "666F6F64"},
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha256, "",
     "4EBEBCA7297864FDEA44B3BF0E4D5AC26A8478E63642CED06182597DF247EC9883E72DFDC"
     "4507EE5058C52F6B929FBC34CE1FCF7C4A914D5B0C5C94638BDB0C70618E3F4FE79D29158"
     "5DFD3197C4C16B57213337D12E59FC7A38617A1BE3DD2B09DA518E568712D2E6523F079C1"
     "6EFCE540CAA829C5E39D62D64902A4E9D0BB3",
     "666F6F64"},
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha384, "",
     "67B83119BA513947000F3D7F144408FCBD6B22F93C01671C1E40607972BB991716EAA79B9"
     "6FF8DEF95D8E673697E279096D035BA9ED79B9FCAC933A26FB003941CC8E8749511CEDFFA"
     "CA653769AB5209E414B80D30A0DC44BE72E49A7FE732819C0DA4142FA0810FFF19E56EFB1"
     "4EAA5649398E4C7D920B254CAE54F8019B25A",
     "666F6F64"},
    // Tests for RSA-OAEP with an optional label specified.
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha1, "66656564206D65207365796D6F7572",
     "A27427C7CB3CD5B7D3C432B3F4BE98577B1FFBF302EDEFF0B219CABAB3E42DF7E8E24F9A8"
     "9B387D314B199219F91B7F2CDE8E8D6F461A9495C3B5A398E0F670919EF62CCFF96CCF0FC"
     "AF178E5D898C332AAD342F4C42B3209B10CE04B0D004A3ED2514A707D617A321206C22508"
     "4C1446BF17208C6D278B0EA3F5968D1D44590",
     "666F6F64"},
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha256, "66656564206D65207365796D6F7572",
     "333777A27C14C6186D2E90024507D7BD01D2A23FD462E9DBA6E9E96759A7025F29ABAA40D"
     "F6B3355648AE8FD90CD08D9D92529CAD337A6BED806D19B998E472EC0704205A6B83BDB4D"
     "4F5DB03FFE8BC68B44CA7F723A032161BD5B6D4517D58FAD8F1859676BC15A3BE9D20D78B"
     "11D73F79B23372CF4793F6301F612E820D67A",
     "666F6F64"},
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha384, "66656564206D65207365796D6F7572",
     "686D4B27EBD14F4745FB947459DE0D349E53C115F466245268E5C6B8C48FFFDAFB9C77373"
     "60D850A6865883ADD516E4AFE71AACE2A73083A4D991293D59199639CF27F6DC0799F7856"
     "53C3B1E4213EC277A6FA121F6CD2F81E29F1A35A4604B473983CFEDACB00AA9786E92999D"
     "C9FA9AFDBDBB3F0EB3F49B301C5BFE14974BE",
     "666F6F64"},
    // With RSA-OAEP, an empty message is valid.
    {"30819f300d06092a864886f70d010101050003818d0030818902818100a56e4a0e7010175"
     "89a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283"
     "a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c"
     "0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f2"
     "0d0ce8cffb2249bd9a21370203010001",
     "30820275020100300d06092a864886f70d01010105000482025f3082025b0201000281810"
     "0a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad991d8c51056"
     "ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfce0b1dfd5cd9508096d5b2b8"
     "b6df5d671ef6377c0921cb23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b"
     "6f64c4ef22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d4f545"
     "1ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c568c8f97853ad07c02"
     "66c8c6a3ca0929f1e8f11231884429fc4d9ae55fee896a10ce707c3ed7e734e44727a3957"
     "4501a532683109c2abacaba283c31b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6"
     "dcee883547c6a3b325024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8"
     "6296b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b3b6dcd3eda"
     "8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde123fd2700180aa90479b6e48d"
     "e8d67ed24f9f19d85ba275874f542cd20dc723e6963364a1f9425452b269a6799fd024028"
     "fa13938655be1f8a159cbaca5a72ea190c30089e19cd274a556f36c4f6e19f554b34c0777"
     "90427bbdd8dd3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa7120"
     "49898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd48be455eaeb6e16"
     "78255827580a8e4e8e14151d1510a82a3f2e729024027156aba4126d24a81f3a528cbfb27"
     "f56886f840a9f6e86e17a44b94fe9319584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb219"
     "0acf832b847f13a3d24a79f4d",
     blink::kWebCryptoAlgorithmIdSha1, "",
     "5E35C080E4A0EBBB5AC8EE44888A6DB6B8C4D05A6427BE0ECBF245A23BCFC4A4A7D9E5466"
     "123511399F5D01A3CF910DD6FEBCA969B23C753A0574163D847E70E7EFBFBBF6CC0154556"
     "0D620F00CFA33AE318AF6090B76E4ADC5FE1686165786F8B7E559156E8AF690D6EF050133"
     "AB5620FAF91B17CF52FED0B57FAC0924F2CC4",
     ""},
};

TEST_F(WebCryptoRsaOaepTest, EncryptDecryptKnownAnswerTest) {
  for (const auto& test : kRsaOaepKnownAnswers) {
    SCOPED_TRACE(&test - &kRsaOaepKnownAnswers[0]);

    std::vector<uint8_t> public_key_der = HexStringToBytes(test.pubkey);
    std::vector<uint8_t> private_key_der = HexStringToBytes(test.privkey);
    std::vector<uint8_t> ciphertext = HexStringToBytes(test.ciphertext);
    std::vector<uint8_t> plaintext = HexStringToBytes(test.plaintext);
    std::vector<uint8_t> label = HexStringToBytes(test.label);

    blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
        blink::kWebCryptoAlgorithmIdRsaOaep, test.hash);
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
        public_key_der, private_key_der, import_algorithm, false,
        blink::kWebCryptoKeyUsageEncrypt, blink::kWebCryptoKeyUsageDecrypt,
        &public_key, &private_key));

    blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);
    std::vector<uint8_t> decrypted_data;
    ASSERT_EQ(Status::Success(),
              Decrypt(op_algorithm, private_key, ciphertext, &decrypted_data));
    EXPECT_BYTES_EQ(plaintext, decrypted_data);
    std::vector<uint8_t> encrypted_data;
    ASSERT_EQ(Status::Success(),
              Encrypt(op_algorithm, public_key, plaintext, &encrypted_data));
    std::vector<uint8_t> redecrypted_data;
    ASSERT_EQ(Status::Success(), Decrypt(op_algorithm, private_key,
                                         encrypted_data, &redecrypted_data));
    EXPECT_BYTES_EQ(plaintext, redecrypted_data);
  }
}

TEST_F(WebCryptoRsaOaepTest, EncryptWithLargeMessageFails) {
  const blink::WebCryptoAlgorithmId kHash = blink::kWebCryptoAlgorithmIdSha1;
  const size_t kHashSize = 20;

  blink::WebCryptoKey public_key;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                CreatePublicKeyJwkDict({}),
                CreateRsaHashedImportAlgorithm(
                    blink::kWebCryptoAlgorithmIdRsaOaep, kHash),
                true, blink::kWebCryptoKeyUsageEncrypt, &public_key));

  // The maximum size of an encrypted message is:
  //   modulus length
  //   - 1 (leading octet)
  //   - hash size (maskedSeed)
  //   - hash size (lHash portion of maskedDB)
  //   - 1 (at least one octet for the padding string)
  size_t kMaxMessageSize = (kModulusLengthBits / 8) - 2 - (2 * kHashSize);

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  // Test that a message just before the boundary succeeds.
  std::vector<uint8_t> large_message;
  large_message.resize(kMaxMessageSize - 1, 'A');

  std::vector<uint8_t> ciphertext;
  ASSERT_EQ(Status::Success(),
            Encrypt(op_algorithm, public_key, large_message, &ciphertext));

  // Test that a message at the boundary succeeds.
  large_message.resize(kMaxMessageSize, 'A');
  ciphertext.clear();

  ASSERT_EQ(Status::Success(),
            Encrypt(op_algorithm, public_key, large_message, &ciphertext));

  // Test that a message greater than the largest size fails.
  large_message.resize(kMaxMessageSize + 1, 'A');
  ciphertext.clear();

  ASSERT_EQ(Status::OperationError(),
            Encrypt(op_algorithm, public_key, large_message, &ciphertext));
}

// Ensures that if the selected hash algorithm for the RSA-OAEP message is too
// large, then it is rejected, independent of the actual message to be
// encrypted.
// For example, a 1024-bit RSA key is too small to accomodate a message that
// uses OAEP with SHA-512, since it requires 1040 bits to encode
// (2 * hash size + 2 padding bytes).
TEST_F(WebCryptoRsaOaepTest, EncryptWithLargeDigestFails) {
  const blink::WebCryptoAlgorithmId kHash = blink::kWebCryptoAlgorithmIdSha512;

  blink::WebCryptoKey public_key;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                CreatePublicKeyJwkDict({}),
                CreateRsaHashedImportAlgorithm(
                    blink::kWebCryptoAlgorithmIdRsaOaep, kHash),
                true, blink::kWebCryptoKeyUsageEncrypt, &public_key));

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  std::vector<uint8_t> small_message = {'A'};
  std::vector<uint8_t> ciphertext;
  // This is an operation error, as the internal consistency checking of the
  // algorithm parameters is up to the implementation.
  ASSERT_EQ(Status::OperationError(),
            Encrypt(op_algorithm, public_key, small_message, &ciphertext));
}

TEST_F(WebCryptoRsaOaepTest, DecryptWithLargeMessageFails) {
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaOaep,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageDecrypt, &private_key));

  // The label has no influence on the maximum message size. For simplicity,
  // use the empty string.
  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm op_algorithm = CreateRsaOaepAlgorithm(label);

  std::vector<uint8_t> large_dummy_message(kModulusLengthBits / 8, 'A');
  std::vector<uint8_t> plaintext;

  ASSERT_EQ(Status::OperationError(), Decrypt(op_algorithm, private_key,
                                              large_dummy_message, &plaintext));
}

TEST_F(WebCryptoRsaOaepTest, WrapUnwrapRawKey) {
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaOaep, blink::kWebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageWrapKey,
      blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageUnwrapKey,
      &public_key, &private_key));

  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm wrapping_algorithm = CreateRsaOaepAlgorithm(label);

  const std::string key_hex = "000102030405060708090A0B0C0D0E0F";
  const blink::WebCryptoAlgorithm key_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKey key =
      ImportSecretKeyFromRaw(HexStringToBytes(key_hex), key_algorithm,
                             blink::kWebCryptoKeyUsageEncrypt);
  ASSERT_FALSE(key.IsNull());

  std::vector<uint8_t> wrapped_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::kWebCryptoKeyFormatRaw, key, public_key,
                    wrapping_algorithm, &wrapped_key));

  // Verify that |wrapped_key| can be decrypted and yields the key data.
  // Because |private_key| supports both decrypt and unwrap, this is valid.
  std::vector<uint8_t> decrypted_key;
  ASSERT_EQ(Status::Success(), Decrypt(wrapping_algorithm, private_key,
                                       wrapped_key, &decrypted_key));
  EXPECT_BYTES_EQ_HEX(key_hex, decrypted_key);

  // Now attempt to unwrap the key, which should also decrypt the data.
  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::kWebCryptoKeyFormatRaw, wrapped_key, private_key,
                      wrapping_algorithm, key_algorithm, true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
  ASSERT_FALSE(unwrapped_key.IsNull());

  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, unwrapped_key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
}

TEST_F(WebCryptoRsaOaepTest, WrapUnwrapJwkSymKey) {
  // The public and private portions of a 2048-bit RSA key with the
  // id-rsaEncryption OID
  const char kPublicKey2048SpkiDerHex[] =
      "30820122300d06092a864886f70d01010105000382010f003082010a0282010100c5d8ce"
      "137a38168c8ab70229cfa5accc640567159750a312ce2e7d54b6e2fdd59b300c6a6c9764"
      "f8de6f00519cdb90111453d273a967462786480621f9e7cee5b73d63358448e7183a3a68"
      "e991186359f26aa88fbca5f53e673e502e4c5a2ba5068aeba60c9d0c44d872458d1b1e2f"
      "7f339f986076d516e93dc750f0b7680b6f5f02bc0d5590495be04c4ae59d34ba17bc5d08"
      "a93c75cfda2828f4a55b153af912038438276cb4a14f8116ca94db0ea9893652d02fc606"
      "36f19975e3d79a4d8ea8bfed6f8e0a24b63d243b08ea70a086ad56dd6341d733711c89ca"
      "749d4a80b3e6ecd2f8e53731eadeac2ea77788ee55d7b4b47c0f2523fbd61b557c16615d"
      "5d0203010001";
  const char kPrivateKey2048Pkcs8DerHex[] =
      "308204bd020100300d06092a864886f70d0101010500048204a7308204a3020100028201"
      "0100c5d8ce137a38168c8ab70229cfa5accc640567159750a312ce2e7d54b6e2fdd59b30"
      "0c6a6c9764f8de6f00519cdb90111453d273a967462786480621f9e7cee5b73d63358448"
      "e7183a3a68e991186359f26aa88fbca5f53e673e502e4c5a2ba5068aeba60c9d0c44d872"
      "458d1b1e2f7f339f986076d516e93dc750f0b7680b6f5f02bc0d5590495be04c4ae59d34"
      "ba17bc5d08a93c75cfda2828f4a55b153af912038438276cb4a14f8116ca94db0ea98936"
      "52d02fc60636f19975e3d79a4d8ea8bfed6f8e0a24b63d243b08ea70a086ad56dd6341d7"
      "33711c89ca749d4a80b3e6ecd2f8e53731eadeac2ea77788ee55d7b4b47c0f2523fbd61b"
      "557c16615d5d02030100010282010074b70feb41a0b0fcbc207670400556c9450042ede3"
      "d4383fb1ce8f3558a6d4641d26dd4c333fa4db842d2b9cf9d2354d3e16ad027a9f682d8c"
      "f4145a1ad97b9edcd8a41c402bd9d8db10f62f43df854cdccbbb2100834f083f53ed6d42"
      "b1b729a59072b004a4e945fc027db15e9c121d1251464d320d4774d5732df6b3dbf751f4"
      "9b19c9db201e19989c883bbaad5333db47f64f6f7a95b8d4936b10d945aa3f794cfaab62"
      "e7d47686129358914f3b8085f03698a650ab5b8c7e45813f2b0515ec05b6e5195b6a7c2a"
      "0d36969745f431ded4fd059f6aa361a4649541016d356297362b778e90f077d48815b339"
      "ec6f43aba345df93e67fcb6c2cb5b4544e9be902818100e9c90abe5f9f32468c5b6d630c"
      "54a4d7d75e29a72cf792f21e242aac78fd7995c42dfd4ae871d2619ff7096cb05baa78e3"
      "23ecab338401a8059adf7a0d8be3b21edc9a9c82c5605634a2ec81ec053271721351868a"
      "4c2e50c689d7cef94e31ff23658af5843366e2b289c5bf81d72756a7b93487dd8770d69c"
      "1f4e089d6d89f302818100d8a58a727c4e209132afd9933b98c89aca862a01cc0be74133"
      "bee517909e5c379e526895ac4af11780c1fe91194c777c9670b6423f0f5a32fd7691a622"
      "113eef4bed2ef863363a335fd55b0e75088c582437237d7f3ed3f0a643950237bc6e6277"
      "ccd0d0a1b4170aa1047aa7ffa7c8c54be10e8c7327ae2e0885663963817f6f02818100e5"
      "aed9ba4d71b7502e6748a1ce247ecb7bd10c352d6d9256031cdf3c11a65e44b0b7ca2945"
      "134671195af84c6b3bb3d10ebf65ae916f38bd5dbc59a0ad1c69b8beaf57cb3a8335f19b"
      "c7117b576987b48331cd9fd3d1a293436b7bb5e1a35c6560de4b5688ea834367cb0997eb"
      "b578f59ed4cb724c47dba94d3b484c1876dcd70281807f15bc7d2406007cac2b138a96af"
      "2d1e00276b84da593132c253fcb73212732dfd25824c2a615bc3d9b7f2c8d2fa542d3562"
      "b0c7738e61eeff580a6056239fb367ea9e5efe73d4f846033602e90c36a78db6fa8ea792"
      "0769675ec58e237bd994d189c8045a96f5dd3a4f12547257ce224e3c9af830a4da3c0eab"
      "9227a0035ae9028180067caea877e0b23090fc689322b71fbcce63d6596e66ab5fcdbaa0"
      "0d49e93aba8effb4518c2da637f209028401a68f344865b4956b032c69acde51d29177ca"
      "3db99fdbf5e74848ed4fa7bdfc2ebb60e2aaa5354770a763e1399ab7a2099762d525fea0"
      "37f3e1972c45a477e66db95c9609bb27f862700ef93379930786cf751b";
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaOaep, blink::kWebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKey2048SpkiDerHex),
      HexStringToBytes(kPrivateKey2048Pkcs8DerHex), import_algorithm, false,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageWrapKey,
      blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageUnwrapKey,
      &public_key, &private_key));

  std::vector<uint8_t> label;
  blink::WebCryptoAlgorithm wrapping_algorithm = CreateRsaOaepAlgorithm(label);

  const std::string key_hex = "000102030405060708090a0b0c0d0e0f";
  const blink::WebCryptoAlgorithm key_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKey key =
      ImportSecretKeyFromRaw(HexStringToBytes(key_hex), key_algorithm,
                             blink::kWebCryptoKeyUsageEncrypt);
  ASSERT_FALSE(key.IsNull());

  std::vector<uint8_t> wrapped_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::kWebCryptoKeyFormatJwk, key, public_key,
                    wrapping_algorithm, &wrapped_key));

  // Verify that |wrapped_key| can be decrypted and yields a valid JWK object.
  // Because |private_key| supports both decrypt and unwrap, this is valid.
  std::vector<uint8_t> decrypted_jwk;
  ASSERT_EQ(Status::Success(), Decrypt(wrapping_algorithm, private_key,
                                       wrapped_key, &decrypted_jwk));
  EXPECT_TRUE(VerifySecretJwk(decrypted_jwk, "A128CBC", key_hex,
                              blink::kWebCryptoKeyUsageEncrypt));

  // Now attempt to unwrap the key, which should also decrypt the data.
  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::kWebCryptoKeyFormatJwk, wrapped_key, private_key,
                      wrapping_algorithm, key_algorithm, true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
  ASSERT_FALSE(unwrapped_key.IsNull());

  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, unwrapped_key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
}

TEST_F(WebCryptoRsaOaepTest, ImportExportJwkRsaPublicKey) {
  struct TestCase {
    const blink::WebCryptoAlgorithmId hash;
    const blink::WebCryptoKeyUsageMask usage;
    const char* const jwk_alg;
  };
  const TestCase kTests[] = {
      {blink::kWebCryptoAlgorithmIdSha1, blink::kWebCryptoKeyUsageEncrypt,
       "RSA-OAEP"},
      {blink::kWebCryptoAlgorithmIdSha256, blink::kWebCryptoKeyUsageEncrypt,
       "RSA-OAEP-256"},
      {blink::kWebCryptoAlgorithmIdSha384, blink::kWebCryptoKeyUsageEncrypt,
       "RSA-OAEP-384"},
      {blink::kWebCryptoAlgorithmIdSha512, blink::kWebCryptoKeyUsageEncrypt,
       "RSA-OAEP-512"}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(&test - &kTests[0]);

    const blink::WebCryptoAlgorithm import_algorithm =
        CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep,
                                       test.hash);

    // Import the spki to create a public key
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatSpki,
                        HexStringToBytes(kPublicKeySpkiDerHex),
                        import_algorithm, true, test.usage, &public_key));

    // Export the public key as JWK and verify its contents
    std::vector<uint8_t> jwk;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &jwk));
    EXPECT_TRUE(VerifyPublicJwk(jwk, test.jwk_alg, kPublicKeyModulusHex,
                                kPublicKeyExponentHex, test.usage));

    // Import the JWK back in to create a new key
    blink::WebCryptoKey public_key2;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatJwk, jwk, import_algorithm,
                        true, test.usage, &public_key2));
    ASSERT_TRUE(public_key2.Handle());
    EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key2.GetType());
    EXPECT_TRUE(public_key2.Extractable());
    EXPECT_EQ(import_algorithm.Id(), public_key2.Algorithm().Id());

    // TODO(eroman): Export the SPKI and verify matches.
  }
}

}  // namespace

}  // namespace webcrypto
