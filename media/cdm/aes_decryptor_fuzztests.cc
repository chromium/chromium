// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"
#include "media/cdm/aes_decryptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

// Data below is all taken from aes_decryptor_unittest.cc. The tests have a
// better description of what is happening.

const uint8_t webm_init_data[] = {
    // base64 equivalent is AAECAw
    0x00, 0x01, 0x02, 0x03};

const uint8_t cenc_init_data[] = {
    0x00, 0x00, 0x00, 0x44,                          // size = 68
    0x70, 0x73, 0x73, 0x68,                          // 'pssh'
    0x01,                                            // version
    0x00, 0x00, 0x00,                                // flags
    0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
    0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
    0x00, 0x00, 0x00, 0x02,                          // key count
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
    0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
    0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
    0x00, 0x00, 0x00, 0x00  // datasize
};

const uint8_t keyids_init_data[] =
    "{\"kids\":[\"AQI\",\"AQIDBA\",\"AQIDBAUGBwgJCgsMDQ4PEA\"]}";

// 3 valid JWKs used as seeds to Update().
const char kKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
    "    }"
    "  ],"
    "  \"type\": \"temporary\""
    "}";

const char kKeyAlternateAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
    "    }"
    "  ]"
    "}";

const char kWrongKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"7u7u7u7u7u7u7u7u7u7u7g\""
    "    }"
    "  ]"
    "}";

const uint8_t kIv[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const media::SubsampleEntry kSubsampleEntriesNormal[] = {{2, 7},
                                                         {3, 11},
                                                         {1, 0}};

// This is the string "Original subsample data." encrypted with key
// 0x0405060708090a0b0c0d0e0f10111213 (base64 equivalent is
// BAUGBwgJCgsMDQ4PEBESEw) and kIv but without any subsamples (thus all
// encrypted).
const uint8_t kEncryptedData[] = {
    0x2f, 0x03, 0x09, 0xef, 0x71, 0xaf, 0x31, 0x16, 0xfa, 0x9d, 0x18, 0x43,
    0x1e, 0x96, 0x71, 0xb5, 0xbf, 0xf5, 0x30, 0x53, 0x9a, 0x20, 0xdf, 0x95};

}  // namespace

// Create a session using |int_init_data_type| and |init_data|. First parameter
// should be EmeInitDataType, but Fuzz tests currently don't support "enum
// class", so using an int (1-3) instead. Test will check parsing of
// |init_data|.
void CreateSessionDoesNotCrash(int int_init_data_type,
                               const std::vector<uint8_t>& init_data) {
  media::EmeInitDataType init_data_type;
  switch (int_init_data_type) {
    case 1:
      init_data_type = media::EmeInitDataType::WEBM;
      break;
    case 2:
      init_data_type = media::EmeInitDataType::CENC;
      break;
    case 3:
      init_data_type = media::EmeInitDataType::KEYIDS;
      break;
    default:
      NOTREACHED();
      return;
  }

  // Create an AesDecryptor. Ignore any messages that may be generated.
  scoped_refptr<media::AesDecryptor> aes_decryptor =
      new media::AesDecryptor(base::DoNothing(), base::DoNothing(),
                              base::DoNothing(), base::DoNothing());

  // Create a session. Ignore the result of the promise as most often it will be
  // rejected due to an error. However, if a session was created, save the
  // session_id so it can be closed.
  std::string session_id;
  auto create_promise =
      std::make_unique<media::CdmCallbackPromise<std::string>>(
          base::BindLambdaForTesting(
              [&](const std::string& session) { session_id = session; }),
          base::DoNothing());
  aes_decryptor->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, init_data_type, init_data,
      std::move(create_promise));

  // If a session was created, free up any session resources.
  if (!session_id.empty()) {
    auto close_promise = std::make_unique<media::CdmCallbackPromise<>>(
        base::DoNothing(), base::DoNothing());
    aes_decryptor->CloseSession(session_id, std::move(close_promise));
  }
}

// Call Update() with |response|. As UpdateSession takes a JSON, passing
// |response| as a string to make specifying seeds easier. Test will check
// parsing of JWK.
void UpdateSessionDoesNotCrash(const std::string& response) {
  // Create an AesDecryptor. Ignore any messages that may be generated.
  scoped_refptr<media::AesDecryptor> aes_decryptor =
      new media::AesDecryptor(base::DoNothing(), base::DoNothing(),
                              base::DoNothing(), base::DoNothing());

  // Create a session. We need to keep track of the session_id as Update() needs
  // a valid session_id. Use WEBM init_data as it's the simplest.
  std::string session_id;
  std::vector<uint8_t> key_id(std::begin(webm_init_data),
                              std::end(webm_init_data));
  auto create_promise =
      std::make_unique<media::CdmCallbackPromise<std::string>>(
          base::BindLambdaForTesting(
              [&](const std::string& session) { session_id = session; }),
          base::DoNothing());
  aes_decryptor->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, media::EmeInitDataType::WEBM, key_id,
      std::move(create_promise));
  EXPECT_GT(session_id.length(), 0ul);

  // Now try UpdateSession with the fuzzed data. Don't bother checking the
  // result of the promise, as most often it will be rejected.
  std::vector<uint8_t> data(std::begin(response), std::end(response));
  auto update_promise = std::make_unique<media::CdmCallbackPromise<>>(
      base::DoNothing(), base::DoNothing());
  aes_decryptor->UpdateSession(session_id, data, std::move(update_promise));

  // Free up any session resources.
  auto close_promise = std::make_unique<media::CdmCallbackPromise<>>(
      base::DoNothing(), base::DoNothing());
  aes_decryptor->CloseSession(session_id, std::move(close_promise));
}

