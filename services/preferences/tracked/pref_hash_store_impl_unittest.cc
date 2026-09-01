// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_store_impl.h"

#include <string>

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/features.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_calculator.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#include "base/memory/scoped_refptr.h"
#endif

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
// Keys expected in the dictionary passed to ImportAuthData if it contains
// structured data.
const char kImportHmacKey[] = "mac";
const char kImportEncryptedHashKey[] = "encrypted_hash";
}  // namespace

class PrefHashStoreImplTest : public testing::Test {
 public:
  PrefHashStoreImplTest() : contents_(pref_store_contents_) {}

  PrefHashStoreImplTest(const PrefHashStoreImplTest&) = delete;
  PrefHashStoreImplTest& operator=(const PrefHashStoreImplTest&) = delete;

 protected:
  HashStoreContents* GetHashStoreContents() { return &contents_; }

  static void FilterEncryptedHashesRecursive(const base::DictValue& src,
                                             base::DictValue& dest) {
    PrefHashStoreImpl::FilterEncryptedHashesRecursive(src, dest);
  }

 private:
  base::DictValue pref_store_contents_;
  // Must be declared after |pref_store_contents_| as it needs to be outlived
  // by it.
  DictionaryHashStoreContents contents_;
};

TEST_F(PrefHashStoreImplTest, ComputeHmac) {
  base::Value string_1("string1");
  base::Value string_2("string2");
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);

  std::string computed_hmac_1 = pref_hash_store.ComputeHmac("path1", &string_1);
  std::string computed_hmac_2 = pref_hash_store.ComputeHmac("path1", &string_2);
  std::string computed_hmac_3 = pref_hash_store.ComputeHmac("path2", &string_1);

  // Quick sanity checks here, see pref_hash_calculator_unittest.cc for more
  // complete tests.
  EXPECT_EQ(computed_hmac_1, pref_hash_store.ComputeHmac("path1", &string_1));
  EXPECT_NE(computed_hmac_1, computed_hmac_2);
  EXPECT_NE(computed_hmac_1, computed_hmac_3);
  EXPECT_EQ(64U, computed_hmac_1.size());
}

TEST_F(PrefHashStoreImplTest, ComputeSplitHmacs) {
  base::DictValue dict;
  dict.Set("a", "string1");
  dict.Set("b", "string2");
  // Verify that dictionary keys can contain a '.' delimiter.
  dict.Set("http://www.example.com", "string3");
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);

  base::DictValue computed_hmacs =
      pref_hash_store.ComputeSplitHmacs("foo.bar", &dict);

  const std::string mac_1 = computed_hmacs.Find("a")->GetString();
  const std::string mac_2 = computed_hmacs.Find("b")->GetString();
  const std::string mac_3 =
      computed_hmacs.Find("http://www.example.com")->GetString();

  EXPECT_EQ(3U, computed_hmacs.size());

  base::Value string_1("string1");
  base::Value string_2("string2");
  base::Value string_3("string3");
  EXPECT_EQ(pref_hash_store.ComputeHmac("foo.bar.a", &string_1), mac_1);
  EXPECT_EQ(pref_hash_store.ComputeHmac("foo.bar.b", &string_2), mac_2);
  EXPECT_EQ(
      pref_hash_store.ComputeHmac("foo.bar.http://www.example.com", &string_3),
      mac_3);
}

TEST_F(PrefHashStoreImplTest, ComputeNullSplitHmacs) {
  PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
  base::DictValue computed_hmacs =
      pref_hash_store.ComputeSplitHmacs("foo.bar", nullptr);

  EXPECT_TRUE(computed_hmacs.empty());
}

TEST_F(PrefHashStoreImplTest, FilterEncryptedHashesRecursive) {
  base::DictValue src;
  src.Set("pref1", "value1");
  src.Set("pref2_encrypted_hash", "hash2");

  base::DictValue nested;
  nested.Set("pref3", "value3");
  nested.Set("pref4_encrypted_hash", "hash4");
  src.Set("nested", std::move(nested));

  base::DictValue dest;
  FilterEncryptedHashesRecursive(src, dest);

  EXPECT_FALSE(dest.Find("pref1"));
  ASSERT_TRUE(dest.Find("pref2_encrypted_hash"));
  EXPECT_EQ("hash2", dest.Find("pref2_encrypted_hash")->GetString());

  const base::Value* dest_nested_val = dest.Find("nested");
  ASSERT_TRUE(dest_nested_val);
  ASSERT_TRUE(dest_nested_val->is_dict());

  const auto& dest_nested_dict = dest_nested_val->GetDict();
  EXPECT_FALSE(dest_nested_dict.Find("pref3"));
  ASSERT_TRUE(dest_nested_dict.Find("pref4_encrypted_hash"));
  EXPECT_EQ("hash4",
            dest_nested_dict.Find("pref4_encrypted_hash")->GetString());
}

TEST_F(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  {
    // 32 NULL bytes is the seed that was used to generate the legacy hash.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // Only NULL should be trusted in the absence of a hash.
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", nullptr));

    transaction->StoreHmac("path1", &string_1);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CLEARED, transaction->CheckValue("path1", nullptr));
    transaction->StoreHmac("path1", nullptr);
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckValue("path1", nullptr));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));

    base::Value dict_val(base::Value::Type::DICT);
    base::DictValue& dict = dict_val.GetDict();
    dict.Set("a", "foo");
    dict.Set("d", "bad");
    dict.Set("b", "bar");
    dict.Set("c", "baz");

    transaction->StoreHmac("path1", &dict_val);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &dict_val));
  }

  ASSERT_FALSE(GetHashStoreContents()->GetSuperHmac().empty());

  {
    // |pref_hash_store| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", nullptr));
  }

  // Manually corrupt the super HMAC.
  GetHashStoreContents()->SetSuperHmac(std::string(64, 'A'));

  {
    // |pref_hash_store| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust non-NULL unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", nullptr));
  }
}

