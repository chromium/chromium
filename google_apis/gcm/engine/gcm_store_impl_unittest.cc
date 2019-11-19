// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_store_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Number of persistent ids to use in tests.
const int kNumPersistentIds = 10;

// Number of per-app messages in tests.
const int kNumMessagesPerApp = 20;

// App name for testing.
const char kAppName[] = "my_app";
const char kAppName2[] = "my_app_2";

// Category name for testing.
const char kCategoryName[] = "my_category";

const uint64_t kDeviceId = 22;
const uint64_t kDeviceToken = 55;

class GCMStoreImplTest : public testing::Test {
 public:
  GCMStoreImplTest();
  ~GCMStoreImplTest() override;

  std::unique_ptr<GCMStoreImpl> BuildGCMStore();
  void LoadGCMStore(GCMStoreImpl* gcm_store,
                    std::unique_ptr<GCMStore::LoadResult>* result_dst);

  std::string GetNextPersistentId();

  void PumpLoop();

  void LoadCallback(std::unique_ptr<GCMStore::LoadResult>* result_dst,
                    std::unique_ptr<GCMStore::LoadResult> result);
  void LoadWithoutCheckCallback(
      std::unique_ptr<GCMStore::LoadResult>* result_dst,
      std::unique_ptr<GCMStore::LoadResult> result);
  void UpdateCallback(bool success);

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_directory_;
  base::FilePath store_path_;
  bool expected_success_;
  uint64_t next_persistent_id_;
};

GCMStoreImplTest::GCMStoreImplTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      expected_success_(true),
      next_persistent_id_(base::Time::Now().ToInternalValue()) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

GCMStoreImplTest::~GCMStoreImplTest() {}

std::unique_ptr<GCMStoreImpl> GCMStoreImplTest::BuildGCMStore() {
  // Pass an non-existent directory as store path to match the exact behavior in
  // the production code. Currently GCMStoreImpl checks if the directory exists
  // and contains a CURRENT file to determine the store existence.
  store_path_ =
      temp_directory_.GetPath().Append(FILE_PATH_LITERAL("GCM Store"));
  return std::unique_ptr<GCMStoreImpl>(
      new GCMStoreImpl(store_path_, task_runner_,
                       base::WrapUnique<Encryptor>(new FakeEncryptor)));
}

void GCMStoreImplTest::LoadGCMStore(
    GCMStoreImpl* gcm_store,
    std::unique_ptr<GCMStore::LoadResult>* result_dst) {
  gcm_store->Load(
      GCMStore::CREATE_IF_MISSING,
      base::Bind(&GCMStoreImplTest::LoadCallback,
                 base::Unretained(this),
                 result_dst));
  PumpLoop();
}

std::string GCMStoreImplTest::GetNextPersistentId() {
  return base::NumberToString(next_persistent_id_++);
}

void GCMStoreImplTest::PumpLoop() { task_runner_->RunUntilIdle(); }

void GCMStoreImplTest::LoadCallback(
    std::unique_ptr<GCMStore::LoadResult>* result_dst,
    std::unique_ptr<GCMStore::LoadResult> result) {
  ASSERT_TRUE(result->success);
  LoadWithoutCheckCallback(result_dst, std::move(result));
}

void GCMStoreImplTest::LoadWithoutCheckCallback(
    std::unique_ptr<GCMStore::LoadResult>* result_dst,
    std::unique_ptr<GCMStore::LoadResult> result) {
  *result_dst = std::move(result);
}

void GCMStoreImplTest::UpdateCallback(bool success) {
  ASSERT_EQ(expected_success_, success);
}

// Verify creating a new database and loading it.
TEST_F(GCMStoreImplTest, LoadNew) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  EXPECT_EQ(0U, load_result->device_android_id);
  EXPECT_EQ(0U, load_result->device_security_token);
  EXPECT_TRUE(load_result->incoming_messages.empty());
  EXPECT_TRUE(load_result->outgoing_messages.empty());
  EXPECT_TRUE(load_result->gservices_settings.empty());
  EXPECT_EQ(base::Time::FromInternalValue(0LL), load_result->last_checkin_time);
}

// Verify new database is not created when DO_NOT_CREATE is passed.
TEST_F(GCMStoreImplTest, LoadWithoutCreatingNewStore) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(GCMStore::DO_NOT_CREATE,
                  base::Bind(&GCMStoreImplTest::LoadWithoutCheckCallback,
                             base::Unretained(this), &load_result));
  PumpLoop();

  EXPECT_FALSE(load_result->success);
  EXPECT_TRUE(load_result->store_does_not_exist);
}

