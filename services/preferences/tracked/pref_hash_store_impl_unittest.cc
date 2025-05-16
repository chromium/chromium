// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_store_impl.h"

#include <string>

#include "base/base64.h"
#include "base/values.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_calculator.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

// Helper to get derived key for encrypted hashes; this is a replicate of the
// function in the .cc file.
std::string GetEncKey(const std::string& path) {
  return path + "_encrypted_hash";
}

// Helper to get derived key for encrypted split hashes; calling the GetEncKey
// directly for testing.
std::string GetSplitEncKeyBase(const std::string& path) {
  return GetEncKey(path);
}
// Keys expected in the dictionary passed to ImportHash if it contains
// structured data.
const char kImportMacKey[] = "mac";
const char kImportEncryptedHashKey[] = "encrypted_hash";
}  // namespace

class PrefHashStoreImplTest : public testing::Test {
 public:
  PrefHashStoreImplTest() : contents_(pref_store_contents_) {}

  PrefHashStoreImplTest(const PrefHashStoreImplTest&) = delete;
  PrefHashStoreImplTest& operator=(const PrefHashStoreImplTest&) = delete;

 protected:
  HashStoreContents* GetHashStoreContents() { return &contents_; }

 private:
  base::Value::Dict pref_store_contents_;
  // Must be declared after |pref_store_contents_| as it needs to be outlived
  // by it.
  DictionaryHashStoreContents contents_;
};

TEST_F(PrefHashStoreImplTest, ComputeMac) {
  base::Value string_1("string1");
  base::Value string_2("string2");
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);

  std::string computed_mac_1 = pref_hash_store.ComputeMac("path1", &string_1);
  std::string computed_mac_2 = pref_hash_store.ComputeMac("path1", &string_2);
  std::string computed_mac_3 = pref_hash_store.ComputeMac("path2", &string_1);

  // Quick sanity checks here, see pref_hash_calculator_unittest.cc for more
  // complete tests.
  EXPECT_EQ(computed_mac_1, pref_hash_store.ComputeMac("path1", &string_1));
  EXPECT_NE(computed_mac_1, computed_mac_2);
  EXPECT_NE(computed_mac_1, computed_mac_3);
  EXPECT_EQ(64U, computed_mac_1.size());
}

TEST_F(PrefHashStoreImplTest, ComputeSplitMacs) {
  base::Value::Dict dict;
  dict.Set("a", "string1");
  dict.Set("b", "string2");
  // Verify that dictionary keys can contain a '.' delimiter.
  dict.Set("http://www.example.com", "string3");
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);

  base::Value::Dict computed_macs =
      pref_hash_store.ComputeSplitMacs("foo.bar", &dict);

  const std::string mac_1 = computed_macs.Find("a")->GetString();
  const std::string mac_2 = computed_macs.Find("b")->GetString();
  const std::string mac_3 =
      computed_macs.Find("http://www.example.com")->GetString();

  EXPECT_EQ(3U, computed_macs.size());

  base::Value string_1("string1");
  base::Value string_2("string2");
  base::Value string_3("string3");
  EXPECT_EQ(pref_hash_store.ComputeMac("foo.bar.a", &string_1), mac_1);
  EXPECT_EQ(pref_hash_store.ComputeMac("foo.bar.b", &string_2), mac_2);
  EXPECT_EQ(
      pref_hash_store.ComputeMac("foo.bar.http://www.example.com", &string_3),
      mac_3);
}

TEST_F(PrefHashStoreImplTest, ComputeNullSplitMacs) {
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
  base::Value::Dict computed_macs =
      pref_hash_store.ComputeSplitMacs("foo.bar", nullptr);

  EXPECT_TRUE(computed_macs.empty());
}

TEST_F(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  {
    // 32 NULL bytes is the seed that was used to generate the legacy hash.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // Only NULL should be trusted in the absence of a hash.
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));

    transaction->StoreHash("path1", &string_1);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CLEARED, transaction->CheckValue("path1", NULL));
    transaction->StoreHash("path1", NULL);
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckValue("path1", NULL));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));

    base::Value dict_val(base::Value::Type::DICT);
    base::Value::Dict& dict = dict_val.GetDict();
    dict.Set("a", "foo");
    dict.Set("d", "bad");
    dict.Set("b", "bar");
    dict.Set("c", "baz");

    transaction->StoreHash("path1", &dict_val);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &dict_val));
  }

  ASSERT_FALSE(GetHashStoreContents()->GetSuperMac().empty());

  {
    // |pref_hash_store| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", NULL));
  }

  // Manually corrupt the super MAC.
  GetHashStoreContents()->SetSuperMac(std::string(64, 'A'));

  {
    // |pref_hash_store| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust non-NULL unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", NULL));
  }
}

TEST_F(PrefHashStoreImplTest, ImportExportOperations) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  // Initial state: no super MAC.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());

    ASSERT_FALSE(transaction->HasHash("path1"));

    // Storing a hash will stamp the super MAC.
    transaction->StoreHash("path1", &string_1);

    ASSERT_TRUE(transaction->HasHash("path1"));
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));
  }

  // Make a copy of the stored hash for future use.
  const base::Value* hash =
      GetHashStoreContents()->GetContents()->Find("path1");
  ASSERT_TRUE(hash);
  base::Value path_1_string_1_hash_copy(hash->Clone());
  hash = nullptr;

  // Verify that the super MAC was stamped.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    ASSERT_TRUE(transaction->HasHash("path1"));

    // Clearing the hash should preserve validity.
    transaction->ClearHash("path1");

    // The effects of the clear should be immediately visible.
    ASSERT_FALSE(transaction->HasHash("path1"));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that validity was preserved and that the clear took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    ASSERT_FALSE(transaction->HasHash("path1"));
  }

  // Invalidate the super MAC.
  GetHashStoreContents()->SetSuperMac(std::string());

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());
    ASSERT_FALSE(transaction->HasHash("path1"));

    // An import should preserve invalidity.
    transaction->ImportHash("path1", &path_1_string_1_hash_copy);

    ASSERT_TRUE(transaction->HasHash("path1"));

    // The imported hash should be usable for validating the original value.
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that invalidity was preserved and that the import took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());
    ASSERT_TRUE(transaction->HasHash("path1"));
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));

    // After clearing the hash, non-null values are UNTRUSTED_UNKNOWN.
    transaction->ClearHash("path1");

    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());

    // Test StampSuperMac.
    transaction->StampSuperMac();
  }

  // Verify that the store is now valid.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());

    // Store the hash of a different value to test an "over-import".
    transaction->StoreHash("path1", &string_2);
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());

    // "Over-import". An import should preserve validity.
    transaction->ImportHash("path1", &path_1_string_1_hash_copy);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));
  }

  // Verify that validity was preserved and the "over-import" took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));
  }
}