TEST_F(PrefHashStoreImplTest, ImportExportOperations) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  // Initial state: no super HMAC.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperHmacValid());

    ASSERT_FALSE(transaction->HasAuthenticator("path1"));

    // Storing a hash will stamp the super HMAC.
    transaction->StoreHmac("path1", &string_1);

    ASSERT_TRUE(transaction->HasAuthenticator("path1"));
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

  // Verify that the super HMAC was stamped.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperHmacValid());
    ASSERT_TRUE(transaction->HasAuthenticator("path1"));

    // Clearing the hash should preserve validity.
    transaction->ClearAuthenticators("path1");

    // The effects of the clear should be immediately visible.
    ASSERT_FALSE(transaction->HasAuthenticator("path1"));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", nullptr));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that validity was preserved and that the clear took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperHmacValid());
    ASSERT_FALSE(transaction->HasAuthenticator("path1"));
  }

  // Invalidate the super HMAC.
  GetHashStoreContents()->SetSuperHmac(std::string());

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperHmacValid());
    ASSERT_FALSE(transaction->HasAuthenticator("path1"));

    // An import should preserve invalidity.
    transaction->ImportAuthData("path1", &path_1_string_1_hash_copy);

    ASSERT_TRUE(transaction->HasAuthenticator("path1"));

    // The imported hash should be usable for validating the original value.
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that invalidity was preserved and that the import took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperHmacValid());
    ASSERT_TRUE(transaction->HasAuthenticator("path1"));
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));

    // After clearing the hash, non-null values are UNTRUSTED_UNKNOWN.
    transaction->ClearAuthenticators("path1");

    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", nullptr));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperHmacValid());

    // Test StampSuperHmac.
    transaction->StampSuperHmac();
  }

  // Verify that the store is now valid.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperHmacValid());

    // Store the hash of a different value to test an "over-import".
    transaction->StoreHmac("path1", &string_2);
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperHmacValid());

    // "Over-import". An import should preserve validity.
    transaction->ImportAuthData("path1", &path_1_string_1_hash_copy);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));
  }

  // Verify that validity was preserved and the "over-import" took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperHmacValid());
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(ValueState::CHANGED, transaction->CheckValue("path1", &string_2));
  }
}

TEST_F(PrefHashStoreImplTest, SuperHmacDisabled) {
  base::Value string_1("string1");
  base::Value string_2("string2");

  {
    // Pass |use_super_hmac| => false.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), false, false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    transaction->StoreHmac("path1", &string_2);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  ASSERT_TRUE(GetHashStoreContents()->GetSuperHmac().empty());
  EXPECT_TRUE(GetHashStoreContents()->GetSuperEncryptedHash().empty());

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), false, false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
  }
}

TEST_F(PrefHashStoreImplTest, SuperEncryptedHashDisabled) {
  base::Value string_1("string1");

  {
    // Pass |use_super_hmac| => true, |use_super_encrypted_hash| => false.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    transaction->StoreHmac("path1", &string_1);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
  }

  // Super HMAC should be stored.
  EXPECT_FALSE(GetHashStoreContents()->GetSuperHmac().empty());
  // SuperEncryptedHash should NOT be stored.
  EXPECT_TRUE(GetHashStoreContents()->GetSuperEncryptedHash().empty());
}