// Verifies that loads with DO_NOT_CREATE set store_does_not_exist to true when
// an empty directory was left behind after destroying the database.
TEST_F(GCMStoreImplTest, LoadWithEmptyDirectory) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());

  // Create an empty directory at the store path, to simulate an empty directory
  // being left behind after destroying a previous store.
  ASSERT_TRUE(base::CreateDirectory(store_path_));

  std::unique_ptr<GCMStore::LoadResult> load_result;
  gcm_store->Load(GCMStore::DO_NOT_CREATE,
                  base::Bind(&GCMStoreImplTest::LoadWithoutCheckCallback,
                             base::Unretained(this), &load_result));
  PumpLoop();

  EXPECT_FALSE(load_result->success);
  EXPECT_TRUE(load_result->store_does_not_exist);
}

TEST_F(GCMStoreImplTest, DeviceCredentials) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  gcm_store->SetDeviceCredentials(
      kDeviceId,
      kDeviceToken,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(kDeviceId, load_result->device_android_id);
  ASSERT_EQ(kDeviceToken, load_result->device_security_token);
}

TEST_F(GCMStoreImplTest, LastCheckinInfo) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  base::Time last_checkin_time = base::Time::Now();
  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  accounts.insert("test_user2@gmail.com");

  gcm_store->SetLastCheckinInfo(
      last_checkin_time,
      accounts,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);
  ASSERT_EQ(last_checkin_time, load_result->last_checkin_time);
  ASSERT_EQ(accounts, load_result->last_checkin_accounts);

  // Negative cases, where the value read is gibberish.
  gcm_store->SetValueForTesting(
      "last_checkin_time",
      "gibberish",
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);
  EXPECT_EQ(base::Time(), load_result->last_checkin_time);
}

TEST_F(GCMStoreImplTest, GServicesSettings_ProtocolV2) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = "12345";
  settings["mcs_port"] = "438";
  settings["checkin_url"] = "http://checkin.google.com";
  std::string digest = "digest1";

  gcm_store->SetGServicesSettings(
      settings,
      digest,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(settings, load_result->gservices_settings);
  ASSERT_EQ(digest, load_result->gservices_digest);

  // Remove some, and add some.
  settings.clear();
  settings["checkin_interval"] = "54321";
  settings["registration_url"] = "http://registration.google.com";
  digest = "digest2";

  gcm_store->SetGServicesSettings(
      settings,
      digest,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(settings, load_result->gservices_settings);
  ASSERT_EQ(digest, load_result->gservices_digest);
}

TEST_F(GCMStoreImplTest, Registrations) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  // Add one registration with one sender.
  std::string registration = "sender1=registration1";
  gcm_store->AddRegistration(
      kAppName,
      registration,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  // Add one registration with multiple senders.
  std::string registration2 = "sender1,sender2=registration2";
  gcm_store->AddRegistration(
      kAppName2,
      registration2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(2u, load_result->registrations.size());
  ASSERT_TRUE(load_result->registrations.find(kAppName) !=
              load_result->registrations.end());
  EXPECT_EQ(registration, load_result->registrations[kAppName]);
  ASSERT_TRUE(load_result->registrations.find(kAppName2) !=
              load_result->registrations.end());
  EXPECT_EQ(registration2, load_result->registrations[kAppName2]);

  gcm_store->RemoveRegistration(
      kAppName2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(1u, load_result->registrations.size());
  ASSERT_TRUE(load_result->registrations.find(kAppName) !=
              load_result->registrations.end());
  EXPECT_EQ(registration, load_result->registrations[kAppName]);
}

// Verify saving some incoming messages, reopening the directory, and then
// removing those incoming messages.
TEST_F(GCMStoreImplTest, IncomingMessages) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::vector<std::string> persistent_ids;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    gcm_store->AddIncomingMessage(
        persistent_ids.back(),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(persistent_ids, load_result->incoming_messages);
  ASSERT_TRUE(load_result->outgoing_messages.empty());

  gcm_store->RemoveIncomingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  load_result->incoming_messages.clear();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Verify saving some outgoing messages, reopening the directory, and then
// removing those outgoing messages.
TEST_F(GCMStoreImplTest, OutgoingMessages) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName + persistent_ids.back());
    message.set_category(kCategoryName + persistent_ids.back());
    gcm_store->AddOutgoingMessage(
        persistent_ids.back(),
        MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_EQ(load_result->outgoing_messages.size(), persistent_ids.size());
  for (int i = 0; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result->outgoing_messages[id].get());
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            load_result->outgoing_messages[id].get());
    ASSERT_EQ(message->from(), kAppName + id);
    ASSERT_EQ(message->category(), kCategoryName + id);
  }

  gcm_store->RemoveOutgoingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  load_result->outgoing_messages.clear();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Verify incoming and outgoing messages don't conflict.
TEST_F(GCMStoreImplTest, IncomingAndOutgoingMessages) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::vector<std::string> persistent_ids;
  const int kNumPersistentIds = 10;
  for (int i = 0; i < kNumPersistentIds; ++i) {
    persistent_ids.push_back(GetNextPersistentId());
    gcm_store->AddIncomingMessage(
        persistent_ids.back(),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();

    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName + persistent_ids.back());
    message.set_category(kCategoryName + persistent_ids.back());
    gcm_store->AddOutgoingMessage(
        persistent_ids.back(),
        MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(persistent_ids, load_result->incoming_messages);
  ASSERT_EQ(load_result->outgoing_messages.size(), persistent_ids.size());
  for (int i = 0; i < kNumPersistentIds; ++i) {
    std::string id = persistent_ids[i];
    ASSERT_TRUE(load_result->outgoing_messages[id].get());
    const mcs_proto::DataMessageStanza* message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            load_result->outgoing_messages[id].get());
    ASSERT_EQ(message->from(), kAppName + id);
    ASSERT_EQ(message->category(), kCategoryName + id);
  }

  gcm_store->RemoveIncomingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();
  gcm_store->RemoveOutgoingMessages(
      persistent_ids,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  load_result->incoming_messages.clear();
  load_result->outgoing_messages.clear();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_TRUE(load_result->incoming_messages.empty());
  ASSERT_TRUE(load_result->outgoing_messages.empty());
}

// Test that per-app message limits are enforced, persisted across restarts,
// and updated as messages are removed.
TEST_F(GCMStoreImplTest, PerAppMessageLimits) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  // Add the initial (below app limit) messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
        base::NumberToString(i), MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this))));
    PumpLoop();
  }

  // Attempting to add some more should fail.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_FALSE(gcm_store->AddOutgoingMessage(
        base::NumberToString(i + kNumMessagesPerApp), MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this))));
    PumpLoop();
  }

  // Tear down and restore the database.
  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  // Adding more messages should still fail.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_FALSE(gcm_store->AddOutgoingMessage(
        base::NumberToString(i + kNumMessagesPerApp), MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this))));
    PumpLoop();
  }

  // Remove the existing messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    gcm_store->RemoveOutgoingMessage(
        base::NumberToString(i),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
    PumpLoop();
  }

  // Successfully add new messages.
  for (int i = 0; i < kNumMessagesPerApp; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
        base::NumberToString(i + kNumMessagesPerApp), MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this))));
    PumpLoop();
  }
}