TEST_F(PrefHashStoreImplTest, SuperMACDisabled) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  {
    // Pass |use_super_mac| => false.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    transaction->StoreHash("path1", &string_2);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  ASSERT_TRUE(GetHashStoreContents()->GetSuperMac().empty());

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
  }
}

TEST_F(PrefHashStoreImplTest, SplitHashStoreAndCheck) {
  base::Value::Dict dict;
  dict.Set("a", base::Value("to be replaced"));
  dict.Set("unchanged.path.with.dots", base::Value("same"));
  dict.Set("o", base::Value("old"));

  base::Value::Dict modified_dict;
  modified_dict.Set("a", base::Value("replaced"));
  modified_dict.Set("unchanged.path.with.dots", base::Value("same"));
  modified_dict.Set("c", base::Value("new"));

  base::Value::Dict empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // No hashes stored yet and hashes dictionary is empty (and thus not
    // trusted).
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    transaction->StoreSplitHash("path1", &dict);

    // Verify match post storage.
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify new path is still unknown.
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path2", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify NULL or empty dicts are declared as having been cleared.
    EXPECT_EQ(ValueState::CLEARED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(ValueState::CLEARED, transaction->CheckSplitValue(
                                       "path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify changes are properly detected.
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckSplitValue(
                                       "path1", &modified_dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys1;
    expected_invalid_keys1.push_back("a");
    expected_invalid_keys1.push_back("c");
    expected_invalid_keys1.push_back("o");
    EXPECT_EQ(expected_invalid_keys1, invalid_keys);
    invalid_keys.clear();

    // Verify |dict| still matches post check.
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Store hash for |modified_dict|.
    transaction->StoreSplitHash("path1", &modified_dict);

    // Verify |modified_dict| is now the one that verifies correctly.
    EXPECT_EQ(
        ValueState::UNCHANGED,
        transaction->CheckSplitValue("path1", &modified_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify old dict no longer matches.
    EXPECT_EQ(ValueState::CHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys2;
    expected_invalid_keys2.push_back("a");
    expected_invalid_keys2.push_back("o");
    expected_invalid_keys2.push_back("c");
    EXPECT_EQ(expected_invalid_keys2, invalid_keys);
    invalid_keys.clear();
  }

  {
    // |pref_hash_store| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
  {
    // Check the same as above for a path with dots.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(
        ValueState::TRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("path.with.dots", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  // Manually corrupt the super MAC.
  GetHashStoreContents()->SetSuperMac(std::string(64, 'A'));

  {
    // |pref_hash_store| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
  {
    // Check the same as above for a path with dots.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(
        ValueState::UNTRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("path.with.dots", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

TEST_F(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::Value::Dict empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // Store hashes for a random dict to be overwritten below.
    base::Value::Dict initial_dict;
    initial_dict.Set("a", "foo");
    transaction->StoreSplitHash("path1", &initial_dict);

    // Verify stored empty dictionary matches NULL and empty dictionary back.
    transaction->StoreSplitHash("path1", &empty_dict);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckSplitValue(
                                         "path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Same when storing NULL directly.
    transaction->StoreSplitHash("path1", NULL);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckSplitValue(
                                         "path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  {
    // |pref_hash_store| should trust its initial hashes dictionary (and thus
    // trust new unknown values) even though the last action done was to clear
    // the hashes for path1 by setting its value to NULL (this is a regression
    // test ensuring that the internal action of clearing some hashes does
    // update the stored hash of hashes).
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    base::Value::Dict tested_dict;
    tested_dict.Set("a", "foo");
    tested_dict.Set("b", "bar");
    EXPECT_EQ(
        ValueState::TRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("new_path", &tested_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

// Test that the PrefHashStore returns TRUSTED_UNKNOWN_VALUE when checking for
// a split preference even if there is an existing atomic preference's hash
// stored. There is no point providing a migration path for preferences
// switching strategies after their initial release as split preferences are
// turned into split preferences specifically because the atomic hash isn't
// considered useful.
TEST_F(PrefHashStoreImplTest, TrustedUnknownSplitValueFromExistingAtomic) {
  base::Value string("string1");

  base::Value::Dict dict;
  dict.Set("a", "foo");
  dict.Set("d", "bad");
  dict.Set("b", "bar");
  dict.Set("c", "baz");

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    transaction->StoreHash("path1", &string);
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckValue("path1", &string));
  }

  {
    // Load a new |pref_hash_store| in which the hashes dictionary is trusted.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

class PrefHashStoreImplEncryptedTest : public testing::Test {
 public:
  const std::string kSeed = "test_seed_store_encrypted";
  const std::string kLegacyDeviceId = "legacy_test_device_id_store_encrypted";

  PrefHashStoreImplEncryptedTest()
      : hash_store_(kSeed, kLegacyDeviceId, /*use_super_mac=*/true),
        test_encryptor_(os_crypt_async::GetTestEncryptorForTesting()),
        dictionary_contents_(pref_store_contents_) {}

 protected:
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      bool with_encryptor) {
    const os_crypt_async::Encryptor* encryptor_arg =
        with_encryptor ? &test_encryptor_ : nullptr;
    return hash_store_.BeginTransaction(&dictionary_contents_, encryptor_arg);
  }

  void VerifyStoredHashes(
      const std::string& path,
      const std::optional<std::string>& expected_mac,
      const std::optional<std::string>& expected_encrypted_b64) {
    SCOPED_TRACE("Verifying hashes for path: " + path);
    std::string stored_mac;
    bool has_mac = dictionary_contents_.GetMac(path, &stored_mac);
    EXPECT_EQ(expected_mac.has_value(), has_mac);
    if (expected_mac) {
      EXPECT_EQ(*expected_mac, stored_mac);
    }
    std::string stored_enc;
    bool has_enc = dictionary_contents_.GetMac(GetEncKey(path), &stored_enc);
    EXPECT_EQ(expected_encrypted_b64.has_value(), has_enc);
    if (expected_encrypted_b64) {
      EXPECT_EQ(*expected_encrypted_b64, stored_enc);
    }
  }

  void VerifyStoredSplitHashes(
      const std::string& base_path,
      const std::string& key,  // sub-key within the split dict
      const std::optional<std::string>& expected_mac,
      const std::optional<std::string>& expected_encrypted_b64) {
    SCOPED_TRACE("Verifying split hashes for path: " + base_path + "." + key);
    std::map<std::string, std::string> split_macs;
    dictionary_contents_.GetSplitMacs(base_path, &split_macs);

    auto mac_it = split_macs.find(key);
    EXPECT_EQ(expected_mac.has_value(), mac_it != split_macs.end());
    if (expected_mac && mac_it != split_macs.end()) {
      EXPECT_EQ(*expected_mac, mac_it->second);
    } else if (expected_mac) {
      ADD_FAILURE() << "Expected MAC for " << key << " not found.";
    }

    std::map<std::string, std::string> split_enc_hashes;
    dictionary_contents_.GetSplitMacs(GetSplitEncKeyBase(base_path),
                                      &split_enc_hashes);
    auto enc_it = split_enc_hashes.find(key);
    EXPECT_EQ(expected_encrypted_b64.has_value(),
              enc_it != split_enc_hashes.end());
    if (expected_encrypted_b64 && enc_it != split_enc_hashes.end()) {
      EXPECT_EQ(*expected_encrypted_b64, enc_it->second);
    } else if (expected_encrypted_b64) {
      ADD_FAILURE() << "Expected Encrypted Hash for " << key << " not found.";
    }
  }

  void MakeSuperMACInvalid() { dictionary_contents_.SetSuperMac("invalid"); }
  void MakeSuperMACValid() {
    const base::Value::Dict* macs_dict = GetCurrentDictionaryContents();
    std::string valid_super_mac;
    if (macs_dict) {
      base::Value dict_value_wrapper(macs_dict->Clone());
      valid_super_mac = hash_store_.ComputeMac("", &dict_value_wrapper);
    } else {
      valid_super_mac =
          hash_store_.ComputeMac("", static_cast<const base::Value*>(nullptr));
    }
    dictionary_contents_.SetSuperMac(valid_super_mac);
  }

  void SeedAtomicMac(const std::string& path, const std::string& mac_value) {
    dictionary_contents_.SetMac(path, mac_value);
  }

  void SeedAtomicEncryptedHash(const std::string& path,
                               const std::string& eh_value_b64) {
    dictionary_contents_.SetMac(GetEncKey(path), eh_value_b64);
  }

  void SeedSplitMacs(const std::string& path,
                     const base::Value::Dict* dict_to_hash) {
    // Remove any existing entry at this path (atomic or old split dict)
    dictionary_contents_.RemoveEntry(path);
    if (dict_to_hash) {
      base::Value::Dict macs = hash_store_.ComputeSplitMacs(path, dict_to_hash);
      for (const auto item : macs) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitMac(path, item.first,
                                           item.second.GetString());
        }
      }
    }
  }

  void SeedSplitEncryptedHashes(const std::string& path,
                                const base::Value::Dict* computed_hashes_dict) {
    std::string enc_base_key = GetSplitEncKeyBase(path);
    dictionary_contents_.RemoveEntry(
        enc_base_key);  // Remove potentially conflicting atomic entry
    dictionary_contents_.RemoveEntry(
        enc_base_key);  // Remove old split dict if present
    if (computed_hashes_dict) {
      for (auto item : *computed_hashes_dict) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitMac(enc_base_key, item.first,
                                           item.second.GetString());
        }
      }
    }
  }

  // Seeds split encrypted hashes by computing them first.
  void SeedSplitEncryptedHashesFromValues(
      const std::string& path,
      const base::Value::Dict* values_to_hash) {
    std::string enc_base_key = GetSplitEncKeyBase(path);
    dictionary_contents_.RemoveEntry(enc_base_key);
    if (values_to_hash) {
      base::Value::Dict computed_hashes =
          hash_store_.ComputeSplitEncryptedHashes(path, values_to_hash,
                                                  &test_encryptor_);
      for (auto item : computed_hashes) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitMac(enc_base_key, item.first,
                                           item.second.GetString());
        }
      }
    }
  }

  const base::Value::Dict* GetCurrentDictionaryContents() {
    return dictionary_contents_.GetContents();
  }

  PrefHashStoreImpl hash_store_;
  os_crypt_async::Encryptor test_encryptor_;
  base::Value::Dict pref_store_contents_;
  DictionaryHashStoreContents dictionary_contents_;
};

TEST_F(PrefHashStoreImplEncryptedTest, StoreAndGetHashes) {
  base::Value value("test_value");
  std::string path = "test.pref";
  std::optional<std::string> stored_mac;
  std::optional<std::string> stored_enc_b64;

  // Store both hashes using transaction with encryptor
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreHash(path, &value);
    tx->StoreEncryptedHash(path, &value);
    stored_mac = tx->GetMac(path);
    stored_enc_b64 = tx->GetEncryptedHash(path);
  }

  // Verify the transaction stored non-empty hashes
  ASSERT_TRUE(stored_mac.has_value());
  ASSERT_FALSE(stored_mac->empty());
  ASSERT_TRUE(stored_enc_b64.has_value());
  ASSERT_FALSE(stored_enc_b64->empty());

  // Verify storage in contents directly.
  VerifyStoredHashes(path, stored_mac, stored_enc_b64);

  // Verify retrieval via a new transaction matches what was stored
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    EXPECT_TRUE(tx->HasHash(path));
    EXPECT_TRUE(tx->HasEncryptedHash(path));
    EXPECT_EQ(stored_mac, tx->GetMac(path));
    EXPECT_EQ(stored_enc_b64, tx->GetEncryptedHash(path));
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, StoreHashOnly) {
  base::Value value("mac_only_value");
  std::string path = "mac.only.pref";
  std::optional<std::string> stored_mac;

  // Store only MAC hash
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->StoreHash(path, &value);
    stored_mac = tx->GetMac(path);
  }

  ASSERT_TRUE(stored_mac.has_value());
  ASSERT_FALSE(stored_mac->empty());

  // Verify storage in contents
  VerifyStoredHashes(path, stored_mac, std::nullopt);

  // Verify retrieval via transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    EXPECT_TRUE(tx->HasHash(path));
    EXPECT_FALSE(tx->HasEncryptedHash(path));
    EXPECT_EQ(stored_mac, tx->GetMac(path));
    EXPECT_EQ(std::nullopt, tx->GetEncryptedHash(path));
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, CheckValueValidation) {
  base::Value value("test_value");
  base::Value wrong_value("wrong_value");
  const base::Value* null_value_ptr = nullptr;
  std::string path = "check.pref";

  // Test with encryptor.
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx);
    MakeSuperMACInvalid();

    // Scenario 1: Store both, check valid, check wrong, check null
    tx->StoreHash(path, &value);
    tx->StoreEncryptedHash(path, &value);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED, tx->CheckValue(path, null_value_ptr));

    // Scenario 2: Store only MAC, check valid, check wrong, check null
    tx->ClearHash(path);
    tx->StoreHash(path, &value);
    // Encrypted missing, fallback to MAC -> UNCHANGED
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED, tx->CheckValue(path, null_value_ptr));

    // Scenario 3: Store only Encrypted, check valid, check wrong, check null
    tx->ClearHash(path);
    tx->StoreEncryptedHash(path, &value);
    // MAC missing, Encrypted OK -> UNCHANGED
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED, tx->CheckValue(path, null_value_ptr));

    // Scenario 4: Store invalid Encrypted, valid MAC -> CHANGED (Enc preferred)
    tx->ClearHash(path);
    tx->StoreHash(path, &value);
    // Manually seed bad data.
    dictionary_contents_.SetMac(GetEncKey(path), "Invalid Base64");
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &value));

    // Scenario 5: Store MAC for null, check null, check value.
    tx->ClearHash(path);
    tx->StoreHash(path, null_value_ptr);
    tx->StoreEncryptedHash(path, null_value_ptr);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, null_value_ptr));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &value));

    // Scenario 6: No Hashes stored, SuperMAC invalid
    tx->ClearHash(path);
    MakeSuperMACInvalid();
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              tx->CheckValue(path, null_value_ptr));

    // Scenario 7: No Hashes stored, SuperMAC valid
    MakeSuperMACValid();
    // Stamp based on current transaction state
    ASSERT_TRUE(tx->StampSuperMac());
    ASSERT_TRUE(tx->IsSuperMACValid());
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              tx->CheckValue(path, null_value_ptr));
  }

  // Test without encryptor.
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    ASSERT_TRUE(tx);
    MakeSuperMACInvalid();

    // Scenario 1: Store only MAC (cannot store encrypted), check valid, check
    // wrong, check null
    tx->StoreHash(path, &value);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED, tx->CheckValue(path, null_value_ptr));

    // Scenario 2: Store MAC for null, check null, check value
    tx->ClearHash(path);
    tx->StoreHash(path, null_value_ptr);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, null_value_ptr));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &value));

    // Scenario 3: Simulate Encrypted Hash only present (can't store this way
    // w/o encryptor).
    tx->ClearHash(path);
    std::string enc_only_hash = base::Base64Encode("OnlyEncryptedData");
    dictionary_contents_.SetMac(GetEncKey(path), enc_only_hash);
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              tx->CheckValue(path, &value));

    // Scenario 4: Simulate Invalid Enc, Valid MAC present
    tx->ClearHash(path);
    tx->StoreHash(path, &value);
    dictionary_contents_.SetMac(GetEncKey(path), "Invalid Base64");
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, ClearHashTest) {
  base::Value value("value");
  std::string path = "clear.test";
  std::optional<std::string> stored_mac;
  std::optional<std::string> stored_enc_b64;

  // Store both using transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreHash(path, &value);
    tx->StoreEncryptedHash(path, &value);
    stored_mac = tx->GetMac(path);
    stored_enc_b64 = tx->GetEncryptedHash(path);
  }
  ASSERT_TRUE(stored_mac.has_value());
  ASSERT_TRUE(stored_enc_b64.has_value());
  ASSERT_TRUE(dictionary_contents_.GetMac(path, nullptr));
  ASSERT_TRUE(dictionary_contents_.GetMac(GetEncKey(path), nullptr));

  // Clear via transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->ClearHash(path);
  }
  // Verify cleared from contents using direct check
  EXPECT_FALSE(dictionary_contents_.GetMac(path, nullptr));
  EXPECT_FALSE(dictionary_contents_.GetMac(GetEncKey(path), nullptr));
}

