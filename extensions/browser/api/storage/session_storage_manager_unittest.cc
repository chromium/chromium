// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The maximum number of bytes that each extension is allowed to store in
// the SessionStorageManager. Currently, this is capped at 1MB. For testing
// purposes, here it will be capped at 1000 bytes.
constexpr int kQuotaBytesPerExtension = 1000;

constexpr char kTestExtensionId1[] = "extension id1";
constexpr char kTestExtensionId2[] = "extension id2";

using ValueChangeList =
    std::vector<extensions::SessionStorageManager::ValueChange>;

}  // namespace

namespace extensions {

class SessionStorageManagerUnittest : public testing::Test {
 public:
  SessionStorageManagerUnittest()
      : value_int_(123),
        value_string_("value"),
        value_list_(base::Value::Type::LIST),
        value_dict_(base::Value::Type::DICTIONARY) {
    value_list_.Append(1);
    value_list_.Append(2);
    value_dict_.SetIntKey("int", 123);
    value_dict_.SetStringKey("string", "abc");

    manager_ = std::make_unique<SessionStorageManager>(kQuotaBytesPerExtension);
  }

 protected:
  // Values with different types.
  base::Value value_int_;
  base::Value value_string_;
  base::Value value_list_;
  base::Value value_dict_;

  // Session storage manager being tested.
  std::unique_ptr<SessionStorageManager> manager_;
};

TEST_F(SessionStorageManagerUnittest, SetAndGetOneExtensionSuccessful) {
  {
    // Store individual value.
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
  }

  {
    // Store multiple values.
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key2", value_string_.Clone());
    values.emplace("key3", value_list_.Clone());
    values.emplace("key4", value_dict_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
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
}

TEST_F(SessionStorageManagerUnittest, SetAndGetMultipleExtensionsSuccessful) {
  {
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
  }

  {
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId2, std::move(values), changes));
  }

  // Different extensions can have an equal key with different associated
  // values.
  EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_int_);
  EXPECT_EQ(*manager_->Get(kTestExtensionId2, "key1"), value_string_);
}

TEST_F(SessionStorageManagerUnittest, ChangeValueOfExistentKey) {
  {
    // New key and value are stored, and change is added to
    // changes list.
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_int_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
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
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
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
    std::map<std::string, base::Value> values;
    values.emplace("key1", value_string_.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
    EXPECT_EQ(*manager_->Get(kTestExtensionId1, "key1"), value_string_);
    EXPECT_TRUE(changes.empty());
  }
}

TEST_F(SessionStorageManagerUnittest, SetFailsWhenQuotaIsExceeded) {
  // `value_over_quota` is greater than 120% of the allocated space.
  base::Value value_over_quota(
      std::string(static_cast<int>(kQuotaBytesPerExtension * 1.2), 'a'));

  // Set fails when a value exceeds the quota limit.
  {
    ValueChangeList changes;
    std::map<std::string, base::Value> values_over_quota;
    values_over_quota.emplace("key1", std::move(value_over_quota));
    EXPECT_FALSE(manager_->Set(kTestExtensionId1, std::move(values_over_quota),
                               changes));
  }

  // `large_value` is greater than 50% of the allocated space.
  base::Value large_value(
      std::string(static_cast<int>(kQuotaBytesPerExtension * 0.6), 'a'));

  {
    // Setting `large_value` once should succeed (it's below quota)
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", large_value.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
  }

  {
    // Attempting to set `large_value` a second time under a new key should fail
    // (it would exceed quota).
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key2", large_value.Clone());
    EXPECT_FALSE(manager_->Set(kTestExtensionId1, std::move(values), changes));
    EXPECT_EQ(nullptr, manager_->Get(kTestExtensionId1, "key2"));
  }

  {
    // Setting `large_value` a second time with the same key should succeed,
    // since it's overwriting an existing value (and thus below quota).
    ValueChangeList changes;
    std::map<std::string, base::Value> values;
    values.emplace("key1", large_value.Clone());
    EXPECT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));
  }
}

TEST_F(SessionStorageManagerUnittest, GetEmptyWhenInvalidKey) {
  EXPECT_EQ(manager_->Get("invalid extension id", ""), nullptr);
  EXPECT_TRUE(
      manager_->Get("invalid extension id", std::vector<std::string>(1, ""))
          .empty());
  EXPECT_TRUE(manager_->GetAll("invalid extension id").empty());

  ValueChangeList changes;
  std::map<std::string, base::Value> values;
  values.emplace("key1", value_int_.Clone());
  ASSERT_TRUE(manager_->Set(kTestExtensionId1, std::move(values), changes));

  EXPECT_EQ(manager_->Get(kTestExtensionId1, "invalid key"), nullptr);
  EXPECT_TRUE(
      manager_
          ->Get(kTestExtensionId1, std::vector<std::string>(1, "invalid key"))
          .empty());
}

}  // namespace extensions