TEST_F(GCMStoreImplTest, AccountMapping) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  // Add account mappings.
  AccountMapping account_mapping1;
  account_mapping1.account_id = CoreAccountId("account_id_1");
  account_mapping1.email = "account_id_1@gmail.com";
  account_mapping1.access_token = "account_token1";
  account_mapping1.status = AccountMapping::ADDING;
  account_mapping1.status_change_timestamp = base::Time();
  account_mapping1.last_message_id = "message_1";

  AccountMapping account_mapping2;
  account_mapping2.account_id = CoreAccountId("account_id_2");
  account_mapping2.email = "account_id_2@gmail.com";
  account_mapping2.access_token = "account_token1";
  account_mapping2.status = AccountMapping::REMOVING;
  account_mapping2.status_change_timestamp =
      base::Time::FromInternalValue(1305734521259935LL);
  account_mapping2.last_message_id = "message_2";

  gcm_store->AddAccountMapping(
      account_mapping1,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();
  gcm_store->AddAccountMapping(
      account_mapping2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  EXPECT_EQ(2UL, load_result->account_mappings.size());
  GCMStore::AccountMappings::iterator iter =
      load_result->account_mappings.begin();
  EXPECT_EQ(account_mapping1.account_id, iter->account_id);
  EXPECT_EQ(account_mapping1.email, iter->email);
  EXPECT_TRUE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::ADDING, iter->status);
  EXPECT_EQ(account_mapping1.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_EQ(account_mapping1.last_message_id, iter->last_message_id);
  ++iter;
  EXPECT_EQ(account_mapping2.account_id, iter->account_id);
  EXPECT_EQ(account_mapping2.email, iter->email);
  EXPECT_TRUE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_id, iter->last_message_id);

  gcm_store->RemoveAccountMapping(
      account_mapping1.account_id,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  EXPECT_EQ(1UL, load_result->account_mappings.size());
  iter = load_result->account_mappings.begin();
  EXPECT_EQ(account_mapping2.account_id, iter->account_id);
  EXPECT_EQ(account_mapping2.email, iter->email);
  EXPECT_TRUE(iter->access_token.empty());
  EXPECT_EQ(AccountMapping::REMOVING, iter->status);
  EXPECT_EQ(account_mapping2.status_change_timestamp,
            iter->status_change_timestamp);
  EXPECT_EQ(account_mapping2.last_message_id, iter->last_message_id);
}

TEST_F(GCMStoreImplTest, HeartbeatInterval) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::string scope1 = "scope1";
  std::string scope2 = "scope2";
  int heartbeat1 = 120 * 1000;
  int heartbeat2 = 360 * 1000;

  gcm_store->AddHeartbeatInterval(
      scope1,
      heartbeat1,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();
  gcm_store->AddHeartbeatInterval(
      scope2,
      heartbeat2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  EXPECT_EQ(2UL, load_result->heartbeat_intervals.size());
  ASSERT_TRUE(load_result->heartbeat_intervals.find(scope1) !=
              load_result->heartbeat_intervals.end());
  EXPECT_EQ(heartbeat1, load_result->heartbeat_intervals[scope1]);
  ASSERT_TRUE(load_result->heartbeat_intervals.find(scope2) !=
              load_result->heartbeat_intervals.end());
  EXPECT_EQ(heartbeat2, load_result->heartbeat_intervals[scope2]);

  gcm_store->RemoveHeartbeatInterval(
      scope2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  EXPECT_EQ(1UL, load_result->heartbeat_intervals.size());
  ASSERT_TRUE(load_result->heartbeat_intervals.find(scope1) !=
              load_result->heartbeat_intervals.end());
  EXPECT_EQ(heartbeat1, load_result->heartbeat_intervals[scope1]);
}

// When the database is destroyed, all database updates should fail. At the
// same time, they per-app message counts should not go up, as failures should
// result in decrementing the counts.
TEST_F(GCMStoreImplTest, AddMessageAfterDestroy) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);
  gcm_store->Destroy(base::Bind(&GCMStoreImplTest::UpdateCallback,
                               base::Unretained(this)));
  PumpLoop();

  expected_success_ = false;
  for (int i = 0; i < kNumMessagesPerApp * 2; ++i) {
    mcs_proto::DataMessageStanza message;
    message.set_from(kAppName);
    message.set_category(kCategoryName);
    // Because all adds are failing, none should hit the per-app message limits.
    EXPECT_TRUE(gcm_store->AddOutgoingMessage(
        base::NumberToString(i), MCSMessage(message),
        base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this))));
    PumpLoop();
  }
}

