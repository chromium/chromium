// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/oblivious_http_request_test_helper.h"

#include <string_view>

#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace network {
namespace test {

namespace {

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
const uint8_t kTestPrivateKey[] = {
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
};

const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

}  // namespace

ObliviousHttpRequestTestHelper::ObliviousHttpRequestTestHelper()
    : key_config_(quiche::ObliviousHttpHeaderKeyConfig::Create(
                      1,
                      EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                      EVP_HPKE_HKDF_SHA256,
                      EVP_HPKE_AES_256_GCM)
                      .value()) {
  ohttp_gateway_ = std::make_unique<quiche::ObliviousHttpGateway>(
      quiche::ObliviousHttpGateway::Create(
          std::string_view(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                           sizeof(kTestPrivateKey)),
          key_config_)
          .value());
}

ObliviousHttpRequestTestHelper::~ObliviousHttpRequestTestHelper() = default;

std::string ObliviousHttpRequestTestHelper::GetPublicKeyConfigs() {
  auto configs =
      quiche::ObliviousHttpKeyConfigs::Create(
          key_config_,
          std::string_view(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                           sizeof(kTestPrivateKey)))
          .value();
  return configs.GenerateConcatenatedKeys().value();
}

std::pair<std::string, quiche::ObliviousHttpRequest::Context>
ObliviousHttpRequestTestHelper::DecryptRequest(
    std::string_view ciphertext_request) {
  auto request =
      ohttp_gateway_->DecryptObliviousHttpRequest(ciphertext_request).value();
  std::string request_plaintext(request.GetPlaintextData());
  return std::make_pair(std::move(request_plaintext),
                        std::move(request).ReleaseContext());
}

std::string ObliviousHttpRequestTestHelper::EncryptResponse(
    std::string plaintext_response,
    quiche::ObliviousHttpRequest::Context& context) {
  auto response =
      ohttp_gateway_->CreateObliviousHttpResponse(plaintext_response, context)
          .value();
  return response.EncapsulateAndSerialize();
}

}  // namespace test
}  // namespace network