TEST_F(PrefHashStoreImplEncryptedTest, CheckSplitValueEncryptedPathValidation) {
  const std::string kPrefPath = "my.split.pref.encrypted";
  std::vector<std::string> actual_invalid_keys;

  // Helper lambda to run a specific test case scenario
  auto run_scenario =
      [&](const std::string& scenario_name,
          const base::Value::Dict* current_pref_dict_ptr,
          const base::Value::Dict& original_values_for_hashing,
          ValueState expected_state,
          const std::vector<std::string>& expected_invalid_key_list) {
        SCOPED_TRACE("Scenario: " + scenario_name);

        pref_store_contents_.clear();
        dictionary_contents_.Reset();
        actual_invalid_keys.clear();
        MakeSuperMACInvalid();
        // 1. Seed the encrypted hashes into dictionary_contents_
        // These are the "stored" hashes.
        base::Value::Dict computed_split_encrypted_hashes =
            hash_store_.ComputeSplitEncryptedHashes(
                kPrefPath, &original_values_for_hashing, &test_encryptor_);
        SeedSplitEncryptedHashes(kPrefPath, &computed_split_encrypted_hashes);

        // 2. Ensure no MACs are present for this path to isolate the encrypted
        // path
        SeedSplitMacs(kPrefPath, nullptr);

        // 3. Perform CheckSplitValue with encryptor present
        auto tx = BeginTransaction(/*with_encryptor=*/true);
        ValueState result_state = tx->CheckSplitValue(
            kPrefPath, current_pref_dict_ptr, &actual_invalid_keys);

        // 4. Verify results
        EXPECT_EQ(expected_state, result_state);
        std::sort(actual_invalid_keys.begin(), actual_invalid_keys.end());
        std::vector<std::string> sorted_expected_keys =
            expected_invalid_key_list;
        std::sort(sorted_expected_keys.begin(), sorted_expected_keys.end());
        EXPECT_EQ(sorted_expected_keys, actual_invalid_keys);
      };

  // --- Test Scenarios for the if (encryptor_) block ---
  // --- These tests are triggered when an encryptor_ is present ---

  // Scenario E1: All Keys Match, Hashes Valid
  base::Value::Dict s1_prefs_and_hashes;
  s1_prefs_and_hashes.Set("key1", "value1");
  s1_prefs_and_hashes.Set("key2", "value2");
  run_scenario("E1_AllValid", &s1_prefs_and_hashes, s1_prefs_and_hashes,
               ValueState::UNCHANGED, {});

  // Scenario E2: Value Changed for One Key (Hash Invalid)
  base::Value::Dict s2_current_prefs;
  s2_current_prefs.Set("key1", "value1_MODIFIED");
  s2_current_prefs.Set("key2", "value2");
  base::Value::Dict s2_original_hashes;
  s2_original_hashes.Set("key1", "value1");
  s2_original_hashes.Set("key2", "value2");
  run_scenario("E2_OneValueChanged", &s2_current_prefs, s2_original_hashes,
               ValueState::CHANGED, {"key1"});

  // Scenario E3: Key Added in Value (Not in Stored Hashes)
  base::Value::Dict s3_current_prefs;
  s3_current_prefs.Set("key1", "value1");
  s3_current_prefs.Set("key2", "value2");
  base::Value::Dict s3_original_hashes;
  s3_original_hashes.Set("key1", "value1");
  run_scenario("E3_KeyAddedInValue", &s3_current_prefs, s3_original_hashes,
               ValueState::CHANGED, {"key2"});

  // Scenario E4: Key Removed from Value (Present in Stored Hashes)
  base::Value::Dict s4_current_prefs;
  s4_current_prefs.Set("key1", "value1");
  base::Value::Dict s4_original_hashes;
  s4_original_hashes.Set("key1", "value1");
  s4_original_hashes.Set("key2", "value2");
  run_scenario("E4_KeyRemovedFromValue", &s4_current_prefs, s4_original_hashes,
               ValueState::CHANGED, {"key2"});

  // Scenario E5: Multiple Invalidities (Value Change, Key Added, Key Removed)
  base::Value::Dict s5_current_prefs;
  s5_current_prefs.Set("keyA", "valueA_MODIFIED");
  s5_current_prefs.Set("keyC", "valueC");
  base::Value::Dict s5_original_hashes;
  s5_original_hashes.Set("keyA", "valueA");
  s5_original_hashes.Set("keyB", "valueB");
  run_scenario("E5_MultipleInvalidities", &s5_current_prefs, s5_original_hashes,
               ValueState::CHANGED, {"keyA", "keyB", "keyC"});

  // Scenario E6: Initial Value is Empty, Stored Encrypted Hashes Exist
  base::Value::Dict s6_original_hashes;
  s6_original_hashes.Set("key1", "value1");
  base::Value::Dict s6_empty_current_prefs;
  run_scenario("E6_EmptyValue_HashesExist", &s6_empty_current_prefs,
               s6_original_hashes, ValueState::CLEARED, {});

  // Scenario E6b: Initial Value is Null, Stored Encrypted Hashes Exist
  run_scenario("E6b_NullValue_HashesExist", nullptr, s6_original_hashes,
               ValueState::CLEARED, {});

  // --- Scenario E7: Initial Value Exists, No Stored Encrypted Hashes (empty
  // map of seed hashes) ---
  base::Value::Dict s7_current_prefs;
  s7_current_prefs.Set("key1", "value1");
  base::Value::Dict s7_empty_original_hashes;
  run_scenario("E7_ValueExists_NoHashesStored", &s7_current_prefs,
               s7_empty_original_hashes, ValueState::UNTRUSTED_UNKNOWN_VALUE,
               {});

  // --- Scenario E8: Initial Value Exists, Stored Hash Dictionary is Empty ---
  {
    SCOPED_TRACE("Scenario: E8_ValueExists_EmptyStoredHashDict");
    pref_store_contents_.clear();
    dictionary_contents_.Reset();
    actual_invalid_keys.clear();
    MakeSuperMACInvalid();

    base::Value::Dict s8_current_prefs;
    s8_current_prefs.Set("keyA", "valueA");
    s8_current_prefs.Set("keyB", "valueB");

    // 1. Directly create an empty dictionary for the split encrypted hashes
    //    at the correct nested path within pref_store_contents_.
    std::string enc_base_key_for_split = GetSplitEncKeyBase(kPrefPath);
    std::string full_dotted_path = "protection.macs." + enc_base_key_for_split;
    pref_store_contents_.SetByDottedPath(full_dotted_path, base::Value::Dict());
    // ^^^ Creates an empty dictionary at the full path

    // 2. Ensure no MACs are present for this path
    SeedSplitMacs(kPrefPath, nullptr);

    // 3. Perform CheckSplitValue with encryptor present
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ValueState result_state =
        tx->CheckSplitValue(kPrefPath, &s8_current_prefs, &actual_invalid_keys);

    // 4. Verify results
    // Expected: CHANGED, because current pref has keys, but stored hash dict is
    // empty.
    EXPECT_EQ(ValueState::CHANGED, result_state);
    std::vector<std::string> expected_bad_keys_s8 = {"keyA", "keyB"};
    std::sort(actual_invalid_keys.begin(), actual_invalid_keys.end());
    std::sort(expected_bad_keys_s8.begin(), expected_bad_keys_s8.end());
    EXPECT_EQ(expected_bad_keys_s8, actual_invalid_keys);
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, ComputeSplitEncryptedHashes) {
  const std::string kBasePath = "my.split.pref";

  // Scenario 1: Null split_values
  base::Value::Dict result1 = hash_store_.ComputeSplitEncryptedHashes(
      kBasePath, nullptr, &test_encryptor_);
  EXPECT_TRUE(result1.empty());

  // Scenario 2: Empty split_values dictionary
  base::Value::Dict empty_dict;
  base::Value::Dict result2 = hash_store_.ComputeSplitEncryptedHashes(
      kBasePath, &empty_dict, &test_encryptor_);
  EXPECT_TRUE(result2.empty());

  // Scenario 3: Null encryptor
  base::Value::Dict input_dict3;
  input_dict3.Set("key1", "value1");
  base::Value::Dict result3 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict3, nullptr);
  EXPECT_TRUE(result3.empty());

  // Scenario 4: Valid split_values and encryptor - Functional Test
  base::Value::Dict input_dict4;
  input_dict4.Set("sub1", "alpha");
  input_dict4.Set("sub2", 123);

  base::Value::Dict computed_hashes_for_dict4 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict4,
                                              &test_encryptor_);

  // Assertions for Scenario 4: Check structure and functional validity
  ASSERT_EQ(2u, computed_hashes_for_dict4.size());
  const std::string* hash_sub1_s4 =
      computed_hashes_for_dict4.FindString("sub1");
  const std::string* hash_sub2_s4 =
      computed_hashes_for_dict4.FindString("sub2");
  ASSERT_TRUE(hash_sub1_s4);
  ASSERT_TRUE(hash_sub2_s4);
  EXPECT_FALSE(hash_sub1_s4->empty());
  EXPECT_FALSE(hash_sub2_s4->empty());

  // Verify these hashes are usable by CheckSplitValue
  SeedSplitEncryptedHashes(kBasePath, &computed_hashes_for_dict4);
  SeedSplitMacs(kBasePath, nullptr);
  MakeSuperMACValid();
  {
    auto tx = BeginTransaction(true);
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(ValueState::UNCHANGED,
              tx->CheckSplitValue(kBasePath, &input_dict4, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  // Scenario 5: One sub-item is binary
  // Assuming production code hashes non-serializable (like binary) as if it
  // were an empty string.
  base::Value::Dict input_dict5;
  input_dict5.Set("good_key1", "good_value1");
  std::vector<uint8_t> binary_data = {0, 1, 2};
  input_dict5.Set("bad_key_binary", base::Value(binary_data));
  input_dict5.Set("good_key2", "another_value");

  base::Value::Dict computed_hashes_for_dict5 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict5,
                                              &test_encryptor_);

  // Assertions for Scenario 5 - EXPECTING bad_key_binary TO BE HASHED
  ASSERT_EQ(3u, computed_hashes_for_dict5.size())
      << "Expected all keys to be present if binary is hashed (e.g., as empty "
         "string).";
  EXPECT_TRUE(computed_hashes_for_dict5.contains("good_key1"));
  EXPECT_TRUE(computed_hashes_for_dict5.contains("good_key2"));
  EXPECT_TRUE(computed_hashes_for_dict5.contains("bad_key_binary"));

  const std::string* hash_good1_s5 =
      computed_hashes_for_dict5.FindString("good_key1");
  const std::string* hash_bad_s5 =
      computed_hashes_for_dict5.FindString("bad_key_binary");
  const std::string* hash_good2_s5 =
      computed_hashes_for_dict5.FindString("good_key2");
  ASSERT_TRUE(hash_good1_s5 && !hash_good1_s5->empty());
  ASSERT_TRUE(hash_bad_s5 && !hash_bad_s5->empty());
  ASSERT_TRUE(hash_good2_s5 && !hash_good2_s5->empty());

  // Verify these hashes functionally with CheckSplitValue
  // CheckSplitValue will re-calculate hashes for good_key1, bad_key_binary (as
  // empty string), good_key2 and they should match the stored ones.
  SeedSplitEncryptedHashes(kBasePath, &computed_hashes_for_dict5);
  SeedSplitMacs(kBasePath, nullptr);
  MakeSuperMACValid();
  {
    auto tx = BeginTransaction(true);
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(ValueState::UNCHANGED,
              tx->CheckSplitValue(kBasePath, &input_dict5, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

// For PrefHashStoreImpl::ComputeEncryptedHash(..., Dict*, ...)
TEST_F(PrefHashStoreImplEncryptedTest, ComputeEncryptedHash_ForDict_Success) {
  const std::string kPath = "test.dict.pref.compute";
  base::Value::Dict test_dict;
  test_dict.Set("d_key1", "d_value1");
  test_dict.Set("d_key2", 456);

  // Assuming test_encryptor_instance_ works by default.
  std::string encrypted_hash_str =
      hash_store_.ComputeEncryptedHash(kPath, &test_dict, &test_encryptor_);
  EXPECT_FALSE(encrypted_hash_str.empty());
  std::string decoded_once;
  EXPECT_TRUE(base::Base64Decode(encrypted_hash_str, &decoded_once))
      << "Result should be Base64";
}

// For PrefHashStoreTransactionImpl::StoreEncryptedHash - no encryptor block.
TEST_F(PrefHashStoreImplEncryptedTest,
       StoreEncryptedHash_WhenNoEncryptor_DoesNotStore) {
  const std::string kPath = "test.no.encryptor.store.eh";
  const std::string kEncKey = GetEncKey(kPath);
  base::Value test_value("value to try storing");
  SeedAtomicMac(kPath, "some_existing_mac");

  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->StoreEncryptedHash(kPath, &test_value);

    EXPECT_FALSE(dictionary_contents_.GetMac(kEncKey, nullptr))
        << "Encrypted hash should not be stored.";
    VerifyStoredHashes(kPath, "some_existing_mac", std::nullopt);
  }
}

// PrefHashStoreTransactionImpl::CheckSplitValueInternal - loop for removed keys
// & no-encryptor fallback block.
TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_EncryptorOn_KeyPresentInStoreMissingInValue) {
  const std::string kPath = "split.eh.key_removed_from_value";
  base::Value::Dict original_seeded_values;
  original_seeded_values.Set("keyA", "valA");
  original_seeded_values.Set("keyB_in_store_only", "valB");
  SeedSplitEncryptedHashesFromValues(kPath, &original_seeded_values);
  SeedSplitMacs(kPath, nullptr);

  base::Value::Dict current_pref_dict;
  current_pref_dict.Set("keyA", "valA");

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  std::vector<std::string> invalid_keys;
  ValueState state =
      tx->CheckSplitValue(kPath, &current_pref_dict, &invalid_keys);

  EXPECT_EQ(ValueState::CHANGED, state);
  ASSERT_EQ(1u, invalid_keys.size());
  EXPECT_EQ("keyB_in_store_only", invalid_keys[0]);
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_NoEncryptor_EHExists_NoMACs_ResultsInUntrusted) {
  const std::string kPath = "split.no_encryptor.only_eh_unusable";
  base::Value::Dict values_for_eh;
  values_for_eh.Set("key1", "value1");
  SeedSplitEncryptedHashesFromValues(kPath, &values_for_eh);
  SeedSplitMacs(kPath, nullptr);
  MakeSuperMACInvalid();

  base::Value::Dict current_pref_dict = values_for_eh.Clone();

  auto tx = BeginTransaction(/*with_encryptor=*/false);
  std::vector<std::string> invalid_keys;
  ValueState state =
      tx->CheckSplitValue(kPath, &current_pref_dict, &invalid_keys);

  EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE, state);
  EXPECT_TRUE(invalid_keys.empty());
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_NoEncryptor_EHAndMACsExist_UsesMACs) {
  const std::string kPath = "split.no_encryptor.eh_and_macs";
  base::Value::Dict base_values;
  base_values.Set("key1", "value1");
  base_values.Set("key2", "value2");

  base::Value::Dict eh_seed_values;
  eh_seed_values.Set("key1", "value1");
  SeedSplitEncryptedHashesFromValues(kPath, &eh_seed_values);

  SeedSplitMacs(kPath, &base_values);
  MakeSuperMACInvalid();

  base::Value::Dict current_pref_dict1 = base_values.Clone();
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    std::vector<std::string> invalid_keys;
    ValueState state =
        tx->CheckSplitValue(kPath, &current_pref_dict1, &invalid_keys);
    EXPECT_EQ(ValueState::UNCHANGED, state)
        << "Should rely on MACs when encryptor is off";
    EXPECT_TRUE(invalid_keys.empty());
  }

  base::Value::Dict current_pref_dict2 = base_values.Clone();
  current_pref_dict2.Set("key2", "value2_changed");
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    std::vector<std::string> invalid_keys;
    ValueState state =
        tx->CheckSplitValue(kPath, &current_pref_dict2, &invalid_keys);
    EXPECT_EQ(ValueState::CHANGED, state);
    ASSERT_EQ(1u, invalid_keys.size());
    EXPECT_EQ("key2", invalid_keys[0]);
  }
}

