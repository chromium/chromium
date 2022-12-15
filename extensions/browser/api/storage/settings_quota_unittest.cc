// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/value_store/testing_value_store.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// To save typing value_store::ValueStore::DEFAULTS/IGNORE_QUOTA everywhere.
const value_store::ValueStore::WriteOptions DEFAULTS =
    value_store::ValueStore::DEFAULTS;
const value_store::ValueStore::WriteOptions IGNORE_QUOTA =
    value_store::ValueStore::IGNORE_QUOTA;

class ExtensionSettingsQuotaTest : public testing::Test {
 public:
  ExtensionSettingsQuotaTest()
      : byte_value_1_(1),
        byte_value_16_("sixteen bytes."),
        byte_value_256_(base::Value(base::Value::Type::LIST)),
        delegate_(new value_store::TestingValueStore()) {
    for (int i = 1; i < 89; ++i) {
      byte_value_256_.GetList().Append(i);
    }
    ValidateByteValues();
  }

  void ValidateByteValues() {
    std::string validate_sizes;
    base::JSONWriter::Write(byte_value_1_, &validate_sizes);
    ASSERT_EQ(1u, validate_sizes.size());
    base::JSONWriter::Write(byte_value_16_, &validate_sizes);
    ASSERT_EQ(16u, validate_sizes.size());
    base::JSONWriter::Write(byte_value_256_, &validate_sizes);
    ASSERT_EQ(256u, validate_sizes.size());
  }

  void TearDown() override { ASSERT_TRUE(storage_.get() != nullptr); }

 protected:
  // Creates |storage_|.  Must only be called once.
  void CreateStorage(
      size_t quota_bytes, size_t quota_bytes_per_item, size_t max_items) {
    ASSERT_TRUE(storage_.get() == nullptr);
    SettingsStorageQuotaEnforcer::Limits limits =
        { quota_bytes, quota_bytes_per_item, max_items };
    storage_ = std::make_unique<SettingsStorageQuotaEnforcer>(
        limits, base::WrapUnique(delegate_.get()));
  }

  // Returns whether the settings in |storage_| and |delegate_| are the same as
  // |settings|.
  bool SettingsEqual(const base::Value::Dict& settings) {
    return settings == storage_->Get().settings() &&
           settings == delegate_->Get().settings();
  }

  // Values with different serialized sizes.
  base::Value byte_value_1_;
  base::Value byte_value_16_;
  base::Value byte_value_256_;

  // Quota enforcing storage area being tested.
  std::unique_ptr<SettingsStorageQuotaEnforcer> storage_;

  // In-memory storage area being delegated to.  Always owned by |storage_|.
  raw_ptr<value_store::TestingValueStore> delegate_;
};

