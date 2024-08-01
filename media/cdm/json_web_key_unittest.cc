// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/json_web_key.h"

#include <stddef.h>
#include <stdint.h>

#include "base/base64.h"
#include "base/check.h"
#include "media/base/content_decryption_module.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class JSONWebKeyTest : public testing::Test {
 public:
  JSONWebKeyTest() = default;

 protected:
  void ExtractJWKKeysAndExpect(const std::string& jwk,
                               bool expected_result,
                               size_t expected_number_of_keys) {
    DCHECK(!jwk.empty());
    KeyIdAndKeyPairs keys;
    CdmSessionType session_type;
    EXPECT_EQ(expected_result,
              ExtractKeysFromJWKSet(jwk, &keys, &session_type));
    EXPECT_EQ(expected_number_of_keys, keys.size());
  }

  void ExtractSessionTypeAndExpect(const std::string& jwk,
                                   bool expected_result,
                                   CdmSessionType expected_type) {
    DCHECK(!jwk.empty());
    KeyIdAndKeyPairs keys;
    CdmSessionType session_type;
    EXPECT_EQ(expected_result,
              ExtractKeysFromJWKSet(jwk, &keys, &session_type));
    if (expected_result) {
      // Only check if successful.
      EXPECT_EQ(expected_type, session_type);
    }
  }

  void CreateLicenseAndExpect(const uint8_t* key_id,
                              int key_id_length,
                              CdmSessionType session_type,
                              const std::string& expected_result) {
    std::vector<uint8_t> result;
    KeyIdList key_ids;
    key_ids.push_back(std::vector<uint8_t>(key_id, key_id + key_id_length));
    CreateLicenseRequest(key_ids, session_type, &result);
    std::string s(result.begin(), result.end());
    EXPECT_EQ(expected_result, s);
  }

  void ExtractKeyFromLicenseAndExpect(const std::string& license,
                                      bool expected_result,
                                      const uint8_t* expected_key,
                                      int expected_key_length) {
    std::vector<uint8_t> license_vector(license.begin(), license.end());
    std::vector<uint8_t> key;
    EXPECT_EQ(expected_result,
              ExtractFirstKeyIdFromLicenseRequest(license_vector, &key));
    if (expected_result)
      VerifyKeyId(key, expected_key, expected_key_length);
  }

  void VerifyKeyId(std::vector<uint8_t> key,
                   const uint8_t* expected_key,
                   int expected_key_length) {
    std::vector<uint8_t> key_result(expected_key,
                                    expected_key + expected_key_length);
    EXPECT_EQ(key_result, key);
  }

  KeyIdAndKeyPair MakeKeyIdAndKeyPair(const uint8_t* key,
                                      int key_length,
                                      const uint8_t* key_id,
                                      int key_id_length) {
    return std::make_pair(std::string(key_id, key_id + key_id_length),
                          std::string(key, key + key_length));
  }
};

TEST_F(JSONWebKeyTest, GenerateJWKSet) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  EXPECT_EQ("{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data1, std::size(data1), data1, std::size(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQIDBA\",\"kid\":\"AQIDBA\",\"kty\":\"oct\"}]}",
      GenerateJWKSet(data2, std::size(data2), data2, std::size(data2)));
  EXPECT_EQ("{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQIDBA\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data1, std::size(data1), data2, std::size(data2)));
  EXPECT_EQ("{\"keys\":[{\"k\":\"AQIDBA\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data2, std::size(data2), data1, std::size(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kid\":"
      "\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kty\":\"oct\"}]}",
      GenerateJWKSet(data3, std::size(data3), data3, std::size(data3)));

  KeyIdAndKeyPairs keys;
  keys.push_back(
      MakeKeyIdAndKeyPair(data1, std::size(data1), data1, std::size(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"temporary\"}",
      GenerateJWKSet(keys, CdmSessionType::kTemporary));
  keys.push_back(
      MakeKeyIdAndKeyPair(data2, std::size(data2), data2, std::size(data2)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"},{\"k\":"
      "\"AQIDBA\",\"kid\":\"AQIDBA\",\"kty\":\"oct\"}],\"type\":\"persistent-"
      "license\"}",
      GenerateJWKSet(keys, CdmSessionType::kPersistentLicense));
}

TEST_F(JSONWebKeyTest, ExtractValidJWKKeys) {
  // Try an empty 'keys' dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\": [] }", true, 0);

  // Try a key list with one entry.
  const std::string kJwksOneEntry =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksOneEntry, true, 1);

  // Try a key list with multiple entries.
  const std::string kJwksMultipleEntries =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4_QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksMultipleEntries, true, 2);

  // Try a key with no spaces and some \n plus additional fields.
  const std::string kJwksNoSpaces =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"GawgguFyGrWKav7AX4VKUg"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksNoSpaces, true, 1);

  // Try a list with multiple keys with the same kid.
  const std::string kJwksDuplicateKids =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4_QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksDuplicateKids, true, 2);
}

