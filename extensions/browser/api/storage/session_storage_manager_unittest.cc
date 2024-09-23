// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/extensions_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

// The maximum number of bytes that each extension is allowed to store in
// the SessionStorageManager. Currently, this is capped at 1MB. For testing
// purposes, here it will be capped at 1000 bytes.
constexpr int kQuotaBytesPerExtension = 1000;

constexpr char kTestExtensionId1[] = "extension id1";
constexpr char kTestExtensionId2[] = "extension id2";

constexpr char kQuotaBytesExceededError[] =
    "Session storage quota bytes exceeded. Values were not stored.";

using ValueChangeList =
    std::vector<extensions::SessionStorageManager::ValueChange>;
using testing::AllOf;
using testing::Ge;
using testing::Le;

std::unique_ptr<KeyedService> SetTestingSessionStorageManager(
    content::BrowserContext* browser_context) {
  return std::make_unique<extensions::SessionStorageManager>(
      kQuotaBytesPerExtension, browser_context);
}

}  // namespace

namespace extensions {

class SessionStorageManagerUnittest : public ExtensionsTest {
 public:
  SessionStorageManagerUnittest()
      : value_int_(123),
        value_string_("value"),
        value_list_(base::Value::Type::LIST) {
    value_list_.GetList().Append(1);
    value_list_.GetList().Append(2);
    value_dict_.Set("int", 123);
    value_dict_.Set("string", "abc");
  }

 protected:
  // ExtensionsTest:
  void SetUp() override;
  void TearDown() override;

  // Values with different types.
  base::Value value_int_;
  base::Value value_string_;
  base::Value value_list_;
  base::Value::Dict value_dict_;

  // Session storage manager being tested.
  raw_ptr<SessionStorageManager> manager_;
};

void SessionStorageManagerUnittest::SetUp() {
  ExtensionsTest::SetUp();
  manager_ = static_cast<SessionStorageManager*>(
      SessionStorageManager::GetFactory()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&SetTestingSessionStorageManager)));
}

void SessionStorageManagerUnittest::TearDown() {
  manager_ = nullptr;
  ExtensionsTest::TearDown();
}

TEST_F(SessionStorageManagerUnittest, SetGetAndRemoveOneExtensionSuccessful) {
  {
    // Store individual value.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  {
    // Store multiple values.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key2", value_string_.Clone());
    values.emplace("key3", value_list_.Clone());
    values.emplace("key4", value_dict_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  // Retrieve individual values from storage.
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_int_);
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key2"), value_string_);
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key3"), value_list_);
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key4"), value_dict_);

  // Retrieve multiple values from storage.
  std::map<std::string, const base::Value*> multiple_values =
      manager_->Get("extension id1", {"key1", "invalid key", "key4"});
  ASSERT_EQ(multiple_values.size(), 2u);
  EXPECT_EQ(*multiple_values["key1"], value_int_);
  EXPECT_FALSE(base::Contains(multiple_values, "invalid_key"));
  EXPECT_EQ(*multiple_values["key4"], value_dict_);

  // Retrieve all values from storage.
  std::map<std::string, const base::Value*> all_values =
      manager_->GetAll(kTestExtensionId1);
  ASSERT_EQ(all_values.size(), 4u);
  EXPECT_EQ(*all_values["key1"], value_int_);
  EXPECT_EQ(*all_values["key2"], value_string_);
  EXPECT_EQ(*all_values["key3"], value_list_);
  EXPECT_EQ(*all_values["key4"], value_dict_);

  {
    // Remove one value from storage.
    ValueChangeList changes;
    manager_->Remove(kTestExtensionId1, "key1", changes);
    ASSERT_EQ(manager_->Get(kTestExtensionId1, "key1"), nullptr);
  }

  {
    // Remove multiple values from storage.
    ValueChangeList changes;
    manager_->Remove(kTestExtensionId1, {"key2", "key3", "invalid key", "key4"},
                     changes);
    ASSERT_EQ(manager_->Get(kTestExtensionId1, "key2"), nullptr);
    ASSERT_EQ(manager_->Get(kTestExtensionId1, "key3"), nullptr);
    ASSERT_EQ(manager_->Get(kTestExtensionId1, "invalid key"), nullptr);
    ASSERT_EQ(manager_->Get(kTestExtensionId1, "key4"), nullptr);
  }

  {
    // Check that a value can be added after removing previous values.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    values.emplace("key5", value_list_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_string_);
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key5"), value_list_);
  }
}

TEST_F(SessionStorageManagerUnittest, ClearOneExtensionSuccessful) {
  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    values.emplace("key2", value_string_.Clone());
    manager_->Set(kTestExtensionId1, std::move(values), changes, &error);
  }

  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    values.emplace("key2", value_string_.Clone());
    manager_->Set(kTestExtensionId2, std::move(values), changes, &error);
  }

  ValueChangeList remove_changes;
  manager_->Clear(kTestExtensionId1, remove_changes);
  EXPECT_TRUE(manager_->GetAll(kTestExtensionId1).empty());

  // Check kTestExtensionId1 got all its values removed.
  ASSERT_EQ(remove_changes.size(), 2u);

  EXPECT_EQ(remove_changes[0].key, "key1");
  ASSERT_TRUE(remove_changes[0].old_value.has_value());
  EXPECT_EQ(remove_changes[0].old_value.value(), value_int_);
  EXPECT_EQ(remove_changes[0].new_value, nullptr);

  EXPECT_EQ(remove_changes[1].key, "key2");
  ASSERT_TRUE(remove_changes[1].old_value.has_value());
  EXPECT_EQ(remove_changes[1].old_value.value(), value_string_);
  EXPECT_EQ(remove_changes[1].new_value, nullptr);

  // Check kTestExtensionId2 still has all its values.
  ASSERT_EQ(*manager_->Get(kTestExtensionId2, "key1"), value_int_);
  ASSERT_EQ(*manager_->Get(kTestExtensionId2, "key2"), value_string_);
}

