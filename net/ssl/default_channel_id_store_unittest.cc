// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_channel_id_store.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/test/channel_id_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

void CallCounter(int* counter) {
  (*counter)++;
}

void GetChannelIDCallbackNotCalled(
    int err,
    const std::string& server_identifier,
    std::unique_ptr<crypto::ECPrivateKey> key_result) {
  ADD_FAILURE() << "Unexpected callback execution.";
}

class AsyncGetChannelIDHelper {
 public:
  AsyncGetChannelIDHelper() : called_(false) {}

  void Callback(int err,
                const std::string& server_identifier,
                std::unique_ptr<crypto::ECPrivateKey> key_result) {
    err_ = err;
    server_identifier_ = server_identifier;
    key_ = std::move(key_result);
    called_ = true;
  }

  int err_;
  std::string server_identifier_;
  std::unique_ptr<crypto::ECPrivateKey> key_;
  bool called_;
};

void GetAllCallback(
    ChannelIDStore::ChannelIDList* dest,
    const ChannelIDStore::ChannelIDList& result) {
  *dest = result;
}

class MockPersistentStore
    : public DefaultChannelIDStore::PersistentStore {
 public:
  MockPersistentStore();

  // DefaultChannelIDStore::PersistentStore implementation.
  void Load(const LoadedCallback& loaded_callback) override;
  void AddChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) override;
  void DeleteChannelID(
      const DefaultChannelIDStore::ChannelID& channel_id) override;
  void SetForceKeepSessionState() override;
  void Flush() override;

 protected:
  ~MockPersistentStore() override;

 private:
  typedef std::map<std::string, DefaultChannelIDStore::ChannelID>
      ChannelIDMap;

  ChannelIDMap channel_ids_;
};

MockPersistentStore::MockPersistentStore() = default;

void MockPersistentStore::Load(const LoadedCallback& loaded_callback) {
  std::unique_ptr<
      std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>>
      channel_ids(
          new std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>());
  ChannelIDMap::iterator it;

  for (it = channel_ids_.begin(); it != channel_ids_.end(); ++it) {
    channel_ids->push_back(
        std::make_unique<DefaultChannelIDStore::ChannelID>(it->second));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(loaded_callback, base::Passed(&channel_ids)));
}

void MockPersistentStore::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  channel_ids_[channel_id.server_identifier()] = channel_id;
}

void MockPersistentStore::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  channel_ids_.erase(channel_id.server_identifier());
}

void MockPersistentStore::SetForceKeepSessionState() {}

void MockPersistentStore::Flush() {}

MockPersistentStore::~MockPersistentStore() = default;

bool DomainEquals(const std::string& domain1, const std::string& domain2) {
  return domain1 == domain2;
}

bool DomainNotEquals(const std::string& domain1, const std::string& domain2) {
  return !DomainEquals(domain1, domain2);
}

}  // namespace

using DefaultChannelIDStoreTest = TestWithScopedTaskEnvironment;

TEST_F(DefaultChannelIDStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddChannelID(DefaultChannelIDStore::ChannelID(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));
  persistent_store->AddChannelID(DefaultChannelIDStore::ChannelID(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));

  // Make sure channel_ids load properly.
  DefaultChannelIDStore store(persistent_store.get());
  // Load has not occurred yet.
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Wait for load & queued set task.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, store.GetChannelIDCount());
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "twitter.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Set should be synchronous now that load is done.
  EXPECT_EQ(3, store.GetChannelIDCount());
}

//TODO(mattm): add more tests of without a persistent store?
TEST_F(DefaultChannelIDStoreTest, TestSettingAndGetting) {
  // No persistent store, all calls will be synchronous.
  DefaultChannelIDStore store(NULL);
  std::unique_ptr<crypto::ECPrivateKey> expected_key(
      crypto::ECPrivateKey::Create());

  std::unique_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_FALSE(key);
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time::FromInternalValue(123),
      expected_key->Copy()));
  EXPECT_EQ(OK, store.GetChannelID("verisign.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_TRUE(KeysEqual(expected_key.get(), key.get()));
}

TEST_F(DefaultChannelIDStoreTest, TestDuplicateChannelIds) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());
  std::unique_ptr<crypto::ECPrivateKey> expected_key(
      crypto::ECPrivateKey::Create());

  std::unique_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time::FromInternalValue(123),
      crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time::FromInternalValue(456),
      expected_key->Copy()));

  // Wait for load & queued set tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(OK, store.GetChannelID("verisign.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_TRUE(KeysEqual(expected_key.get(), key.get()));
}

TEST_F(DefaultChannelIDStoreTest, TestAsyncGet) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  std::unique_ptr<crypto::ECPrivateKey> expected_key(
      crypto::ECPrivateKey::Create());
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "verisign.com", base::Time::FromInternalValue(123),
      expected_key->Copy()));

  DefaultChannelIDStore store(persistent_store.get());
  AsyncGetChannelIDHelper helper;
  std::unique_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&helper))));

  // Wait for load & queued get tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_FALSE(key);
  EXPECT_TRUE(helper.called_);
  EXPECT_THAT(helper.err_, IsOk());
  EXPECT_EQ("verisign.com", helper.server_identifier_);
  EXPECT_TRUE(KeysEqual(expected_key.get(), helper.key_.get()));
}

TEST_F(DefaultChannelIDStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "harvard.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Wait for load & queued set tasks.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, store.GetChannelIDCount());
  int delete_finished = 0;
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(0, store.GetChannelIDCount());
}

