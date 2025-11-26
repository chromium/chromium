// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_calculator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#endif

namespace {
class PrefHashCalculatorEncryptedTest : public testing::Test {
 protected:
  const std::string kSeed = "test_seed_for_calculator";
  const std::string kDeviceId = "test_device_id_calc";

  PrefHashCalculatorEncryptedTest()
      : calculator_(kSeed, kDeviceId),
        test_encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}

  PrefHashCalculator calculator_;
  const os_crypt_async::TestEncryptor test_encryptor_;
};
}  // namespace

TEST(PrefHashCalculatorTest, TestCurrentAlgorithm) {
  base::Value string_value_1("string value 1");
  base::Value string_value_2("string value 2");
  base::Value::Dict dictionary_value_1;
  dictionary_value_1.Set("int value", 1);
  dictionary_value_1.Set("nested empty map", base::Value::Dict());
  base::Value::Dict dictionary_value_1_equivalent;
  dictionary_value_1_equivalent.Set("int value", 1);
  base::Value::Dict dictionary_value_2;
  dictionary_value_2.Set("int value", 2);

  PrefHashCalculator calc1("seed1", "deviceid");
  PrefHashCalculator calc1_dup("seed1", "deviceid");
  PrefHashCalculator calc2("seed2", "deviceid");
  PrefHashCalculator calc3("seed1", "deviceid2");

  // Two calculators with same seed produce same hash.
  ASSERT_EQ(calc1.Calculate("pref_path", &string_value_1),
            calc1_dup.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::VALID,
            calc1_dup.Validate("pref_path", &string_value_1,
                               calc1.Calculate("pref_path", &string_value_1)));

  // Different seeds, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc2.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::INVALID,
            calc2.Validate("pref_path", &string_value_1,
                           calc1.Calculate("pref_path", &string_value_1)));

  // Different device IDs, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc3.Calculate("pref_path", &string_value_1));

  // Different values, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path", &string_value_2));

  // Different paths, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path_2", &string_value_1));

  // Works for dictionaries.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1));
  ASSERT_NE(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_2));

  // Empty dictionary children are pruned.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1_equivalent));

  // NULL value is supported.
  ASSERT_FALSE(
      calc1.Calculate("pref_path", static_cast<const base::Value*>(nullptr))
          .empty());
}

#if BUILDFLAG(IS_WIN)
class PrefHashCalculatorEnterpriseTest : public testing::Test {
 protected:
  PrefHashCalculatorEnterpriseTest() : calculator_("seed", "deviceid") {}

  PrefHashCalculator calculator_;
  std::optional<base::AutoReset<bool>> is_enterprise_device_for_testing_;
};

TEST_F(PrefHashCalculatorEnterpriseTest, EnterpriseDevice) {
  base::Value string_value_1("string value 1");
  base::Value string_value_2("string value 2");

  is_enterprise_device_for_testing_ =
      base::SetIsEnterpriseDeviceForTesting(true);
  ASSERT_EQ(PrefHashCalculator::VALID,
            calculator_.Validate(
                "pref_path", &string_value_1,
                calculator_.Calculate("pref_path", &string_value_2)));
  is_enterprise_device_for_testing_.reset();
}
#endif