TEST_F(PrefHashStoreImplTest, SplitHashStoreAndCheck) {
  base::DictValue dict;
  dict.Set("a", base::Value("to be replaced"));
  dict.Set("unchanged.path.with.dots", base::Value("same"));
  dict.Set("o", base::Value("old"));

  base::DictValue modified_dict;
  modified_dict.Set("a", base::Value("replaced"));
  modified_dict.Set("unchanged.path.with.dots", base::Value("same"));
  modified_dict.Set("c", base::Value("new"));

  base::DictValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // No hashes stored yet and hashes dictionary is empty (and thus not
    // trusted).
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    transaction->StoreSplitHmac("path1", &dict);

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
              transaction->CheckSplitValue("path1", nullptr, &invalid_keys));

    // invalid_keys should contain the keys that were removed.
    std::vector<std::string> expected_cleared_keys;
    expected_cleared_keys.push_back("a");
    expected_cleared_keys.push_back("unchanged.path.with.dots");
    expected_cleared_keys.push_back("o");

    // Sort both vectors for a stable comparison.
    std::sort(expected_cleared_keys.begin(), expected_cleared_keys.end());
    std::sort(invalid_keys.begin(), invalid_keys.end());
    EXPECT_EQ(expected_cleared_keys, invalid_keys);
    invalid_keys.clear();

    EXPECT_EQ(ValueState::CLEARED, transaction->CheckSplitValue(
                                       "path1", &empty_dict, &invalid_keys));
    // The same keys should be reported as invalid/cleared for an empty dict.
    std::sort(invalid_keys.begin(), invalid_keys.end());
    EXPECT_EQ(expected_cleared_keys, invalid_keys);
    invalid_keys.clear();

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
    transaction->StoreSplitHmac("path1", &modified_dict);

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
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
  {
    // Check the same as above for a path with dots.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(
        ValueState::TRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("path.with.dots", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  // Manually corrupt the super HMAC.
  GetHashStoreContents()->SetSuperHmac(std::string(64, 'A'));

  {
    // |pref_hash_store| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust unknown values.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
  {
    // Check the same as above for a path with dots.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));
    EXPECT_EQ(
        ValueState::UNTRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("path.with.dots", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

TEST_F(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::DictValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    // Store hashes for a random dict to be overwritten below.
    base::DictValue initial_dict;
    initial_dict.Set("a", "foo");
    transaction->StoreSplitHmac("path1", &initial_dict);

    // Verify stored empty dictionary matches NULL and empty dictionary back.
    transaction->StoreSplitHmac("path1", &empty_dict);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", nullptr, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckSplitValue(
                                         "path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Same when storing NULL directly.
    transaction->StoreSplitHmac("path1", nullptr);
    EXPECT_EQ(ValueState::UNCHANGED,
              transaction->CheckSplitValue("path1", nullptr, &invalid_keys));
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
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    base::DictValue tested_dict;
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

  base::DictValue dict;
  dict.Set("a", "foo");
  dict.Set("d", "bad");
  dict.Set("b", "bar");
  dict.Set("c", "baz");

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(GetHashStoreContents()));

    transaction->StoreHmac("path1", &string);
    EXPECT_EQ(ValueState::UNCHANGED, transaction->CheckValue("path1", &string));
  }

  {
    // Load a new |pref_hash_store| in which the hashes dictionary is trusted.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), true, true);
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

  PrefHashStoreImplEncryptedTest()
      : hash_store_(kSeed,
                    /*use_super_hmac=*/true,
                    /*use_super_encrypted_hash=*/true),
        test_encryptor_(os_crypt_async::GetTestEncryptorForTesting()),
        dictionary_contents_(pref_store_contents_) {}

 protected:
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      bool with_encryptor) {
    scoped_refptr<const os_crypt_async::Encryptor> encryptor_arg =
        with_encryptor ? test_encryptor_ : nullptr;
    return hash_store_.BeginTransaction(&dictionary_contents_, encryptor_arg);
  }

  void VerifyStoredHashes(
      const std::string& path,
      const std::optional<std::string>& expected_hmac,
      const std::optional<std::string>& expected_encrypted_b64) {
    SCOPED_TRACE("Verifying hashes for path: " + path);
    std::string stored_hmac;
    bool has_hmac =
        dictionary_contents_.GetAtomicPrefAuthenticator(path, &stored_hmac);
    EXPECT_EQ(expected_hmac.has_value(), has_hmac);
    if (expected_hmac) {
      EXPECT_EQ(*expected_hmac, stored_hmac);
    }
    std::string stored_enc;
    bool has_enc = dictionary_contents_.GetAtomicPrefAuthenticator(
        GetEncKey(path), &stored_enc);
    EXPECT_EQ(expected_encrypted_b64.has_value(), has_enc);
    if (expected_encrypted_b64) {
      EXPECT_EQ(*expected_encrypted_b64, stored_enc);
    }
  }

  void VerifyStoredSplitHashes(
      const std::string& base_path,
      const std::string& key,  // sub-key within the split dict
      const std::optional<std::string>& expected_hmac,
      const std::optional<std::string>& expected_encrypted_b64) {
    SCOPED_TRACE("Verifying split hashes for path: " + base_path + "." + key);
    std::map<std::string, std::string> split_hmacs;
    dictionary_contents_.GetSplitPrefAuthenticators(base_path, &split_hmacs);

    auto mac_it = split_hmacs.find(key);
    EXPECT_EQ(expected_hmac.has_value(), mac_it != split_hmacs.end());
    if (expected_hmac && mac_it != split_hmacs.end()) {
      EXPECT_EQ(*expected_hmac, mac_it->second);
    } else if (expected_hmac) {
      ADD_FAILURE() << "Expected MAC for " << key << " not found.";
    }

    std::map<std::string, std::string> split_enc_hashes;
    dictionary_contents_.GetSplitPrefAuthenticators(
        GetSplitEncKeyBase(base_path), &split_enc_hashes);
    auto enc_it = split_enc_hashes.find(key);
    EXPECT_EQ(expected_encrypted_b64.has_value(),
              enc_it != split_enc_hashes.end());
    if (expected_encrypted_b64 && enc_it != split_enc_hashes.end()) {
      EXPECT_EQ(*expected_encrypted_b64, enc_it->second);
    } else if (expected_encrypted_b64) {
      ADD_FAILURE() << "Expected Encrypted Hash for " << key << " not found.";
    }
  }

  void MakeSuperHmacInvalid() { dictionary_contents_.SetSuperHmac("invalid"); }
  void MakeSuperHmacValid() {
    const base::DictValue* macs_dict = GetCurrentDictionaryContents();
    std::string valid_super_hmac;
    if (macs_dict) {
      base::Value dict_value_wrapper(macs_dict->Clone());
      valid_super_hmac = hash_store_.ComputeHmac("", &dict_value_wrapper);
    } else {
      valid_super_hmac =
          hash_store_.ComputeHmac("", static_cast<const base::Value*>(nullptr));
    }
    dictionary_contents_.SetSuperHmac(valid_super_hmac);
  }

  void SeedAtomicHmac(const std::string& path, const std::string& hmac_value) {
    dictionary_contents_.SetAtomicPrefAuthenticator(path, hmac_value);
  }

  void SeedAtomicEncryptedHash(const std::string& path,
                               const std::string& eh_value_b64) {
    dictionary_contents_.SetAtomicPrefAuthenticator(GetEncKey(path),
                                                    eh_value_b64);
  }

  void SeedSplitHmacs(const std::string& path,
                      const base::DictValue* dict_to_hash) {
    // Remove any existing entry at this path (atomic or old split dict)
    dictionary_contents_.RemoveAuthenticator(path);
    if (dict_to_hash) {
      base::DictValue hmacs = hash_store_.ComputeSplitHmacs(path, dict_to_hash);
      for (const auto item : hmacs) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitPrefAuthenticator(
              path, item.first, item.second.GetString());
        }
      }
    }
  }

  void SeedSplitEncryptedHashes(const std::string& path,
                                const base::DictValue* computed_hashes_dict) {
    std::string enc_base_key = GetSplitEncKeyBase(path);
    dictionary_contents_.RemoveAuthenticator(
        enc_base_key);  // Remove potentially conflicting atomic entry
    dictionary_contents_.RemoveAuthenticator(
        enc_base_key);  // Remove old split dict if present
    if (computed_hashes_dict) {
      for (auto item : *computed_hashes_dict) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitPrefAuthenticator(
              enc_base_key, item.first, item.second.GetString());
        }
      }
    }
  }

  // Seeds split encrypted hashes by computing them first.
  void SeedSplitEncryptedHashesFromValues(
      const std::string& path,
      const base::DictValue* values_to_hash) {
    std::string enc_base_key = GetSplitEncKeyBase(path);
    dictionary_contents_.RemoveAuthenticator(enc_base_key);
    if (values_to_hash) {
      base::DictValue computed_hashes = hash_store_.ComputeSplitEncryptedHashes(
          path, values_to_hash, test_encryptor_.get());
      for (auto item : computed_hashes) {
        if (item.second.is_string()) {
          dictionary_contents_.SetSplitPrefAuthenticator(
              enc_base_key, item.first, item.second.GetString());
        }
      }
    }
  }

  const base::DictValue* GetCurrentDictionaryContents() {
    return dictionary_contents_.GetContents();
  }

  PrefHashStoreImpl hash_store_;
  scoped_refptr<os_crypt_async::Encryptor> test_encryptor_;
  base::DictValue pref_store_contents_;
  DictionaryHashStoreContents dictionary_contents_;
};

TEST_F(PrefHashStoreImplEncryptedTest, SuperEncryptedHashOnly) {
  base::Value value("test_value");
  std::string path = "test.pref";

  {
    // Pass |use_super_hmac| => false, |use_super_encrypted_hash| => true.
    PrefHashStoreImpl local_hash_store(kSeed, false, true);
    auto tx = local_hash_store.BeginTransaction(&dictionary_contents_,
                                                test_encryptor_);
    tx->StoreEncryptedHash(path, &value);
  }

  // Super HMAC should NOT be stored.
  EXPECT_TRUE(dictionary_contents_.GetSuperHmac().empty());
  // SuperEncryptedHash should be stored.
  EXPECT_FALSE(dictionary_contents_.GetSuperEncryptedHash().empty());
}

TEST_F(PrefHashStoreImplEncryptedTest, StoreAndGetHashes) {
  base::Value value("test_value");
  std::string path = "test.pref";
  std::optional<std::string> stored_hmac;
  std::optional<std::string> stored_enc_b64;

  // Store both hashes using transaction with encryptor
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreHmac(path, &value);
    tx->StoreEncryptedHash(path, &value);
    stored_hmac = tx->GetHmac(path);
    stored_enc_b64 = tx->GetEncryptedHash(path);
  }

  // Verify the transaction stored non-empty hashes
  ASSERT_TRUE(stored_hmac.has_value());
  ASSERT_FALSE(stored_hmac->empty());
  ASSERT_TRUE(stored_enc_b64.has_value());
  ASSERT_FALSE(stored_enc_b64->empty());

  // Verify storage in contents directly.
  VerifyStoredHashes(path, stored_hmac, stored_enc_b64);

  // Verify retrieval via a new transaction matches what was stored
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    EXPECT_TRUE(tx->HasAuthenticator(path));
    EXPECT_TRUE(tx->HasEncryptedHash(path));
    EXPECT_EQ(stored_hmac, tx->GetHmac(path));
    EXPECT_EQ(stored_enc_b64, tx->GetEncryptedHash(path));
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, SuperEncryptedHashDualWrite) {
  base::Value value("test_value");
  std::string path = "test_pref";

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreEncryptedHash(path, &value);
  }

  std::string super_encrypted_hash =
      dictionary_contents_.GetSuperEncryptedHash();
  EXPECT_FALSE(super_encrypted_hash.empty());

  // Verify that it is NOT written if encryptor is missing and we start fresh.
  base::DictValue fresh_store_contents;
  DictionaryHashStoreContents fresh_contents(fresh_store_contents);
  {
    auto tx = hash_store_.BeginTransaction(&fresh_contents, nullptr);
    tx->StoreHmac(path, &value);
  }
  std::string hash = fresh_contents.GetSuperEncryptedHash();
  EXPECT_TRUE(hash.empty()) << "Hash was not empty! Value: " << hash;
}