TEST_F(JSONWebKeyTest, ExtractInvalidJWKKeys) {
  // Try a simple JWK key (i.e. not in a set)
  const std::string kJwkSimple =
      "{"
      "  \"kty\": \"oct\","
      "  \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "  \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "}";
  ExtractJWKKeysAndExpect(kJwkSimple, false, 0);

  // Try some non-ASCII characters.
  ExtractJWKKeysAndExpect(
      "This is not ASCII due to \xff\xfe\xfd in it.", false, 0);

  // Try some non-ASCII characters in an otherwise valid JWK.
  const std::string kJwksInvalidCharacters =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"\xff\xfe\xfd"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksInvalidCharacters, false, 0);

  // Try a badly formatted key. Assume that the JSON parser is fully tested,
  // so we won't try a lot of combinations. However, need a test to ensure
  // that the code doesn't crash if invalid JSON received.
  ExtractJWKKeysAndExpect("This is not a JSON key.", false, 0);

  // Try passing some valid JSON that is not a dictionary at the top level.
  ExtractJWKKeysAndExpect("40", false, 0);

  // Try an empty dictionary.
  ExtractJWKKeysAndExpect("{ }", false, 0);

  // Try with 'keys' not a dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\":\"1\" }", false, 0);

  // Try with 'keys' a list of integers.
  ExtractJWKKeysAndExpect("{ \"keys\": [ 1, 2, 3 ] }", false, 0);

  // Try padding(=) at end of 'k' base64 string.
  const std::string kJwksWithPaddedKey =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw==\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKey, false, 0);

  // Try padding(=) at end of 'kid' base64 string.
  const std::string kJwksWithPaddedKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw==\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKeyId, false, 0);

  // Try a key with invalid base64 encoding.
  const std::string kJwksWithInvalidBase64 =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"!@#$%^&*()\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithInvalidBase64, false, 0);

  // Empty key id.
  const std::string kJwksWithEmptyKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyKeyId, false, 0);
}

TEST_F(JSONWebKeyTest, KeyType) {
  // Valid key type.
  const std::string kJwksWithValidKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithValidKeyType, true, 1);

  // Empty key type.
  const std::string kJwksWithEmptyKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyKeyType, false, 0);

  // Key type is case sensitive.
  const std::string kJwksWithUppercaseKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"OCT\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithUppercaseKeyType, false, 0);

  // Wrong key type.
  const std::string kJwksWithWrongKeyType =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"RSA\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithWrongKeyType, false, 0);
}

TEST_F(JSONWebKeyTest, Alg) {
  // 'alg' is ignored, so verify that anything is allowed.
  // Valid alg.
  const std::string kJwksWithValidAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithValidAlg, true, 1);

  // Empty alg.
  const std::string kJwksWithEmptyAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyAlg, true, 1);

  // Alg is case sensitive.
  const std::string kJwksWithLowercaseAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"a128kw\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithLowercaseAlg, true, 1);

  // Wrong alg.
  const std::string kJwksWithWrongAlg =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"RS256\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithWrongAlg, true, 1);
}

TEST_F(JSONWebKeyTest, CdmSessionType) {
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}", true,
      CdmSessionType::kTemporary);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"temporary\"}",
      true, CdmSessionType::kTemporary);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"persistent-license\"}",
      true, CdmSessionType::kPersistentLicense);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":"
      "\"unknown\"}",
      false, CdmSessionType::kTemporary);
  ExtractSessionTypeAndExpect(
      "{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}],\"type\":3}",
      false, CdmSessionType::kTemporary);
}