// Tests the output against a known value to catch unexpected algorithm changes.
// The test hashes below must NEVER be updated, the serialization algorithm used
// must always be able to generate data that will produce these exact hashes.
TEST(PrefHashCalculatorTest, CatchHashChanges) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kDeviceId[] = "test_device_id1";

  base::Value null_value;
  base::Value bool_value(false);
  base::Value int_value(1234567890);
  base::Value double_value(123.0987654321);
  base::Value string_value("testing with special chars:\n<>{}:^^@#$\\/");

  // For legacy reasons, we have to support pruning of empty lists/dictionaries
  // and nested empty lists/dicts in the hash generation algorithm.
  base::Value::Dict nested_empty_dict;
  nested_empty_dict.Set("a", base::Value::Dict());
  nested_empty_dict.Set("b", base::Value::List());
  base::Value::List nested_empty_list;
  nested_empty_list.Append(base::Value::Dict());
  nested_empty_list.Append(base::Value::List());
  nested_empty_list.Append(nested_empty_dict.Clone());

  // A dictionary with an empty dictionary, an empty list, and nested empty
  // dictionaries/lists in it.
  base::Value::Dict dict_value;
  dict_value.Set("a", "foo");
  dict_value.Set("d", base::Value::List());
  dict_value.Set("b", base::Value::Dict());
  dict_value.Set("c", "baz");
  dict_value.Set("e", std::move(nested_empty_dict));
  dict_value.Set("f", std::move(nested_empty_list));

  base::Value::List list;
  list.Append(true);
  list.Append(100);
  list.Append(1.0);
  base::Value list_value(std::move(list));

  ASSERT_TRUE(null_value.is_none());
  ASSERT_TRUE(bool_value.is_bool());
  ASSERT_TRUE(int_value.is_int());
  ASSERT_TRUE(double_value.is_double());
  ASSERT_TRUE(string_value.is_string());
  ASSERT_TRUE(list_value.is_list());

  // Test every value type independently. Intentionally omits Type::BINARY which
  // isn't even allowed in JSONWriter's input.
  static const char kExpectedNullValue[] =
      "82A9F3BBC7F9FF84C76B033C854E79EEB162783FA7B3E99FF9372FA8E12C44F7";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &null_value, kExpectedNullValue));

  static const char kExpectedBooleanValue[] =
      "A520D8F43EA307B0063736DC9358C330539D0A29417580514C8B9862632C4CCC";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &bool_value, kExpectedBooleanValue));

  static const char kExpectedIntegerValue[] =
      "8D60DA1F10BF5AA29819D2D66D7CCEF9AABC5DA93C11A0D2BD21078D63D83682";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &int_value, kExpectedIntegerValue));

  static const char kExpectedDoubleValue[] =
      "C9D94772516125BEEDAE68C109D44BC529E719EE020614E894CC7FB4098C545D";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &double_value, kExpectedDoubleValue));

  static const char kExpectedStringValue[] =
      "05ACCBD3B05C45C36CD06190F63EC577112311929D8380E26E5F13182EB68318";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &string_value, kExpectedStringValue));

  static const char kExpectedDictValue[] =
      "7A84DCC710D796C771F789A4DA82C952095AA956B6F1667EE42D0A19ECAA3C4A";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &dict_value, kExpectedDictValue));

  static const char kExpectedListValue[] =
      "8D5A25972DF5AE20D041C780E7CA54E40F614AD53513A0724EE8D62D4F992740";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &list_value, kExpectedListValue));

  // Also test every value type together in the same dictionary.
  base::Value::Dict everything;
  everything.Set("null", std::move(null_value));
  everything.Set("bool", std::move(bool_value));
  everything.Set("int", std::move(int_value));
  everything.Set("double", std::move(double_value));
  everything.Set("string", std::move(string_value));
  everything.Set("list", std::move(list_value));
  everything.Set("dict", std::move(dict_value));
  static const char kExpectedEverythingValue[] =
      "B97D09BE7005693574DCBDD03D8D9E44FB51F4008B73FB56A49A9FA671A1999B";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &everything, kExpectedEverythingValue));
}

TEST_F(PrefHashCalculatorEncryptedTest, CalculateEncryptedHash) {
  base::Value value_int(987);
  base::Value value_str("encrypt me");
  base::Value::Dict value_dict;
  value_dict.Set("key", "value");
  const base::Value value_dict_val(value_dict.Clone());
  const base::Value* null_ptr = static_cast<const base::Value*>(nullptr);

  std::optional<std::string> hash1_opt =
      calculator_.CalculateEncryptedHash("p.int", &value_int, &test_encryptor_);
  std::optional<std::string> hash2_opt =
      calculator_.CalculateEncryptedHash("p.str", &value_str, &test_encryptor_);
  std::optional<std::string> hash1_again_opt =
      calculator_.CalculateEncryptedHash("p.int", &value_int, &test_encryptor_);
  std::optional<std::string> hash_dict_opt = calculator_.CalculateEncryptedHash(
      "p.dict", &value_dict_val, &test_encryptor_);
  std::optional<std::string> hash_dict_ptr_opt =
      calculator_.CalculateEncryptedHash("p.dict", &value_dict,
                                         &test_encryptor_);
  std::optional<std::string> hash_null_opt =
      calculator_.CalculateEncryptedHash("p.null", null_ptr, &test_encryptor_);

  // Verify the values.
  ASSERT_TRUE(hash1_opt.has_value());
  ASSERT_TRUE(hash2_opt.has_value());
  ASSERT_TRUE(hash1_again_opt.has_value());
  ASSERT_TRUE(hash_dict_opt.has_value());
  ASSERT_TRUE(hash_dict_ptr_opt.has_value());
  ASSERT_TRUE(hash_null_opt.has_value());

  EXPECT_FALSE(hash1_opt->empty());
  EXPECT_TRUE(base::IsStringUTF8(*hash1_opt) &&
              base::Base64Decode(*hash1_opt).has_value());
  EXPECT_FALSE(hash2_opt->empty());
  EXPECT_FALSE(hash_dict_opt->empty());
  EXPECT_FALSE(hash_null_opt->empty());

  // Check different inputs produce different hashes
  EXPECT_NE(hash1_opt.value(), hash2_opt.value());
  EXPECT_NE(hash1_opt.value(), hash1_again_opt.value());
  EXPECT_NE(hash_dict_opt.value(), hash_dict_ptr_opt.value());
}