TEST_F(PrefHashStoreImplEncryptedTest, StoreHashOnly) {
  base::Value value("mac_only_value");
  std::string path = "mac.only.pref";
  std::optional<std::string> stored_hmac;

  // Store only HMAC
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->StoreHmac(path, &value);
    stored_hmac = tx->GetHmac(path);
  }

  ASSERT_TRUE(stored_hmac.has_value());
  ASSERT_FALSE(stored_hmac->empty());

  // Verify storage in contents
  VerifyStoredHashes(path, stored_hmac, std::nullopt);

  // Verify retrieval via transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    EXPECT_TRUE(tx->HasAuthenticator(path));
    EXPECT_FALSE(tx->HasEncryptedHash(path));
    EXPECT_EQ(stored_hmac, tx->GetHmac(path));
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
    MakeSuperHmacInvalid();

    // Scenario 1: Store both, check valid, check wrong, check null
    tx->StoreHmac(path, &value);
    tx->StoreEncryptedHash(path, &value);
    EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED_ENCRYPTED,
              tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED_ENCRYPTED,
              tx->CheckValue(path, null_value_ptr));

    // Scenario 2: Store only HMAC, check valid, check wrong, check null
    tx->ClearAuthenticators(path);
    tx->StoreHmac(path, &value);
    // Encrypted missing, fallback to HMAC
    EXPECT_EQ(ValueState::UNCHANGED_VIA_HMAC_FALLBACK,
              tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED_VIA_HMAC_FALLBACK,
              tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED_VIA_HMAC_FALLBACK,
              tx->CheckValue(path, null_value_ptr));

    // Scenario 3: Store only Encrypted, check valid, check wrong, check null
    tx->ClearAuthenticators(path);
    tx->StoreEncryptedHash(path, &value);
    // HMAC missing, Encrypted OK
    EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED_ENCRYPTED,
              tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED_ENCRYPTED,
              tx->CheckValue(path, null_value_ptr));

    // Scenario 4: Store invalid Encrypted, valid HMAC -> CHANGED (Enc
    // preferred)
    tx->ClearAuthenticators(path);
    tx->StoreHmac(path, &value);
    // Manually seed bad data.
    dictionary_contents_.SetAtomicPrefAuthenticator(GetEncKey(path),
                                                    "Invalid Base64");
    EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, tx->CheckValue(path, &value));

    // Scenario 5: Store HMAC for null, check null, check value.
    tx->ClearAuthenticators(path);
    tx->StoreHmac(path, null_value_ptr);
    tx->StoreEncryptedHash(path, null_value_ptr);
    EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED,
              tx->CheckValue(path, null_value_ptr));
    EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, tx->CheckValue(path, &value));

    // Scenario 6: No Hashes stored, Super HMAC invalid
    tx->ClearAuthenticators(path);
    MakeSuperHmacInvalid();
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              tx->CheckValue(path, null_value_ptr));

    // Scenario 7: No Hashes stored, Super HMAC valid
    MakeSuperHmacValid();
    // Stamp based on current transaction state
    ASSERT_TRUE(tx->StampSuperHmac());
    ASSERT_TRUE(tx->IsSuperHmacValid());
    EXPECT_EQ(ValueState::TRUSTED_UNKNOWN_VALUE, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
              tx->CheckValue(path, null_value_ptr));
  }

  // Test without encryptor.
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    ASSERT_TRUE(tx);
    MakeSuperHmacInvalid();

    // Scenario 1: Store only HMAC (cannot store encrypted), check valid, check
    // wrong, check null
    tx->StoreHmac(path, &value);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &wrong_value));
    EXPECT_EQ(ValueState::CLEARED, tx->CheckValue(path, null_value_ptr));

    // Scenario 2: Store HMAC for null, check null, check value
    tx->ClearAuthenticators(path);
    tx->StoreHmac(path, null_value_ptr);
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, null_value_ptr));
    EXPECT_EQ(ValueState::CHANGED, tx->CheckValue(path, &value));

    // Scenario 3: Simulate Encrypted Hash only present (can't store this way
    // w/o encryptor).
    tx->ClearAuthenticators(path);
    std::string enc_only_hash = base::Base64Encode("OnlyEncryptedData");
    dictionary_contents_.SetAtomicPrefAuthenticator(GetEncKey(path),
                                                    enc_only_hash);
    EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
              tx->CheckValue(path, &value));

    // Scenario 4: Simulate Invalid Enc, Valid HMAC present
    tx->ClearAuthenticators(path);
    tx->StoreHmac(path, &value);
    dictionary_contents_.SetAtomicPrefAuthenticator(GetEncKey(path),
                                                    "Invalid Base64");
    EXPECT_EQ(ValueState::UNCHANGED, tx->CheckValue(path, &value));
  }
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckValue_DisallowLegacyPrefMacFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(tracked::kDisallowLegacyPrefMacFallback);

  base::Value value("valid_value");
  base::Value wrong_value("wrong_value");
  const base::Value* null_value_ptr = nullptr;
  std::string path = "check.pref";

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx);
  MakeSuperHmacInvalid();

  // Scenario 1: Store only HMAC (legacy). With fallback disallowed, this must
  // NOT return UNCHANGED_VIA_HMAC_FALLBACK and should be treated as untrusted.
  tx->ClearAuthenticators(path);
  tx->StoreHmac(path, &value);
  EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE, tx->CheckValue(path, &value));
  EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE,
            tx->CheckValue(path, &wrong_value));
  EXPECT_EQ(ValueState::TRUSTED_NULL_VALUE,
            tx->CheckValue(path, null_value_ptr));

  // Scenario 2: Store both HMAC and Encrypted Hash. Encrypted hash is valid.
  tx->ClearAuthenticators(path);
  tx->StoreHmac(path, &value);
  tx->StoreEncryptedHash(path, &value);
  EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED, tx->CheckValue(path, &value));
  EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, tx->CheckValue(path, &wrong_value));
  EXPECT_EQ(ValueState::CLEARED_ENCRYPTED,
            tx->CheckValue(path, null_value_ptr));

  // Scenario 3: Store only Encrypted Hash (no HMAC).
  tx->ClearAuthenticators(path);
  tx->StoreEncryptedHash(path, &value);
  EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED, tx->CheckValue(path, &value));
  EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, tx->CheckValue(path, &wrong_value));
  EXPECT_EQ(ValueState::CLEARED_ENCRYPTED,
            tx->CheckValue(path, null_value_ptr));
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_DisallowLegacyPrefMacFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(tracked::kDisallowLegacyPrefMacFallback);

  const std::string kPrefPath = "my.split.pref.disallow_fallback";
  std::vector<std::string> actual_invalid_keys;

  base::DictValue current_prefs;
  current_prefs.Set("key1", "value1");
  current_prefs.Set("key2", "value2");

  // Scenario 1: Store only split HMACs (legacy). With fallback disallowed, this
  // must NOT return UNCHANGED_VIA_HMAC_FALLBACK and should be treated as
  // untrusted.
  pref_store_contents_.clear();
  dictionary_contents_.Reset();
  actual_invalid_keys.clear();
  MakeSuperHmacInvalid();

  base::DictValue computed_split_hmacs =
      hash_store_.ComputeSplitHmacs(kPrefPath, &current_prefs);
  SeedSplitHmacs(kPrefPath, &computed_split_hmacs);

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ValueState result_state =
      tx->CheckSplitValue(kPrefPath, &current_prefs, &actual_invalid_keys);
  EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE, result_state);

  // Scenario 2: Store both split HMACs and split encrypted hashes.
  pref_store_contents_.clear();
  dictionary_contents_.Reset();
  actual_invalid_keys.clear();
  MakeSuperHmacInvalid();

  SeedSplitHmacs(kPrefPath, &computed_split_hmacs);
  base::DictValue computed_split_encrypted_hashes =
      hash_store_.ComputeSplitEncryptedHashes(kPrefPath, &current_prefs,
                                              test_encryptor_.get());
  SeedSplitEncryptedHashes(kPrefPath, &computed_split_encrypted_hashes);

  tx = BeginTransaction(/*with_encryptor=*/true);
  result_state =
      tx->CheckSplitValue(kPrefPath, &current_prefs, &actual_invalid_keys);
  EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED, result_state);
  EXPECT_TRUE(actual_invalid_keys.empty());
}