TEST_F(SessionStorageManagerUnittest,
       SetGetAndRemovetMultipleExtensionsSuccessful) {
  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId2, std::move(values), changes, &error));
  }

  // Different extensions can have an equal key with different associated
  // values.
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_int_);
  EXPECT_EQ(*manager_->Get(kTestExtensionId2, "key1"), value_string_);
  ValueChangeList changes;
  manager_->Remove(kTestExtensionId1, "key1", changes);
  EXPECT_EQ(manager_->Get(kTestExtensionId1, "key1"), nullptr);
  EXPECT_EQ(*manager_->Get(kTestExtensionId2, "key1"), value_string_);
}

TEST_F(SessionStorageManagerUnittest, ChangeValueOfExistentKeys) {
  {
    // New key and value are stored, and change is added to
    // changes list.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_int_);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].key, "key1");
    EXPECT_FALSE(changes[0].old_value.has_value());
    ASSERT_TRUE(changes[0].new_value);
    EXPECT_EQ(*changes[0].new_value, value_int_);
  }

  {
    // Value pointed by an existing key is changed, and change is added to
    // changes list with old value.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_string_);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].key, "key1");
    ASSERT_TRUE(changes[0].old_value.has_value());
    EXPECT_EQ(changes[0].old_value.value(), value_int_);
    ASSERT_TRUE(changes[0].new_value);
    EXPECT_EQ(*changes[0].new_value, value_string_);
  }

  {
    // Value pointed by an existing key is changed to an equal value, and no
    // change is added to changes list.
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_string_);
    EXPECT_TRUE(changes.empty());
  }

  {
    // Value pointed by an existing key is removed, and change is added to
    // changes list.
    ValueChangeList changes;
    manager_->Remove(kTestExtensionId1, "key1", changes);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].key, "key1");
    ASSERT_TRUE(changes[0].old_value.has_value());
    EXPECT_EQ(changes[0].old_value.value(), value_string_);
    EXPECT_EQ(changes[0].new_value, nullptr);
  }

  {
    // Values pointed by existing keys are removed, and changes are added to
    // changes list.
    ValueChangeList set_changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key2", value_string_.Clone());
    values.emplace("key3", value_list_.Clone());
    manager_->Set(kTestExtensionId1, std::move(values), set_changes, &error);

    ValueChangeList remove_changes;
    std::vector<std::string> keys{"key2", "key3"};
    manager_->Remove(kTestExtensionId1, keys, remove_changes);
    ASSERT_EQ(remove_changes.size(), 2u);
    EXPECT_EQ(remove_changes[0].key, "key2");
    ASSERT_TRUE(remove_changes[0].old_value.has_value());
    EXPECT_EQ(remove_changes[0].old_value.value(), value_string_);
    EXPECT_EQ(remove_changes[0].new_value, nullptr);
    EXPECT_EQ(remove_changes[1].key, "key3");
    ASSERT_TRUE(remove_changes[1].old_value.has_value());
    EXPECT_EQ(remove_changes[1].old_value.value(), value_list_);
    EXPECT_EQ(remove_changes[1].new_value, nullptr);
  }
}

TEST_F(SessionStorageManagerUnittest, SetFailsWhenQuotaIsExceeded) {
  // `value_over_quota` is greater than 120% of the allocated space.
  base::Value value_over_quota(
      std::string(static_cast<int>(kQuotaBytesPerExtension * 1.2), 'a'));

  // Set fails when a value exceeds the quota limit.
  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values_over_quota;
    values_over_quota.emplace("key1", std::move(value_over_quota));
    EXPECT_FALSE(manager_->Set(kTestExtensionId1, std::move(values_over_quota),
                               changes, &error));
    EXPECT_EQ(error, kQuotaBytesExceededError);
  }

  // `large_value` is greater than 50% of the allocated space.
  base::Value large_value(
      std::string(static_cast<int>(kQuotaBytesPerExtension * 0.6), 'a'));

  {
    // Setting `large_value` once should succeed (it's below quota)
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", large_value.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  {
    // Attempting to set `large_value` a second time under a new key should fail
    // (it would exceed quota).
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key2", large_value.Clone());
    EXPECT_FALSE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
    EXPECT_EQ(error, kQuotaBytesExceededError);
    EXPECT_EQ(nullptr, manager_->Get(kTestExtensionId1, "key2"));
  }

  {
    // Setting `large_value` a second time with the same key should succeed,
    // since it's overwriting an existing value (and thus below quota).
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", large_value.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }
}