// Decrypts |data| and checks that it succeeds without crashing. The data is
// assumed to have |clear_bytes| in the clear followed by |encrypted_bytes| that
// need to be decrypted, repeated up to the length of |data|. A new session is
// created, and initialized with key AAECAw(base64). Decryption will produce
// random data, but at least we can verify that it doesn't crash.
void DecryptDoesNotCrash(std::size_t clear_bytes,
                         std::size_t encrypted_bytes,
                         const std::vector<uint8_t>& data) {
  // Create an AesDecryptor. Ignore any messages that may be generated.
  scoped_refptr<media::AesDecryptor> aes_decryptor =
      new media::AesDecryptor(base::DoNothing(), base::DoNothing(),
                              base::DoNothing(), base::DoNothing());

  // Create a session. We need to keep track of the session_id as Update() needs
  // a valid session_id.
  std::string session_id;
  std::vector<uint8_t> key_id(std::begin(webm_init_data),
                              std::end(webm_init_data));
  auto create_promise =
      std::make_unique<media::CdmCallbackPromise<std::string>>(
          base::BindLambdaForTesting(
              [&](const std::string& session) { session_id = session; }),
          base::DoNothing());
  aes_decryptor->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, media::EmeInitDataType::WEBM, key_id,
      std::move(create_promise));
  EXPECT_GT(session_id.length(), 0ul);

  // Now UpdateSession with |kKeyAsJWK|. This uses key
  // 0x0405060708090a0b0c0d0e0f10111213 to decrypt the data. Don't bother
  // checking the result of the promise. (Using pop_back() to remove the \0 at
  // the end of the string.)
  std::vector<uint8_t> response(std::begin(kKeyAsJWK), std::end(kKeyAsJWK));
  response.pop_back();
  auto update_promise = std::make_unique<media::CdmCallbackPromise<>>(
      base::DoNothing(), base::DoNothing());
  aes_decryptor->UpdateSession(session_id, response, std::move(update_promise));

  // Create the subsample array. Each subsample will have |clear_bytes| in the
  // clear followed by |encrypted_bytes| that need to be decrypted, repeated up
  // to the length of |data|. If one value is 0, use a single subsample for the
  // whole buffer.
  std::vector<media::SubsampleEntry> subsamples;
  std::size_t length = data.size();
  if (clear_bytes == 0ul) {
    // Assume the whole buffer is encrypted.
    subsamples.emplace_back(0ul, length);
  } else if (encrypted_bytes == 0ul) {
    // Assume the whole buffer is clear.
    subsamples.emplace_back(length, 0ul);
  } else {
    while (length > 0ul) {
      std::size_t clear = std::min(clear_bytes, length);
      length -= clear;
      std::size_t encrypted = std::min(encrypted_bytes, length);
      length -= encrypted;
      subsamples.emplace_back(clear, encrypted);
    }
  }

  // Now attempt to decrypt the fuzzed data. Seed data should be properly
  // encrypted buffer.
  auto encrypted_buffer =
      base::MakeRefCounted<media::DecoderBuffer>(data.size());
  memcpy(encrypted_buffer->writable_data(), data.data(), data.size());
  std::string key_id_string(std::begin(key_id), std::end(key_id));
  std::string iv_string(std::begin(kIv), std::end(kIv));
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCencConfig(
      key_id_string, iv_string, subsamples));
  media::Decryptor::Status result;
  aes_decryptor->Decrypt(
      media::Decryptor::kVideo, encrypted_buffer,
      base::BindLambdaForTesting(
          [&](media::Decryptor::Status status,
              scoped_refptr<media::DecoderBuffer>) { result = status; }));
  // Decrypt should always succeed. Data will be random.
  EXPECT_EQ(result, media::Decryptor::Status::kSuccess);

  // Free up any session resources.
  auto close_promise = std::make_unique<media::CdmCallbackPromise<>>(
      base::DoNothing(), base::DoNothing());
  aes_decryptor->CloseSession(session_id, std::move(close_promise));
}

// Note that most functions check that something is specified, so setting
// MinSize to 1.

// Seed data for CreateSession is valid WEBM, CENC, and KEYIDS init data.
// No support for "enum class" EmeInitDataType, so using an int (1-3) instead.
FUZZ_TEST(AesDecryptorFuzzTests, CreateSessionDoesNotCrash)
    .WithDomains(fuzztest::InRange<int>(1, 3),
                 fuzztest::Arbitrary<std::vector<uint8_t>>().WithMinSize(1))
    .WithSeeds({{1, std::vector<uint8_t>(std::begin(webm_init_data),
                                         std::end(webm_init_data))},
                {2, std::vector<uint8_t>(std::begin(cenc_init_data),
                                         std::end(cenc_init_data))},
                {3, std::vector<uint8_t>(std::begin(keyids_init_data),
                                         std::end(keyids_init_data))}});

// Seed data for UpdateSession is valid JWK.
FUZZ_TEST(AesDecryptorFuzzTests, UpdateSessionDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMinSize(1))
    .WithSeeds({kKeyAsJWK, kKeyAlternateAsJWK, kWrongKeyAsJWK});

// Seed data for Decrypt is a fully encrypted data. First parameter is number of
// clear bytes per subsample, second is number of encrypted bytes per subsample.
FUZZ_TEST(AesDecryptorFuzzTests, DecryptDoesNotCrash)
    .WithDomains(fuzztest::InRange<std::size_t>(0, 100),
                 fuzztest::InRange<std::size_t>(0, 100),
                 fuzztest::Arbitrary<std::vector<uint8_t>>().WithMinSize(1))
    .WithSeeds({{0ul, sizeof(kEncryptedData),
                 std::vector<uint8_t>(std::begin(kEncryptedData),
                                      std::end(kEncryptedData))}});