TEST_F(PrefHashStoreImplEncryptedTest, ClearHashTest) {
  base::Value value("value");
  std::string path = "clear.test";
  std::optional<std::string> stored_hmac;
  std::optional<std::string> stored_enc_b64;

  // Store both using transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreHmac(path, &value);
    tx->StoreEncryptedHash(path, &value);
    stored_hmac = tx->GetHmac(path);
    stored_enc_b64 = tx->GetEncryptedHash(path);
  }
  ASSERT_TRUE(stored_hmac.has_value());
  ASSERT_TRUE(stored_enc_b64.has_value());
  ASSERT_TRUE(dictionary_contents_.GetAtomicPrefAuthenticator(path, nullptr));
  ASSERT_TRUE(dictionary_contents_.GetAtomicPrefAuthenticator(GetEncKey(path),
                                                              nullptr));

  // Clear via transaction
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->ClearAuthenticators(path);
  }
  // Verify cleared from contents using direct check
  EXPECT_FALSE(dictionary_contents_.GetAtomicPrefAuthenticator(path, nullptr));
  EXPECT_FALSE(dictionary_contents_.GetAtomicPrefAuthenticator(GetEncKey(path),
                                                               nullptr));
}

TEST_F(PrefHashStoreImplEncryptedTest, CheckSplitValueEncryptedPathValidation) {
  const std::string kPrefPath = "my.split.pref.encrypted";
  std::vector<std::string> actual_invalid_keys;

  // Helper lambda to run a specific test case scenario
  auto run_scenario =
      [&](const std::string& scenario_name,
          const base::DictValue* current_pref_dict_ptr,
          const base::DictValue& original_values_for_hashing,
          ValueState expected_state,
          const std::vector<std::string>& expected_invalid_key_list) {
        SCOPED_TRACE("Scenario: " + scenario_name);

        pref_store_contents_.clear();
        dictionary_contents_.Reset();
        actual_invalid_keys.clear();
        MakeSuperHmacInvalid();
        // 1. Seed the encrypted hashes into dictionary_contents_
        // These are the "stored" hashes.
        base::DictValue computed_split_encrypted_hashes =
            hash_store_.ComputeSplitEncryptedHashes(
                kPrefPath, &original_values_for_hashing, test_encryptor_.get());
        SeedSplitEncryptedHashes(kPrefPath, &computed_split_encrypted_hashes);

        // 2. Ensure no HMACs are present for this path to isolate the encrypted
        // path
        SeedSplitHmacs(kPrefPath, nullptr);

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
  base::DictValue s1_prefs_and_hashes;
  s1_prefs_and_hashes.Set("key1", "value1");
  s1_prefs_and_hashes.Set("key2", "value2");
  run_scenario("E1_AllValid", &s1_prefs_and_hashes, s1_prefs_and_hashes,
               ValueState::UNCHANGED_ENCRYPTED, {});

  // Scenario E2: Value Changed for One Key (Hash Invalid)
  base::DictValue s2_current_prefs;
  s2_current_prefs.Set("key1", "value1_MODIFIED");
  s2_current_prefs.Set("key2", "value2");
  base::DictValue s2_original_hashes;
  s2_original_hashes.Set("key1", "value1");
  s2_original_hashes.Set("key2", "value2");
  run_scenario("E2_OneValueChanged", &s2_current_prefs, s2_original_hashes,
               ValueState::CHANGED_ENCRYPTED, {"key1"});

  // Scenario E3: Key Added in Value (Not in Stored Hashes)
  base::DictValue s3_current_prefs;
  s3_current_prefs.Set("key1", "value1");
  s3_current_prefs.Set("key2", "value2");
  base::DictValue s3_original_hashes;
  s3_original_hashes.Set("key1", "value1");
  run_scenario("E3_KeyAddedInValue", &s3_current_prefs, s3_original_hashes,
               ValueState::CHANGED_ENCRYPTED, {"key2"});

  // Scenario E4: Key Removed from Value (Present in Stored Hashes)
  base::DictValue s4_current_prefs;
  s4_current_prefs.Set("key1", "value1");
  base::DictValue s4_original_hashes;
  s4_original_hashes.Set("key1", "value1");
  s4_original_hashes.Set("key2", "value2");
  run_scenario("E4_KeyRemovedFromValue", &s4_current_prefs, s4_original_hashes,
               ValueState::CHANGED_ENCRYPTED, {"key2"});

  // Scenario E5: Multiple Invalidities (Value Change, Key Added, Key Removed)
  base::DictValue s5_current_prefs;
  s5_current_prefs.Set("keyA", "valueA_MODIFIED");
  s5_current_prefs.Set("keyC", "valueC");
  base::DictValue s5_original_hashes;
  s5_original_hashes.Set("keyA", "valueA");
  s5_original_hashes.Set("keyB", "valueB");
  run_scenario("E5_MultipleInvalidities", &s5_current_prefs, s5_original_hashes,
               ValueState::CHANGED_ENCRYPTED, {"keyA", "keyB", "keyC"});

  // Scenario E6: Initial Value is Empty, Stored Encrypted Hashes Exist
  base::DictValue s6_original_hashes;
  s6_original_hashes.Set("key1", "value1");
  base::DictValue s6_empty_current_prefs;
  run_scenario("E6_EmptyValue_HashesExist", &s6_empty_current_prefs,
               s6_original_hashes, ValueState::CLEARED_ENCRYPTED, {"key1"});

  // Scenario E6b: Initial Value is Null, Stored Encrypted Hashes Exist
  run_scenario("E6b_NullValue_HashesExist", nullptr, s6_original_hashes,
               ValueState::CLEARED_ENCRYPTED, {"key1"});

  // --- Scenario E7: Initial Value Exists, No Stored Encrypted Hashes (empty
  // map of seed hashes) ---
  base::DictValue s7_current_prefs;
  s7_current_prefs.Set("key1", "value1");
  base::DictValue s7_empty_original_hashes;
  run_scenario("E7_ValueExists_NoHashesStored", &s7_current_prefs,
               s7_empty_original_hashes, ValueState::UNTRUSTED_UNKNOWN_VALUE,
               {});

  // --- Scenario E8: Initial Value Exists, Stored Hash Dictionary is Empty ---
  {
    SCOPED_TRACE("Scenario: E8_ValueExists_EmptyStoredHashDict");
    pref_store_contents_.clear();
    dictionary_contents_.Reset();
    actual_invalid_keys.clear();
    MakeSuperHmacInvalid();

    base::DictValue s8_current_prefs;
    s8_current_prefs.Set("keyA", "valueA");
    s8_current_prefs.Set("keyB", "valueB");

    // 1. Directly create an empty dictionary for the split encrypted hashes
    //    at the correct nested path within pref_store_contents_.
    std::string enc_base_key_for_split = GetSplitEncKeyBase(kPrefPath);
    std::string full_dotted_path = "protection.macs." + enc_base_key_for_split;
    pref_store_contents_.SetByDottedPath(full_dotted_path, base::DictValue());
    // ^^^ Creates an empty dictionary at the full path

    // 2. Ensure no HMACs are present for this path
    SeedSplitHmacs(kPrefPath, nullptr);

    // 3. Perform CheckSplitValue with encryptor present
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ValueState result_state =
        tx->CheckSplitValue(kPrefPath, &s8_current_prefs, &actual_invalid_keys);

    // 4. Verify results
    // Expected: CHANGED, because current pref has keys, but stored hash dict is
    // empty.
    EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, result_state);
    std::vector<std::string> expected_bad_keys_s8 = {"keyA", "keyB"};
    std::sort(actual_invalid_keys.begin(), actual_invalid_keys.end());
    std::sort(expected_bad_keys_s8.begin(), expected_bad_keys_s8.end());
    EXPECT_EQ(expected_bad_keys_s8, actual_invalid_keys);
  }
}