TEST_F(PrefHashCalculatorEncryptedTest, ValidateEncryptedHash) {
  base::Value value_int(555);
  base::Value value_other_int(999);
  const base::Value* null_value_ptr = static_cast<const base::Value*>(nullptr);
  std::string path = "p.validate";

  // Generate a VALID hash using the calculator and test encryptor instance
  std::optional<std::string> valid_hash_opt =
      calculator_.CalculateEncryptedHash(path, &value_int, &test_encryptor_);
  ASSERT_TRUE(valid_hash_opt.has_value());
  const std::string& valid_hash_base64 = *valid_hash_opt;

  // Generate hash for null value
  std::optional<std::string> null_hash_opt = calculator_.CalculateEncryptedHash(
      "p.null", null_value_ptr, &test_encryptor_);
  ASSERT_TRUE(null_hash_opt.has_value());
  const std::string& null_hash_base64 = *null_hash_opt;

  // Valid case: Correct value, path, and generated hash
  EXPECT_EQ(PrefHashCalculator::VALID_ENCRYPTED,
            calculator_.ValidateEncrypted(path, &value_int, valid_hash_base64,
                                          &test_encryptor_));

  // Wrong value: Correct path and hash, but different value being checked
  EXPECT_EQ(PrefHashCalculator::INVALID_ENCRYPTED,
            calculator_.ValidateEncrypted(path, &value_other_int,
                                          valid_hash_base64, &test_encryptor_));

  // Wrong path: Correct value and hash, but different path being checked
  EXPECT_EQ(PrefHashCalculator::INVALID_ENCRYPTED,
            calculator_.ValidateEncrypted("p.wrong", &value_int,
                                          valid_hash_base64, &test_encryptor_));

  // Non-Base64 stored hash: Validation should fail (Base64Decode returns false)
  EXPECT_EQ(PrefHashCalculator::INVALID_ENCRYPTED,
            calculator_.ValidateEncrypted(
                path, &value_int, "this is not base64!", &test_encryptor_));

  // Test validation of null value
  EXPECT_EQ(PrefHashCalculator::VALID_ENCRYPTED,
            calculator_.ValidateEncrypted("p.null", null_value_ptr,
                                          null_hash_base64, &test_encryptor_));
  // Null expected, int provided -> Invalid
  EXPECT_EQ(PrefHashCalculator::INVALID_ENCRYPTED,
            calculator_.ValidateEncrypted("p.null", &value_int,
                                          null_hash_base64, &test_encryptor_));
  // Int expected, null provided -> Invalid
  EXPECT_EQ(PrefHashCalculator::INVALID_ENCRYPTED,
            calculator_.ValidateEncrypted(path, null_value_ptr,
                                          valid_hash_base64, &test_encryptor_));
}

TEST_F(PrefHashCalculatorEncryptedTest, EncryptedHashValuesAreStable) {
  base::Value::Dict dict;
  dict.Set("key", "value");
  std::optional<std::string> encrypted_hash =
      calculator_.CalculateEncryptedHash("p.dict", &dict, &test_encryptor_);

  // The hash was encrypted with test_encryptor_, then base64-encoded. Since
  // TestEncryptor uses a random key, and Encryptors always use a random nonce,
  // the actual ciphertext isn't stable between runs. We decode the base64 here
  // then decrypt the hash to get the raw hash value, and compare it against a
  // known hash.
  std::optional<std::string> decrypted_hash =
      test_encryptor_.DecryptData(*base::Base64Decode(*encrypted_hash));
  ASSERT_TRUE(decrypted_hash.has_value());

  // Despite using a std::string to represent it, the decrypted hash is actually
  // a byte string, so we compare it against raw bytes below.
  constexpr auto kExpectedHash = std::to_array<uint8_t>({
      0xd9, 0x01, 0x6e, 0x93, 0x04, 0x0a, 0xfc, 0xcb, 0x86, 0x87, 0x90,
      0x31, 0x93, 0xc5, 0x67, 0xdc, 0xdf, 0xad, 0x49, 0x36, 0x14, 0xee,
      0xad, 0xd0, 0x48, 0x34, 0x81, 0x52, 0x8c, 0xfc, 0x1b, 0x0e,
  });

  EXPECT_EQ(base::as_byte_span(*decrypted_hash), kExpectedHash);
}