TEST_F(JSONWebKeyTest, CreateLicense) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  CreateLicenseAndExpect(data1, std::size(data1), CdmSessionType::kTemporary,
                         "{\"kids\":[\"AQI\"],\"type\":\"temporary\"}");
  CreateLicenseAndExpect(
      data1, std::size(data1), CdmSessionType::kPersistentLicense,
      "{\"kids\":[\"AQI\"],\"type\":\"persistent-license\"}");
  CreateLicenseAndExpect(data2, std::size(data2), CdmSessionType::kTemporary,
                         "{\"kids\":[\"AQIDBA\"],\"type\":\"temporary\"}");
  CreateLicenseAndExpect(data3, std::size(data3),
                         CdmSessionType::kPersistentLicense,
                         "{\"kids\":[\"AQIDBAUGBwgJCgsMDQ4PEA\"],\"type\":"
                         "\"persistent-license\"}");
}

TEST_F(JSONWebKeyTest, ExtractLicense) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  ExtractKeyFromLicenseAndExpect("{\"kids\":[\"AQI\"],\"type\":\"temporary\"}",
                                 true, data1, std::size(data1));
  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"AQIDBA\"],\"type\":\"temporary\"}", true, data2,
      std::size(data2));
  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"AQIDBAUGBwgJCgsMDQ4PEA\"],\"type\":\"persistent\"}", true,
      data3, std::size(data3));

  // Try some incorrect JSON.
  ExtractKeyFromLicenseAndExpect("", false, NULL, 0);
  ExtractKeyFromLicenseAndExpect("!@#$%^&*()", false, NULL, 0);

  // Valid JSON, but not a dictionary.
  ExtractKeyFromLicenseAndExpect("6", false, NULL, 0);
  ExtractKeyFromLicenseAndExpect("[\"AQI\"]", false, NULL, 0);

  // Dictionary, but missing expected tag.
  ExtractKeyFromLicenseAndExpect("{\"kid\":[\"AQI\"]}", false, NULL, 0);

  // Correct tag, but empty list.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[]}", false, NULL, 0);

  // Correct tag, but list doesn't contain a string.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[[\"AQI\"]]}", false, NULL, 0);

  // Correct tag, but invalid base64 encoding.
  ExtractKeyFromLicenseAndExpect("{\"kids\":[\"!@#$%^&*()\"]}", false, NULL, 0);
}

TEST_F(JSONWebKeyTest, Base64UrlEncoding) {
  const uint8_t data1[] = {0xfb, 0xfd, 0xfb, 0xfd, 0xfb, 0xfd, 0xfb};

  // Verify that |data1| contains invalid base64url characters '+' and '/'
  // and is padded with = when converted to base64.
  std::string encoded_text = base::Base64Encode(
      std::string(reinterpret_cast<const char*>(&data1[0]), std::size(data1)));
  EXPECT_EQ(encoded_text, "+/37/fv9+w==");
  EXPECT_NE(encoded_text.find('+'), std::string::npos);
  EXPECT_NE(encoded_text.find('/'), std::string::npos);
  EXPECT_NE(encoded_text.find('='), std::string::npos);

  // base64url characters '-' and '_' not in base64 encoding.
  EXPECT_EQ(encoded_text.find('-'), std::string::npos);
  EXPECT_EQ(encoded_text.find('_'), std::string::npos);

  CreateLicenseAndExpect(data1, std::size(data1), CdmSessionType::kTemporary,
                         "{\"kids\":[\"-_37_fv9-w\"],\"type\":\"temporary\"}");

  ExtractKeyFromLicenseAndExpect(
      "{\"kids\":[\"-_37_fv9-w\"],\"type\":\"temporary\"}", true, data1,
      std::size(data1));
}

TEST_F(JSONWebKeyTest, MultipleKeys) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  std::vector<uint8_t> result;
  KeyIdList key_ids;
  key_ids.push_back(std::vector<uint8_t>(data1, data1 + std::size(data1)));
  key_ids.push_back(std::vector<uint8_t>(data2, data2 + std::size(data2)));
  key_ids.push_back(std::vector<uint8_t>(data3, data3 + std::size(data3)));
  CreateLicenseRequest(key_ids, CdmSessionType::kTemporary, &result);
  std::string s(result.begin(), result.end());
  EXPECT_EQ(
      "{\"kids\":[\"AQI\",\"AQIDBA\",\"AQIDBAUGBwgJCgsMDQ4PEA\"],\"type\":"
      "\"temporary\"}",
      s);
}