TEST_F(PrefHashStoreImplEncryptedTest, ComputeSplitEncryptedHashes) {
  const std::string kBasePath = "my.split.pref";

  // Scenario 1: Null split_values
  base::DictValue result1 = hash_store_.ComputeSplitEncryptedHashes(
      kBasePath, nullptr, test_encryptor_.get());
  EXPECT_TRUE(result1.empty());

  // Scenario 2: Empty split_values dictionary
  base::DictValue empty_dict;
  base::DictValue result2 = hash_store_.ComputeSplitEncryptedHashes(
      kBasePath, &empty_dict, test_encryptor_.get());
  EXPECT_TRUE(result2.empty());

  // Scenario 3: Null encryptor
  base::DictValue input_dict3;
  input_dict3.Set("key1", "value1");
  base::DictValue result3 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict3, nullptr);
  EXPECT_TRUE(result3.empty());

  // Scenario 4: Valid split_values and encryptor - Functional Test
  base::DictValue input_dict4;
  input_dict4.Set("sub1", "alpha");
  input_dict4.Set("sub2", 123);

  base::DictValue computed_hashes_for_dict4 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict4,
                                              test_encryptor_.get());

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
  SeedSplitHmacs(kBasePath, nullptr);
  MakeSuperHmacValid();
  {
    auto tx = BeginTransaction(true);
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED,
              tx->CheckSplitValue(kBasePath, &input_dict4, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  // Scenario 5: One sub-item is binary
  // Assuming production code hashes non-serializable (like binary) as if it
  // were an empty string.
  base::DictValue input_dict5;
  input_dict5.Set("good_key1", "good_value1");
  std::vector<uint8_t> binary_data = {0, 1, 2};
  input_dict5.Set("bad_key_binary", base::Value(binary_data));
  input_dict5.Set("good_key2", "another_value");

  base::DictValue computed_hashes_for_dict5 =
      hash_store_.ComputeSplitEncryptedHashes(kBasePath, &input_dict5,
                                              test_encryptor_.get());

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
  SeedSplitHmacs(kBasePath, nullptr);
  MakeSuperHmacValid();
  {
    auto tx = BeginTransaction(true);
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(ValueState::UNCHANGED_ENCRYPTED,
              tx->CheckSplitValue(kBasePath, &input_dict5, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

// For PrefHashStoreImpl::ComputeEncryptedHash(..., Dict*, ...)
TEST_F(PrefHashStoreImplEncryptedTest, ComputeEncryptedHash_ForDict_Success) {
  const std::string kPath = "test.dict.pref.compute";
  base::DictValue test_dict;
  test_dict.Set("d_key1", "d_value1");
  test_dict.Set("d_key2", 456);

  // Assuming test_encryptor_instance_ works by default.
  std::string encrypted_hash_str = hash_store_.ComputeEncryptedHash(
      kPath, &test_dict, test_encryptor_.get());
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
  SeedAtomicHmac(kPath, "some_existing_hmac");

  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    tx->StoreEncryptedHash(kPath, &test_value);

    EXPECT_FALSE(
        dictionary_contents_.GetAtomicPrefAuthenticator(kEncKey, nullptr))
        << "Encrypted hash should not be stored.";
    VerifyStoredHashes(kPath, "some_existing_hmac", std::nullopt);
  }
}

// PrefHashStoreTransactionImpl::CheckSplitValueInternal - loop for removed keys
// & no-encryptor fallback block.
TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_EncryptorOn_KeyPresentInStoreMissingInValue) {
  const std::string kPath = "split.eh.key_removed_from_value";
  base::DictValue original_seeded_values;
  original_seeded_values.Set("keyA", "valA");
  original_seeded_values.Set("keyB_in_store_only", "valB");
  SeedSplitEncryptedHashesFromValues(kPath, &original_seeded_values);
  SeedSplitHmacs(kPath, nullptr);

  base::DictValue current_pref_dict;
  current_pref_dict.Set("keyA", "valA");

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  std::vector<std::string> invalid_keys;
  ValueState state =
      tx->CheckSplitValue(kPath, &current_pref_dict, &invalid_keys);

  EXPECT_EQ(ValueState::CHANGED_ENCRYPTED, state);
  ASSERT_EQ(1u, invalid_keys.size());
  EXPECT_EQ("keyB_in_store_only", invalid_keys[0]);
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_NoEncryptor_EHExists_NoMACs_ResultsInUntrusted) {
  const std::string kPath = "split.no_encryptor.only_eh_unusable";
  base::DictValue values_for_eh;
  values_for_eh.Set("key1", "value1");
  SeedSplitEncryptedHashesFromValues(kPath, &values_for_eh);
  SeedSplitHmacs(kPath, nullptr);
  MakeSuperHmacInvalid();

  base::DictValue current_pref_dict = values_for_eh.Clone();

  auto tx = BeginTransaction(/*with_encryptor=*/false);
  std::vector<std::string> invalid_keys;
  ValueState state =
      tx->CheckSplitValue(kPath, &current_pref_dict, &invalid_keys);

  EXPECT_EQ(ValueState::UNTRUSTED_UNKNOWN_VALUE, state);
  EXPECT_TRUE(invalid_keys.empty());
}

TEST_F(PrefHashStoreImplEncryptedTest,
       CheckSplitValue_NoEncryptor_EHAndHmacsExist_UsesHmacs) {
  const std::string kPath = "split.no_encryptor.eh_and_hmacs";
  base::DictValue base_values;
  base_values.Set("key1", "value1");
  base_values.Set("key2", "value2");

  base::DictValue eh_seed_values;
  eh_seed_values.Set("key1", "value1");
  SeedSplitEncryptedHashesFromValues(kPath, &eh_seed_values);

  SeedSplitHmacs(kPath, &base_values);
  MakeSuperHmacInvalid();

  base::DictValue current_pref_dict1 = base_values.Clone();
  {
    auto tx = BeginTransaction(/*with_encryptor=*/false);
    std::vector<std::string> invalid_keys;
    ValueState state =
        tx->CheckSplitValue(kPath, &current_pref_dict1, &invalid_keys);
    EXPECT_EQ(ValueState::UNCHANGED, state)
        << "Should rely on HMACs when encryptor is off";
    EXPECT_TRUE(invalid_keys.empty());
  }

  base::DictValue current_pref_dict2 = base_values.Clone();
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

// For PrefHashStoreTransactionImpl::ImportAuthData
TEST_F(PrefHashStoreImplEncryptedTest,
       ImportAuthData_StringHmac_ClearsOldEH_AndStampsSuperHmacIfValid) {
  const std::string kPath = "import.string.mac.specific";
  SeedAtomicEncryptedHash(kPath, "eh_to_be_cleared_b64");
  MakeSuperHmacValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperHmacValid());

    base::Value hmac_to_import("newly_imported_hmac");
    tx->ImportAuthData(kPath, &hmac_to_import);

    VerifyStoredHashes(kPath, "newly_imported_hmac", std::nullopt);
    EXPECT_TRUE(tx->StampSuperHmac());
    EXPECT_TRUE(tx->IsSuperHmacValid());
  }
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportAuthData_Dict_HmacOnly_ClearsOldEH_AndStampsSuperHmacIfValid) {
  const std::string kPath = "import.dict.hmaconly.specific";
  SeedAtomicEncryptedHash(kPath, "eh_to_be_cleared_b64");
  SeedAtomicHmac(kPath, "old_hmac_to_be_overwritten");
  MakeSuperHmacValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperHmacValid());

    base::DictValue dict_to_import_data;
    dict_to_import_data.Set(kImportHmacKey, "imported_dict_hmac");
    base::Value value_for_import(dict_to_import_data.Clone());
    tx->ImportAuthData(kPath, &value_for_import);

    VerifyStoredHashes(kPath, "imported_dict_hmac", std::nullopt);
    EXPECT_TRUE(tx->StampSuperHmac());
    EXPECT_TRUE(tx->IsSuperHmacValid());
  }
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportAuthData_Dict_EHOnly_ClearsOldHmac_AndStampsSuperHmacIfValid) {
  const std::string kPath = "import.dict.ehonly.specific";
  SeedAtomicHmac(kPath, "hmac_to_be_cleared");
  SeedAtomicEncryptedHash(kPath, "old_eh_to_be_overwritten_b64");
  MakeSuperHmacValid();

  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    ASSERT_TRUE(tx->IsSuperHmacValid());

    base::DictValue dict_to_import_data;
    dict_to_import_data.Set(kImportEncryptedHashKey, "imported_dict_eh_b64");
    base::Value value_for_import(dict_to_import_data.Clone());
    tx->ImportAuthData(kPath, &value_for_import);

    VerifyStoredHashes(kPath, std::nullopt, "imported_dict_eh_b64");
    EXPECT_TRUE(tx->StampSuperHmac());
    EXPECT_TRUE(tx->IsSuperHmacValid());
  }
}