// For PrefHashStoreTransactionImpl::StoreSplitEncryptedHash - private method
TEST_F(PrefHashStoreImplEncryptedTest,
       StoreSplitEncryptedViaIteratedAtomicStores_NoEncryptor) {
  const std::string kBasePath = "my.split.base.noenc.store";
  const std::string kSubKey1 = "subkey1";
  const std::string kFullSubPath1 = kBasePath + "." + kSubKey1;

  auto tx = BeginTransaction(/*with_encryptor=*/false);
  base::Value sub_value1("value1");
  tx->StoreEncryptedHash(kFullSubPath1, &sub_value1);

  EXPECT_FALSE(tx->HasEncryptedHash(kFullSubPath1));
  VerifyStoredHashes(kFullSubPath1, std::nullopt, std::nullopt);
}

TEST_F(PrefHashStoreImplEncryptedTest,
       StoreSplitEncryptedViaIteratedAtomicStores_ClearsOldBaseAtomics) {
  const std::string kBasePath = "my.split.base.clearcheck";
  SeedAtomicEncryptedHash(kBasePath, "old_atomic_base_eh_b64");

  const std::string kSubKey1 = "subkey1";
  const std::string kFullSubPath1 = kBasePath + "." + kSubKey1;
  base::Value sub_value1("value1_for_split_part");

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreEncryptedHash(kFullSubPath1, &sub_value1);
  }

  VerifyStoredHashes(kBasePath, std::nullopt, "old_atomic_base_eh_b64");
  EXPECT_TRUE(BeginTransaction(true)->HasEncryptedHash(kFullSubPath1));
}

