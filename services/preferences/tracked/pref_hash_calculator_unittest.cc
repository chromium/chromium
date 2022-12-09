// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_calculator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefHashCalculatorTest, TestCurrentAlgorithm) {
  base::Value string_value_1("string value 1");
  base::Value string_value_2("string value 2");
  base::DictionaryValue dictionary_value_1;
  dictionary_value_1.GetDict().Set("int value", 1);
  dictionary_value_1.SetKey("nested empty map", base::DictionaryValue());
  base::DictionaryValue dictionary_value_1_equivalent;
  dictionary_value_1_equivalent.GetDict().Set("int value", 1);
  base::DictionaryValue dictionary_value_2;
  dictionary_value_2.GetDict().Set("int value", 2);

  PrefHashCalculator calc1("seed1", "deviceid", "legacydeviceid");
  PrefHashCalculator calc1_dup("seed1", "deviceid", "legacydeviceid");
  PrefHashCalculator calc2("seed2", "deviceid", "legacydeviceid");
  PrefHashCalculator calc3("seed1", "deviceid2", "legacydeviceid");

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
  base::DictionaryValue nested_empty_dict;
  nested_empty_dict.SetKey("a", base::DictionaryValue());
  nested_empty_dict.SetKey("b", base::ListValue());
  base::ListValue nested_empty_list;
  nested_empty_list.Append(base::Value(base::Value::Type::DICTIONARY));
  nested_empty_list.Append(base::Value(base::Value::Type::LIST));
  nested_empty_list.Append(nested_empty_dict.Clone());

  // A dictionary with an empty dictionary, an empty list, and nested empty
  // dictionaries/lists in it.
  base::DictionaryValue dict_value;
  dict_value.SetString("a", "foo");
  dict_value.SetKey("d", base::ListValue());
  dict_value.SetKey("b", base::DictionaryValue());
  dict_value.SetString("c", "baz");
  dict_value.SetKey("e", std::move(nested_empty_dict));
  dict_value.SetKey("f", std::move(nested_empty_list));

  base::ListValue list_value;
  list_value.Append(true);
  list_value.Append(100);
  list_value.Append(1.0);

  ASSERT_EQ(base::Value::Type::NONE, null_value.type());
  ASSERT_EQ(base::Value::Type::BOOLEAN, bool_value.type());
  ASSERT_EQ(base::Value::Type::INTEGER, int_value.type());
  ASSERT_EQ(base::Value::Type::DOUBLE, double_value.type());
  ASSERT_EQ(base::Value::Type::STRING, string_value.type());
  ASSERT_EQ(base::Value::Type::DICTIONARY, dict_value.type());
  ASSERT_EQ(base::Value::Type::LIST, list_value.type());

  // Test every value type independently. Intentionally omits Type::BINARY which
  // isn't even allowed in JSONWriter's input.
  static const char kExpectedNullValue[] =
      "82A9F3BBC7F9FF84C76B033C854E79EEB162783FA7B3E99FF9372FA8E12C44F7";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &null_value, kExpectedNullValue));

  static const char kExpectedBooleanValue[] =
      "A520D8F43EA307B0063736DC9358C330539D0A29417580514C8B9862632C4CCC";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &bool_value, kExpectedBooleanValue));

  static const char kExpectedIntegerValue[] =
      "8D60DA1F10BF5AA29819D2D66D7CCEF9AABC5DA93C11A0D2BD21078D63D83682";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &int_value, kExpectedIntegerValue));

  static const char kExpectedDoubleValue[] =
      "C9D94772516125BEEDAE68C109D44BC529E719EE020614E894CC7FB4098C545D";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &double_value, kExpectedDoubleValue));

  static const char kExpectedStringValue[] =
      "05ACCBD3B05C45C36CD06190F63EC577112311929D8380E26E5F13182EB68318";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &string_value, kExpectedStringValue));

  static const char kExpectedDictValue[] =
      "7A84DCC710D796C771F789A4DA82C952095AA956B6F1667EE42D0A19ECAA3C4A";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &dict_value, kExpectedDictValue));

  static const char kExpectedListValue[] =
      "8D5A25972DF5AE20D041C780E7CA54E40F614AD53513A0724EE8D62D4F992740";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &list_value, kExpectedListValue));

  // Also test every value type together in the same dictionary.
  base::DictionaryValue everything;
  everything.SetKey("null", std::move(null_value));
  everything.SetKey("bool", std::move(bool_value));
  everything.SetKey("int", std::move(int_value));
  everything.SetKey("double", std::move(double_value));
  everything.SetKey("string", std::move(string_value));
  everything.SetKey("list", std::move(list_value));
  everything.SetKey("dict", std::move(dict_value));
  static const char kExpectedEverythingValue[] =
      "B97D09BE7005693574DCBDD03D8D9E44FB51F4008B73FB56A49A9FA671A1999B";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId, "legacydeviceid")
                .Validate("pref.path", &everything, kExpectedEverythingValue));
}

TEST(PrefHashCalculatorTest, TestCompatibilityWithLegacyDeviceId) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kNewDeviceId[] = "new_test_device_id1";
  static const char kLegacyDeviceId[] = "test_device_id1";

  // As in PrefHashCalculatorTest.CatchHashChanges.
  const base::Value string_value("testing with special chars:\n<>{}:^^@#$\\/");
  static const char kExpectedValue[] =
      "05ACCBD3B05C45C36CD06190F63EC577112311929D8380E26E5F13182EB68318";

  EXPECT_EQ(PrefHashCalculator::VALID_SECURE_LEGACY,
            PrefHashCalculator(kSeed, kNewDeviceId, kLegacyDeviceId)
                .Validate("pref.path", &string_value, kExpectedValue));
}

TEST(PrefHashCalculatorTest, TestNotCompatibleWithEmptyLegacyDeviceId) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kNewDeviceId[] = "unused";
  static const char kLegacyDeviceId[] = "";

  const base::Value string_value("testing with special chars:\n<>{}:^^@#$\\/");
  static const char kExpectedValue[] =
      "F14F989B7CAABF3B36ECAE34492C4D8094D2500E7A86D9A3203E54B274C27CB5";

  EXPECT_EQ(PrefHashCalculator::INVALID,
            PrefHashCalculator(kSeed, kNewDeviceId, kLegacyDeviceId)
                .Validate("pref.path", &string_value, kExpectedValue));
}