TEST_F(JSONWebKeyTest, ExtractKeyIds) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  KeyIdList key_ids;
  std::string error_message;

  EXPECT_TRUE(ExtractKeyIdsFromKeyIdsInitData("{\"kids\":[\"AQI\"]}", &key_ids,
                                              &error_message));
  EXPECT_EQ(1u, key_ids.size());
  EXPECT_EQ(0u, error_message.length());
  VerifyKeyId(key_ids[0], data1, std::size(data1));

  EXPECT_TRUE(ExtractKeyIdsFromKeyIdsInitData(
      "{\"kids\":[\"AQI\",\"AQIDBA\",\"AQIDBAUGBwgJCgsMDQ4PEA\"]}", &key_ids,
      &error_message));
  EXPECT_EQ(3u, key_ids.size());
  EXPECT_EQ(0u, error_message.length());
  VerifyKeyId(key_ids[0], data1, std::size(data1));
  VerifyKeyId(key_ids[1], data2, std::size(data2));
  VerifyKeyId(key_ids[2], data3, std::size(data3));

  // Expect failure when non-ascii.
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData(
      "This is not ASCII due to \xff\xfe\x0a in it.", &key_ids,
      &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message,
            "Non ASCII: This is not ASCII due to \\u00FF\\u00FE\\n in it.");

  // Expect failure when not JSON or not a dictionary.
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("This is invalid.", &key_ids,
                                               &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message, "Not valid JSON: This is invalid.");
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("6", &key_ids, &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message, "Not valid JSON: 6");
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData(
      "This is a very long string that is longer than 64 characters and is "
      "invalid.",
      &key_ids, &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message,
            "Not valid JSON: This is a very long string that is longer than 64 "
            "characters ...");

  // Expect failure when "kids" not specified.
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("{\"keys\":[\"AQI\"]}", &key_ids,
                                               &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message, "Missing 'kids' parameter or not a list");

  // Expect failure when invalid key_ids specified.
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("{\"kids\":[1]}", &key_ids,
                                               &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message, "'kids'[0] is not string.");
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("{\"kids\": {\"id\":\"AQI\" }}",
                                               &key_ids, &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message, "Missing 'kids' parameter or not a list");

  // Expect failure when non-base64 key_ids specified.
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("{\"kids\":[\"AQI+\"]}",
                                               &key_ids, &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message,
            "'kids'[0] is not valid base64url encoded. Value: AQI+");
  EXPECT_FALSE(ExtractKeyIdsFromKeyIdsInitData("{\"kids\":[\"AQI\",\"AQI/\"]}",
                                               &key_ids, &error_message));
  EXPECT_EQ(3u, key_ids.size());  // |key_ids| should be unchanged.
  EXPECT_EQ(error_message,
            "'kids'[1] is not valid base64url encoded. Value: AQI/");
}

TEST_F(JSONWebKeyTest, CreateInitData) {
  const uint8_t data1[] = {0x01, 0x02};
  const uint8_t data2[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t data3[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

  KeyIdList key_ids;
  std::string error_message;

  key_ids.push_back(std::vector<uint8_t>(data1, data1 + std::size(data1)));
  std::vector<uint8_t> init_data1;
  CreateKeyIdsInitData(key_ids, &init_data1);
  std::string result1(init_data1.begin(), init_data1.end());
  EXPECT_EQ(result1, "{\"kids\":[\"AQI\"]}");

  key_ids.push_back(std::vector<uint8_t>(data2, data2 + std::size(data2)));
  std::vector<uint8_t> init_data2;
  CreateKeyIdsInitData(key_ids, &init_data2);
  std::string result2(init_data2.begin(), init_data2.end());
  EXPECT_EQ(result2, "{\"kids\":[\"AQI\",\"AQIDBA\"]}");

  key_ids.push_back(std::vector<uint8_t>(data3, data3 + std::size(data3)));
  std::vector<uint8_t> init_data3;
  CreateKeyIdsInitData(key_ids, &init_data3);
  std::string result3(init_data3.begin(), init_data3.end());
  EXPECT_EQ(result3,
            "{\"kids\":[\"AQI\",\"AQIDBA\",\"AQIDBAUGBwgJCgsMDQ4PEA\"]}");
}

}  // namespace media