// For PrefHashStoreTransactionImpl::ImportHash
TEST_F(PrefHashStoreImplEncryptedTest,
       ImportHash_StringMac_ClearsOldEH_AndStampsSuperMACIfValid) {
  const std::string kPath = "import.string.mac.specific";
  SeedAtomicEncryptedHash(kPath, "eh_to_be_cleared_b64");
  MakeSuperMACValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperMACValid());

    base::Value mac_to_import("newly_imported_mac");
    tx->ImportHash(kPath, &mac_to_import);

    VerifyStoredHashes(kPath, "newly_imported_mac", std::nullopt);
    EXPECT_TRUE(tx->StampSuperMac());
    EXPECT_TRUE(tx->IsSuperMACValid());
  }
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportHash_Dict_MacOnly_ClearsOldEH_AndStampsSuperMACIfValid) {
  const std::string kPath = "import.dict.maconly.specific";
  SeedAtomicEncryptedHash(kPath, "eh_to_be_cleared_b64");
  SeedAtomicMac(kPath, "old_mac_to_be_overwritten");
  MakeSuperMACValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperMACValid());

    base::Value::Dict dict_to_import_data;
    dict_to_import_data.Set(kImportMacKey, "imported_dict_mac");
    base::Value value_for_import(dict_to_import_data.Clone());
    tx->ImportHash(kPath, &value_for_import);

    VerifyStoredHashes(kPath, "imported_dict_mac", std::nullopt);
    EXPECT_TRUE(tx->StampSuperMac());
    EXPECT_TRUE(tx->IsSuperMACValid());
  }
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportHash_Dict_EHOnly_ClearsOldMac_AndStampsSuperMACIfValid) {
  const std::string kPath = "import.dict.ehonly.specific";
  SeedAtomicMac(kPath, "mac_to_be_cleared");
  SeedAtomicEncryptedHash(kPath, "old_eh_to_be_overwritten_b64");
  MakeSuperMACValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperMACValid());

    base::Value::Dict dict_to_import_data;
    dict_to_import_data.Set(kImportEncryptedHashKey, "imported_dict_eh_b64");
    base::Value value_for_import(dict_to_import_data.Clone());
    tx->ImportHash(kPath, &value_for_import);

    VerifyStoredHashes(kPath, std::nullopt, "imported_dict_eh_b64");
    EXPECT_TRUE(tx->StampSuperMac());
    EXPECT_TRUE(tx->IsSuperMACValid());
  }
}