TEST_F(GCMStoreImplTest, ReloadAfterClose) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  gcm_store->Close();
  PumpLoop();

  LoadGCMStore(gcm_store.get(), &load_result);
}

TEST_F(GCMStoreImplTest, LastTokenFetchTime) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);
  EXPECT_EQ(base::Time(), load_result->last_token_fetch_time);

  base::Time last_token_fetch_time = base::Time::Now();
  gcm_store->SetLastTokenFetchTime(
      last_token_fetch_time,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);
  EXPECT_EQ(last_token_fetch_time, load_result->last_token_fetch_time);

  // Negative cases, where the value read is gibberish.
  gcm_store->SetValueForTesting(
      "last_token_fetch_time",
      "gibberish",
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);
  EXPECT_EQ(base::Time(), load_result->last_token_fetch_time);
}

TEST_F(GCMStoreImplTest, InstanceIDData) {
  std::unique_ptr<GCMStoreImpl> gcm_store(BuildGCMStore());
  std::unique_ptr<GCMStore::LoadResult> load_result;
  LoadGCMStore(gcm_store.get(), &load_result);

  std::string instance_id_data("Foo");
  gcm_store->AddInstanceIDData(
      kAppName,
      instance_id_data,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  std::string instance_id_data2("Hello Instance ID");
  gcm_store->AddInstanceIDData(
      kAppName2,
      instance_id_data2,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(2u, load_result->instance_id_data.size());
  ASSERT_TRUE(load_result->instance_id_data.find(kAppName) !=
              load_result->instance_id_data.end());
  ASSERT_TRUE(load_result->instance_id_data.find(kAppName2) !=
              load_result->instance_id_data.end());
  EXPECT_EQ(instance_id_data, load_result->instance_id_data[kAppName]);
  EXPECT_EQ(instance_id_data2, load_result->instance_id_data[kAppName2]);

  gcm_store->RemoveInstanceIDData(
      kAppName,
      base::Bind(&GCMStoreImplTest::UpdateCallback, base::Unretained(this)));
  PumpLoop();

  gcm_store = BuildGCMStore();
  LoadGCMStore(gcm_store.get(), &load_result);

  ASSERT_EQ(1u, load_result->instance_id_data.size());
  ASSERT_TRUE(load_result->instance_id_data.find(kAppName2) !=
              load_result->instance_id_data.end());
  EXPECT_EQ(instance_id_data2, load_result->instance_id_data[kAppName2]);
}

}  // namespace

}  // namespace gcm
