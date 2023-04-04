// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/account_mapping.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

TEST(AccountMappingTest, SerializeAccountMapping) {
  AccountMapping account_mapping;
  account_mapping.account_id = CoreAccountId::FromGaiaId("acc_id");
  account_mapping.email = "test@example.com";
  account_mapping.access_token = "access_token";
  account_mapping.status = AccountMapping::NEW;
  account_mapping.status_change_timestamp = base::Time();
  account_mapping.last_message_id.clear();

  EXPECT_EQ("test@example.com&new&0", account_mapping.SerializeAsString());

  account_mapping.status = AccountMapping::ADDING;
  account_mapping.status_change_timestamp =
      base::Time::FromInternalValue(1305797421259977LL);
  account_mapping.last_message_id = "last_message_id_1";

  EXPECT_EQ("test@example.com&adding&1305797421259977&last_message_id_1",
            account_mapping.SerializeAsString());

  account_mapping.status = AccountMapping::MAPPED;

  EXPECT_EQ("test@example.com&mapped&1305797421259977&last_message_id_1",
            account_mapping.SerializeAsString());

  account_mapping.last_message_id.clear();

  EXPECT_EQ("test@example.com&mapped&1305797421259977",
            account_mapping.SerializeAsString());

  account_mapping.account_id = CoreAccountId::FromGaiaId("acc_id2");
  account_mapping.email = "test@gmail.com";
  account_mapping.access_token = "access_token";  // should be ignored.
  account_mapping.status = AccountMapping::REMOVING;

  // Serialize removing message without a message_id.
  EXPECT_EQ("test@gmail.com&removing&1305797421259977",
            account_mapping.SerializeAsString());

  // Add a message ID and serialize again.
  account_mapping.last_message_id = "last_message_id_2";

  EXPECT_EQ("test@gmail.com&removing&1305797421259977&last_message_id_2",
            account_mapping.SerializeAsString());
}

TEST(AccountMappingTest, DeserializeAccountMapping) {
  AccountMapping account_mapping;
  account_mapping.account_id = CoreAccountId::FromGaiaId("acc_id");

  EXPECT_TRUE(account_mapping.ParseFromString("test@example.com&new&0"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@example.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::NEW, account_mapping.status);
  EXPECT_EQ(base::Time(), account_mapping.status_change_timestamp);
  EXPECT_TRUE(account_mapping.last_message_id.empty());

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&adding&1305797421259977&last_message_id_1"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::ADDING, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_EQ("last_message_id_1", account_mapping.last_message_id);

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@example.com&mapped&1305797421259977"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@example.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::MAPPED, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_TRUE(account_mapping.last_message_id.empty());

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&mapped&1305797421259977&last_message_id_1"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::MAPPED, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_EQ("last_message_id_1", account_mapping.last_message_id);

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&removing&1305797421259977&last_message_id_2"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259977LL),
            account_mapping.status_change_timestamp);
  EXPECT_EQ("last_message_id_2", account_mapping.last_message_id);

  EXPECT_TRUE(account_mapping.ParseFromString(
      "test@gmail.com&removing&1305797421259935"));
  EXPECT_EQ("acc_id", account_mapping.account_id.ToString());
  EXPECT_EQ("test@gmail.com", account_mapping.email);
  EXPECT_TRUE(account_mapping.access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, account_mapping.status);
  EXPECT_EQ(base::Time::FromInternalValue(1305797421259935LL),
            account_mapping.status_change_timestamp);
  EXPECT_TRUE(account_mapping.last_message_id.empty());
}

TEST(AccountMappingTest, DeserializeAccountMappingInvalidInput) {
  AccountMapping account_mapping;
  account_mapping.account_id = CoreAccountId::FromGaiaId("acc_id");
  // Too many arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&adding&1305797421259935&last_message_id_1&stuff_here"));
  // Too few arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&adding&1305797421259935"));
  // Too few arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&new"));
  // Too few arguments.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&mapped"));
  // Missing email.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "&remove&1305797421259935&last_message_id_2"));
  // Missing mapping status.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&&1305797421259935&last_message_id_2"));
  // Unkown mapping status.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@example.com&random&1305797421259935&last_message_id_2"));
  // Missing mapping status change timestamp.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@gmail.com&removing&&last_message_id_2"));
  // Last mapping status change timestamp not parseable.
  EXPECT_FALSE(account_mapping.ParseFromString(
      "test@gmail.com&removing&asdfjkl&last_message_id_2"));
}

}  // namespace
}  // namespace gcm