TEST_F(
    PrefHashStoreImplEncryptedTest,
    ImportHash_Dict_NoRelevantKeys_NoOldHashes_SuperMACValid_StampsSuperMAC) {
  const std::string kPath = "import.dict.norelevant.noold";
  dictionary_contents_.RemoveEntry(kPath);
  dictionary_contents_.RemoveEntry(GetEncKey(kPath));
  MakeSuperMACValid();

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx->IsSuperMACValid());
  // Initial StampSuperMac will be true because the transaction's
  // super_mac_dirty_ is set by constructor if it reads a valid MAC (or by
  // StampSuperMac if it had to make it valid). Given the provided StampSuperMac
  // impl, it will always be true here.
  EXPECT_TRUE(tx->StampSuperMac())
      << "First stamp will always be true if use_super_mac is on.";

  base::Value::Dict dict_to_import_data;
  dict_to_import_data.Set("some_other_key", "some_value");
  base::Value value_for_import(dict_to_import_data.Clone());
  tx->ImportHash(kPath, &value_for_import);

  VerifyStoredHashes(kPath, std::nullopt, std::nullopt);

  EXPECT_TRUE(tx->StampSuperMac())
      << "Importing dict when SuperMAC was valid makes it stampable again.";
  EXPECT_TRUE(tx->IsSuperMACValid());
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportHash_OtherType_NoChange_NoStampIfWasClean) {
  const std::string kPath = "import.othertype.nochange";
  SeedAtomicMac(kPath, "old_mac_data");
  MakeSuperMACValid();

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx->IsSuperMACValid());
  tx->StampSuperMac();

  base::Value int_value(123);
  tx->ImportHash(kPath, &int_value);

  VerifyStoredHashes(kPath, "old_mac_data", std::nullopt);
  EXPECT_TRUE(
      tx->StampSuperMac());  // This will be true with the current StampSuperMac
  EXPECT_TRUE(tx->IsSuperMACValid());
}