TEST_F(SessionStorageManagerUnittest, GetEmptyWhenInvalidKey) {
  EXPECT_EQ(manager_->Get("invalid extension id", ""), nullptr);
  EXPECT_TRUE(
      manager_->Get("invalid extension id", std::vector<std::string>(1, ""))
          .empty());
  EXPECT_TRUE(manager_->GetAll("invalid extension id").empty());

  ValueChangeList changes;
  std::string error;
  std::map<std::string, base::Value> values;
  values.emplace("key1", value_int_.Clone());
  ASSERT_TRUE(
      manager_->Set(kTestExtensionId1, std::move(values), changes, &error));

  EXPECT_EQ(manager_->Get(kTestExtensionId1, "invalid key"), nullptr);
  EXPECT_TRUE(
      manager_
          ->Get(kTestExtensionId1, std::vector<std::string>(1, "invalid key"))
          .empty());
}

TEST_F(SessionStorageManagerUnittest, GetBytesInUse) {
  // `value` is a string with 32 bytes in size. Due to reserved space and
  // overhead of members (and also likely dependent on OS/compiler variations),
  // the actual memory usage is >32 bytes. It should be less than 100 bytes,
  // though, so we use this as a benchmark.
  const base::Value value(std::string(32, 'a'));
  // GetBytesInUse includes both the key and the value it stores. The key's size
  // is estimated using short string optimization, which could return a size of
  // 0. Since this test uses short keys, we will use the value size as the size
  // bounds.
  const size_t kOneEntryLowerBound = 32u;
  const size_t kOneEntryUpperBound = 100u;

  EXPECT_EQ(0u, manager_->GetBytesInUse(kTestExtensionId1, "key1"));
  EXPECT_EQ(0u, manager_->GetBytesInUse(kTestExtensionId1, "key2"));
  EXPECT_EQ(0u, manager_->GetBytesInUse(kTestExtensionId1, "key3"));
  EXPECT_EQ(
      0u, manager_->GetBytesInUse(kTestExtensionId1, {"key1", "key2", "key3"}));
  EXPECT_EQ(0u, manager_->GetTotalBytesInUse(kTestExtensionId1));

  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  EXPECT_THAT(manager_->GetBytesInUse(kTestExtensionId1, "key1"),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key2"), 0u);
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key3"), 0u);
  EXPECT_THAT(
      manager_->GetBytesInUse(kTestExtensionId1, {"key1", "key2", "key3"}),
      AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_THAT(manager_->GetTotalBytesInUse(kTestExtensionId1),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));

  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace("key2", value.Clone());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  EXPECT_THAT(manager_->GetBytesInUse(kTestExtensionId1, "key1"),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_THAT(manager_->GetBytesInUse(kTestExtensionId1, "key2"),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key3"), 0u);
  EXPECT_THAT(
      manager_->GetBytesInUse(kTestExtensionId1, {"key1", "key2", "key3"}),
      AllOf(Ge(2u * kOneEntryLowerBound), Le(2u * kOneEntryUpperBound)));
  EXPECT_THAT(
      manager_->GetTotalBytesInUse(kTestExtensionId1),
      AllOf(Ge(2u * kOneEntryLowerBound), Le(2u * kOneEntryUpperBound)));

  {
    ValueChangeList changes;
    manager_->Remove(kTestExtensionId1, "key1", changes);
  }

  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key1"), 0u);
  EXPECT_THAT(manager_->GetBytesInUse(kTestExtensionId1, "key2"),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key3"), 0u);
  EXPECT_THAT(
      manager_->GetBytesInUse(kTestExtensionId1, {"key1", "key2", "key3"}),
      AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));
  EXPECT_THAT(manager_->GetTotalBytesInUse(kTestExtensionId1),
              AllOf(Ge(kOneEntryLowerBound), Le(kOneEntryUpperBound)));

  {
    ValueChangeList changes;
    manager_->Clear(kTestExtensionId1, changes);
  }

  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key1"), 0u);
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key2"), 0u);
  EXPECT_EQ(manager_->GetBytesInUse(kTestExtensionId1, "key3"), 0u);
  EXPECT_EQ(
      manager_->GetBytesInUse(kTestExtensionId1, {"key1", "key2", "key3"}), 0u);
  EXPECT_EQ(manager_->GetTotalBytesInUse(kTestExtensionId1), 0u);

  // The key should also count towards used storage. This ensures that
  // extensions don't game our quota limit by using the key itself to store
  // data.
  const std::string massive_key(500, 'a');
  {
    ValueChangeList changes;
    std::string error;
    std::map<std::string, base::Value> values;
    values.emplace(massive_key, base::Value());
    EXPECT_TRUE(
        manager_->Set(kTestExtensionId1, std::move(values), changes, &error));
  }

  EXPECT_THAT(manager_->GetBytesInUse(kTestExtensionId1, massive_key),
              AllOf(Ge(500u), Le(600u)));
}

}  // namespace extensions