TEST_F(DefaultChannelIDStoreTest, TestDeleteForDomains) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "harvard.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Wait for load & queued set tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, store.GetChannelIDCount());

  // Whitelist deletion.
  int deletions_finished = 0;
  store.DeleteForDomainsCreatedBetween(
      base::Bind(&DomainEquals, base::ConstRef(std::string("verisign.com"))),
      base::Time(), base::Time(),
      base::Bind(&CallCounter, &deletions_finished));
  ASSERT_EQ(1, deletions_finished);
  EXPECT_EQ(2, store.GetChannelIDCount());
  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  EXPECT_EQ("google.com", channel_ids.begin()->server_identifier());
  EXPECT_EQ("harvard.com", channel_ids.back().server_identifier());

  // Blacklist deletion.
  store.DeleteForDomainsCreatedBetween(
      base::Bind(&DomainNotEquals, base::ConstRef(std::string("google.com"))),
      base::Time(), base::Time(),
      base::Bind(&CallCounter, &deletions_finished));
  ASSERT_EQ(2, deletions_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  EXPECT_EQ("google.com", channel_ids.begin()->server_identifier());
}

TEST_F(DefaultChannelIDStoreTest, TestAsyncGetAndDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));

  ChannelIDStore::ChannelIDList pre_channel_ids;
  ChannelIDStore::ChannelIDList post_channel_ids;
  int delete_finished = 0;
  DefaultChannelIDStore store(persistent_store.get());

  store.GetAllChannelIDs(base::Bind(GetAllCallback, &pre_channel_ids));
  store.DeleteAll(base::Bind(&CallCounter, &delete_finished));
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &post_channel_ids));
  // Tasks have not run yet.
  EXPECT_EQ(0u, pre_channel_ids.size());
  // Wait for load & queued tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(2u, pre_channel_ids.size());
  EXPECT_EQ(0u, post_channel_ids.size());
}

TEST_F(DefaultChannelIDStoreTest, TestDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  std::unique_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Wait for load & queued set task.
  base::RunLoop().RunUntilIdle();

  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));

  EXPECT_EQ(2, store.GetChannelIDCount());
  int delete_finished = 0;
  store.DeleteChannelID("verisign.com",
                              base::Bind(&CallCounter, &delete_finished));
  ASSERT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("verisign.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
  EXPECT_EQ(OK, store.GetChannelID("google.com", &key,
                                   base::Bind(&GetChannelIDCallbackNotCalled)));
  int delete2_finished = 0;
  store.DeleteChannelID("google.com",
                        base::Bind(&CallCounter, &delete2_finished));
  ASSERT_EQ(1, delete2_finished);
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            store.GetChannelID("google.com", &key,
                               base::Bind(&GetChannelIDCallbackNotCalled)));
}

TEST_F(DefaultChannelIDStoreTest, TestAsyncDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  std::unique_ptr<crypto::ECPrivateKey> expected_key(
      crypto::ECPrivateKey::Create());
  persistent_store->AddChannelID(
      ChannelIDStore::ChannelID("a.com", base::Time::FromInternalValue(1),
                                crypto::ECPrivateKey::Create()));
  persistent_store->AddChannelID(ChannelIDStore::ChannelID(
      "b.com", base::Time::FromInternalValue(3), expected_key->Copy()));
  DefaultChannelIDStore store(persistent_store.get());
  int delete_finished = 0;
  store.DeleteChannelID("a.com",
                        base::Bind(&CallCounter, &delete_finished));

  AsyncGetChannelIDHelper a_helper;
  AsyncGetChannelIDHelper b_helper;
  std::unique_ptr<crypto::ECPrivateKey> key;
  EXPECT_EQ(0, store.GetChannelIDCount());
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("a.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&a_helper))));
  EXPECT_EQ(ERR_IO_PENDING,
            store.GetChannelID("b.com", &key,
                               base::Bind(&AsyncGetChannelIDHelper::Callback,
                                          base::Unretained(&b_helper))));

  EXPECT_EQ(0, delete_finished);
  EXPECT_FALSE(a_helper.called_);
  EXPECT_FALSE(b_helper.called_);
  // Wait for load & queued tasks.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delete_finished);
  EXPECT_EQ(1, store.GetChannelIDCount());
  EXPECT_FALSE(key);
  EXPECT_TRUE(a_helper.called_);
  EXPECT_THAT(a_helper.err_, IsError(ERR_FILE_NOT_FOUND));
  EXPECT_EQ("a.com", a_helper.server_identifier_);
  EXPECT_FALSE(a_helper.key_);
  EXPECT_TRUE(b_helper.called_);
  EXPECT_THAT(b_helper.err_, IsOk());
  EXPECT_EQ("b.com", b_helper.server_identifier_);
  EXPECT_TRUE(KeysEqual(expected_key.get(), b_helper.key_.get()));
}

TEST_F(DefaultChannelIDStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultChannelIDStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetChannelIDCount());
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "verisign.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "google.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "harvard.com", base::Time(), crypto::ECPrivateKey::Create()));
  store.SetChannelID(std::make_unique<ChannelIDStore::ChannelID>(
      "mit.com", base::Time(), crypto::ECPrivateKey::Create()));
  // Wait for load & queued set tasks.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4, store.GetChannelIDCount());
  ChannelIDStore::ChannelIDList channel_ids;
  store.GetAllChannelIDs(base::Bind(GetAllCallback, &channel_ids));
  EXPECT_EQ(4u, channel_ids.size());
}

}  // namespace net