TEST_F(
    PrefHashStoreImplEncryptedTest,
    ImportAuthData_Dict_NoRelevantKeys_NoOldHashes_SuperHmacValid_StampsSuperHmac) {
  const std::string kPath = "import.dict.norelevant.noold";
  dictionary_contents_.RemoveAuthenticator(kPath);
  dictionary_contents_.RemoveAuthenticator(GetEncKey(kPath));
  MakeSuperHmacValid();

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx->IsSuperHmacValid());
  // Initial StampSuperHmac will be true because the transaction's
  // super_hmac_dirty_ is set by constructor if it reads a valid HMAC (or by
  // StampSuperHmac if it had to make it valid). Given the provided
  // StampSuperHmac impl, it will always be true here.
  EXPECT_TRUE(tx->StampSuperHmac())
      << "First stamp will always be true if use_super_hmac is on.";

  base::DictValue dict_to_import_data;
  dict_to_import_data.Set("some_other_key", "some_value");
  base::Value value_for_import(dict_to_import_data.Clone());
  tx->ImportAuthData(kPath, &value_for_import);

  VerifyStoredHashes(kPath, std::nullopt, std::nullopt);

  EXPECT_TRUE(tx->StampSuperHmac())
      << "Importing dict when Super HMAC was valid makes it stampable again.";
  EXPECT_TRUE(tx->IsSuperHmacValid());
}

TEST_F(PrefHashStoreImplEncryptedTest,
       ImportAuthData_OtherType_NoChange_NoStampIfWasClean) {
  const std::string kPath = "import.othertype.nochange";
  SeedAtomicHmac(kPath, "old_hmac_data");
  MakeSuperHmacValid();

  auto tx = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx->IsSuperHmacValid());
  tx->StampSuperHmac();

  base::Value int_value(123);
  tx->ImportAuthData(kPath, &int_value);

  VerifyStoredHashes(kPath, "old_hmac_data", std::nullopt);
  EXPECT_TRUE(tx->StampSuperHmac());  // This will be true with the current
                                      // StampSuperHmac
  EXPECT_TRUE(tx->IsSuperHmacValid());
}