TEST_F(ExtensionSettingsQuotaTest, ZeroQuotaBytes) {
  base::Value::Dict empty;
  CreateStorage(0, UINT_MAX, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, KeySizeTakenIntoAccount) {
  base::Value::Dict empty;
  CreateStorage(8u, UINT_MAX, UINT_MAX);
  EXPECT_FALSE(
      storage_->Set(DEFAULTS, "Really long key", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, SmallByteQuota) {
  base::Value::Dict settings;
  CreateStorage(8u, UINT_MAX, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, MediumByteQuota) {
  base::Value::Dict settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  base::Value::Dict to_set;
  to_set.Set("a", byte_value_1_.Clone());
  to_set.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set value to other under-quota value.
  to_set.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, ZeroMaxKeys) {
  base::Value::Dict empty;
  CreateStorage(UINT_MAX, UINT_MAX, 0);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, SmallMaxKeys) {
  base::Value::Dict settings;
  CreateStorage(UINT_MAX, UINT_MAX, 1);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing key to other value without going over quota.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, MediumMaxKeys) {
  base::Value::Dict settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  base::Value::Dict to_set;
  to_set.Set("a", byte_value_1_.Clone());
  to_set.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing keys to other values without going over
  // quota.
  to_set.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, RemovingExistingSettings) {
  base::Value::Dict settings;
  CreateStorage(266, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "b", byte_value_16_);
  settings.Set("b", byte_value_16_.Clone());
  // Not enough quota.
  storage_->Set(DEFAULTS, "c", byte_value_256_);
  EXPECT_TRUE(SettingsEqual(settings));

  // Try again with "b" removed, enough quota.
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  settings.Remove("b");
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  settings.Set("c", byte_value_256_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Enough byte quota but max keys not high enough.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Back under max keys.
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  settings.Remove("a");
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  settings.Set("b", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, RemovingNonexistentSettings) {
  base::Value::Dict settings;
  CreateStorage(36, UINT_MAX, 3);

  // Max out bytes.
  base::Value::Dict to_set;
  to_set.Set("b1", byte_value_16_.Clone());
  to_set.Set("b2", byte_value_16_.Clone());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_16_.Clone());
  settings.Set("b2", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  std::vector<std::string> to_remove;
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_TRUE(storage_->Remove(to_remove).status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Max out key count.
  to_set.clear();
  to_set.Set("b1", byte_value_1_.Clone());
  to_set.Set("b2", byte_value_1_.Clone());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_1_.Clone());
  settings.Set("b2", byte_value_1_.Clone());
  storage_->Set(DEFAULTS, "b3", byte_value_1_);
  settings.Set("b3", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  to_remove.clear();
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_TRUE(storage_->Remove(to_remove).status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, Clear) {
  base::Value::Dict settings;
  CreateStorage(40, UINT_MAX, 5);

  // Test running out of byte quota.
  {
    base::Value::Dict to_set;
    to_set.Set("a", byte_value_16_.Clone());
    to_set.Set("b", byte_value_16_.Clone());
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());

    EXPECT_TRUE(storage_->Clear().status().ok());

    // (repeat)
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  }

  // Test reaching max keys.
  storage_->Clear();
  {
    base::Value::Dict to_set;
    to_set.Set("a", byte_value_1_.Clone());
    to_set.Set("b", byte_value_1_.Clone());
    to_set.Set("c", byte_value_1_.Clone());
    to_set.Set("d", byte_value_1_.Clone());
    to_set.Set("e", byte_value_1_.Clone());
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "f", byte_value_1_).status().ok());

    storage_->Clear();

    // (repeat)
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "f", byte_value_1_).status().ok());
  }
}

TEST_F(ExtensionSettingsQuotaTest, ChangingUsedBytesWithSet) {
  base::Value::Dict settings;
  CreateStorage(20, UINT_MAX, UINT_MAX);

  // Change a setting to make it go over quota.
  storage_->Set(DEFAULTS, "a", byte_value_16_);
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Change a setting to reduce usage and room for another setting.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "foobar", byte_value_1_).status().ok());
  storage_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "foobar", byte_value_1_).status().ok());
  settings.Set("foobar", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, SetsOnlyEntirelyCompletedWithByteQuota) {
  base::Value::Dict settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  storage_->Set(DEFAULTS, "a", byte_value_16_);
  settings.Set("a", byte_value_16_.Clone());

  // The entire change is over quota.
  base::Value::Dict to_set;
  to_set.Set("b", byte_value_16_.Clone());
  to_set.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // The entire change is over quota, but quota reduced in existing key.
  to_set.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, SetsOnlyEntireCompletedWithMaxKeys) {
  base::Value::Dict settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());

  base::Value::Dict to_set;
  to_set.Set("b", byte_value_16_.Clone());
  to_set.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, WithInitialDataAndByteQuota) {
  base::Value::Dict settings;
  delegate_->Set(DEFAULTS, "a", byte_value_256_);
  settings.Set("a", byte_value_256_.Clone());

  CreateStorage(280, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  // Add some data.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Not enough quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Reduce usage of original setting so that "c" can fit.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove to free up some more data.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_256_).status().ok());

  std::vector<std::string> to_remove;
  to_remove.push_back("a");
  to_remove.push_back("b");
  storage_->Remove(to_remove);
  settings.Remove("a");
  settings.Remove("b");
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", byte_value_256_).status().ok());
  settings.Set("d", byte_value_256_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, WithInitialDataAndMaxKeys) {
  base::Value::Dict settings;
  delegate_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  settings.Set("b", byte_value_1_.Clone());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_1_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, InitiallyOverByteQuota) {
  base::Value::Dict settings;
  settings.Set("a", byte_value_16_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  delegate_->Set(DEFAULTS, settings);

  CreateStorage(40, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());

  // Take under quota by reducing size of an existing setting
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able set another small setting.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", byte_value_1_).status().ok());
  settings.Set("d", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, InitiallyOverMaxKeys) {
  base::Value::Dict settings;
  settings.Set("a", byte_value_16_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  delegate_->Set(DEFAULTS, settings);

  CreateStorage(UINT_MAX, UINT_MAX, 2);
  EXPECT_TRUE(SettingsEqual(settings));

  // Can't set either an existing or new setting.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able after removing 2.
  storage_->Remove("a");
  settings.Remove("a");
  storage_->Remove("b");
  settings.Remove("b");
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "e", byte_value_1_).status().ok());
  settings.Set("e", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still can't set any.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, ZeroQuotaBytesPerSetting) {
  base::Value::Dict empty;
  CreateStorage(UINT_MAX, 0, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, QuotaBytesPerSetting) {
  base::Value::Dict settings;

  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, QuotaBytesPerSettingWithInitialSettings) {
  base::Value::Dict settings;

  delegate_->Set(DEFAULTS, "a", byte_value_1_);
  delegate_->Set(DEFAULTS, "b", byte_value_16_);
  delegate_->Set(DEFAULTS, "c", byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest,
    QuotaBytesPerSettingWithInitialSettingsForced) {
  // This is a lazy test to make sure IGNORE_QUOTA lets through changes: the
  // test above copied, but using IGNORE_QUOTA and asserting nothing is ever
  // rejected...
  base::Value::Dict settings;

  delegate_->Set(DEFAULTS, "a", byte_value_1_);
  delegate_->Set(DEFAULTS, "b", byte_value_16_);
  delegate_->Set(DEFAULTS, "c", byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_16_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_256_).status().ok());
  settings.Set("a", byte_value_256_.Clone());

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_16_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_256_).status().ok());
  settings.Set("b", byte_value_256_.Clone());

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "c", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());

  // ... except the last.  Make sure it can still fail.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, GetBytesInUse) {
  // Just testing GetBytesInUse, no need for a quota.
  CreateStorage(UINT_MAX, UINT_MAX, UINT_MAX);

  std::vector<std::string> ab;
  ab.push_back("a");
  ab.push_back("b");

  EXPECT_EQ(0u, storage_->GetBytesInUse());
  EXPECT_EQ(0u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(0u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "a", byte_value_1_);

  EXPECT_EQ(2u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(2u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "b", byte_value_1_);

  EXPECT_EQ(4u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "c", byte_value_1_);

  EXPECT_EQ(6u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));
}

}  // namespace extensions