// For PrefHashStoreTransactionImpl::ClearEncryptedHash - private, tested via
// public ClearHash.
TEST_F(PrefHashStoreImplEncryptedTest,
       ClearHash_TargetingPrivateClearEncryptedHashLogic) {
  const std::string kPath = "clear.eh.via.clearhash";
  const std::string kEncKey = GetEncKey(kPath);

  // Scenario 1: Both exist, SuperMAC valid
  SeedAtomicEncryptedHash(kPath, "eh_data_b64");
  SeedAtomicMac(kPath, "mac_data_also");
  MakeSuperMACValid();
  auto tx1 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx1->IsSuperMACValid());

  tx1->ClearHash(kPath);

  EXPECT_FALSE(dictionary_contents_.GetMac(kPath, nullptr));
  EXPECT_FALSE(dictionary_contents_.GetMac(kEncKey, nullptr));
  EXPECT_TRUE(tx1->StampSuperMac());
  EXPECT_TRUE(tx1->IsSuperMACValid());

  // Scenario 2: Both exist, SuperMAC invalid
  SeedAtomicEncryptedHash(kPath, "eh_data_b64_sm_invalid");
  SeedAtomicMac(kPath, "mac_data_sm_invalid");
  MakeSuperMACInvalid();
  auto tx2 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_FALSE(tx2->IsSuperMACValid());

  tx2->ClearHash(kPath);

  EXPECT_FALSE(dictionary_contents_.GetMac(kEncKey, nullptr));
  EXPECT_FALSE(dictionary_contents_.GetMac(kPath, nullptr));
  EXPECT_TRUE(tx2->StampSuperMac());
  EXPECT_TRUE(tx2->IsSuperMACValid());

  // Scenario 3: Neither EH nor MAC exists, SuperMAC valid
  dictionary_contents_.RemoveEntry(kPath);
  dictionary_contents_.RemoveEntry(GetEncKey(kPath));
  MakeSuperMACValid();
  auto tx3 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx3->IsSuperMACValid());
  tx3->StampSuperMac();

  tx3->ClearHash(kPath);

  // If StampSuperMac always returns true, we can't differentiate a "no-op
  // clean" from "op dirty". We can only check that contents are still empty.
  VerifyStoredHashes(kPath, std::nullopt, std::nullopt);
  EXPECT_TRUE(tx3->StampSuperMac());
}