// For PrefHashStoreTransactionImpl::ClearEncryptedHash - private, tested via
// public ClearAuthData.
TEST_F(PrefHashStoreImplEncryptedTest,
       ClearAuthData_TargetingPrivateClearEncryptedHashLogic) {
  const std::string kPath = "clear.eh.via.clearhashes";
  const std::string kEncKey = GetEncKey(kPath);

  // Scenario 1: Both exist, Super HMAC valid
  SeedAtomicEncryptedHash(kPath, "eh_data_b64");
  SeedAtomicHmac(kPath, "hmac_data_also");
  MakeSuperHmacValid();
  auto tx1 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx1->IsSuperHmacValid());

  tx1->ClearAuthenticators(kPath);

  EXPECT_FALSE(dictionary_contents_.GetAtomicPrefAuthenticator(kPath, nullptr));
  EXPECT_FALSE(
      dictionary_contents_.GetAtomicPrefAuthenticator(kEncKey, nullptr));
  EXPECT_TRUE(tx1->StampSuperHmac());
  EXPECT_TRUE(tx1->IsSuperHmacValid());

  // Scenario 2: Both exist, Super HMAC invalid
  SeedAtomicEncryptedHash(kPath, "eh_data_b64_sm_invalid");
  SeedAtomicHmac(kPath, "mac_data_sm_invalid");
  MakeSuperHmacInvalid();
  auto tx2 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_FALSE(tx2->IsSuperHmacValid());

  tx2->ClearAuthenticators(kPath);

  EXPECT_FALSE(
      dictionary_contents_.GetAtomicPrefAuthenticator(kEncKey, nullptr));
  EXPECT_FALSE(dictionary_contents_.GetAtomicPrefAuthenticator(kPath, nullptr));
  EXPECT_TRUE(tx2->StampSuperHmac());
  EXPECT_TRUE(tx2->IsSuperHmacValid());

  // Scenario 3: Neither EH nor HMAC exists, Super HMAC valid
  dictionary_contents_.RemoveAuthenticator(kPath);
  dictionary_contents_.RemoveAuthenticator(GetEncKey(kPath));
  MakeSuperHmacValid();
  auto tx3 = BeginTransaction(/*with_encryptor=*/true);
  ASSERT_TRUE(tx3->IsSuperHmacValid());
  tx3->StampSuperHmac();

  tx3->ClearAuthenticators(kPath);

  // If StampSuperHmac always returns true, we can't differentiate a "no-op
  // clean" from "op dirty". We can only check that contents are still empty.
  VerifyStoredHashes(kPath, std::nullopt, std::nullopt);
  EXPECT_TRUE(tx3->StampSuperHmac());
}

TEST_F(
    PrefHashStoreImplEncryptedTest,
    StoreSplitEncryptedHash_ClearsOldSplitEncHashes_KeepsLegacyHmac_StoresNew) {
  const std::string kPath = "split.clear.old.split.keephmac";
  const std::string kEncKeyForSplitHashes = GetEncKey(kPath);

  // 1. Seed a legacy HMAC for the base path.
  const std::string kLegacyHmacValue = "legacy_hmac_for_split_test";
  SeedAtomicHmac(kPath, kLegacyHmacValue);
  std::string temp_check_val;
  ASSERT_TRUE(
      dictionary_contents_.GetAtomicPrefAuthenticator(kPath, &temp_check_val));
  ASSERT_EQ(kLegacyHmacValue, temp_check_val);

  // 2. Seed old split encrypted hashes.
  base::DictValue old_split_content;
  old_split_content.Set("old_subkey", "old_value");
  old_split_content.Set("common_subkey", "common_value_old_hash");
  base::DictValue old_computed_split_ehs =
      hash_store_.ComputeSplitEncryptedHashes(kPath, &old_split_content,
                                              test_encryptor_.get());
  SeedSplitEncryptedHashes(kPath, &old_computed_split_ehs);

  // Verify old split EHs are present.
  std::map<std::string, std::string> temp_split_ehs;
  ASSERT_TRUE(dictionary_contents_.GetSplitPrefAuthenticators(
      kEncKeyForSplitHashes, &temp_split_ehs));
  ASSERT_TRUE(temp_split_ehs.count("old_subkey"));

  // 3. Prepare new split value.
  base::DictValue new_split_content;
  new_split_content.Set("new_subkey", "new_value");
  new_split_content.Set("common_subkey", "common_value_new_hash");

  // 4. Perform StoreSplitEncryptedHash transaction.
  {
    auto tx = BeginTransaction(/*with_encryptor=*/true);
    tx->StoreSplitEncryptedHash(kPath, &new_split_content);
  }

  // 5. Verify the legacy HMAC is still present.
  EXPECT_TRUE(
      dictionary_contents_.GetAtomicPrefAuthenticator(kPath, &temp_check_val));
  EXPECT_EQ(kLegacyHmacValue, temp_check_val);

  // 6. Verify new split encrypted hashes are stored and old ones are gone.
  std::map<std::string, std::string> stored_split_ehs;
  ASSERT_TRUE(dictionary_contents_.GetSplitPrefAuthenticators(
      kEncKeyForSplitHashes, &stored_split_ehs));
  EXPECT_FALSE(stored_split_ehs.count("old_subkey"));
  EXPECT_TRUE(stored_split_ehs.count("new_subkey"));
  EXPECT_FALSE(stored_split_ehs.at("new_subkey").empty());
  EXPECT_TRUE(stored_split_ehs.count("common_subkey"));
  // common_subkey's hash should have been updated.
  EXPECT_NE(temp_split_ehs.at("common_subkey"),
            stored_split_ehs.at("common_subkey"));
}

#if BUILDFLAG(IS_WIN)
TEST_F(PrefHashStoreImplEncryptedTest,
       CheckValueEnterpriseRoamingHmacOnWindows) {
  const std::string kPath = "enterprise.roaming.pref";
  base::Value value("enterprise_value");

  SeedAtomicHmac(kPath, hash_store_.ComputeHmac(kPath, &value));
  SeedAtomicEncryptedHash(kPath, hash_store_.ComputeEncryptedHash(
                                     kPath, &value, test_encryptor_.get()));
  SeedAtomicHmac(kPath, "invalid_roaming_hmac");

  std::optional<base::AutoReset<bool>> is_enterprise_device_for_testing_ =
      base::SetIsEnterpriseDeviceForTesting(true);

  // Perform CheckValue without the encryptor.
  auto tx = BeginTransaction(/*with_encryptor=*/false);
  ValueState state = tx->CheckValue(kPath, &value);

  // On an enterprise device, the presence of an encrypted hash should cause
  // the transaction to defer validation.
  EXPECT_EQ(ValueState::UNCHANGED, state);

  is_enterprise_device_for_testing_.reset();
}
#endif
