// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include <stdlib.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Sequence;

namespace {

using remoting::protocol::PairingRegistry;

class MockPairingRegistryCallbacks {
 public:
  MockPairingRegistryCallbacks() = default;
  virtual ~MockPairingRegistryCallbacks() = default;

  MOCK_METHOD1(DoneCallback, void(bool));
  MOCK_METHOD1(GetAllPairingsCallbackPtr, void(base::ListValue*));
  MOCK_METHOD1(GetPairingCallback, void(PairingRegistry::Pairing));

  void GetAllPairingsCallback(std::unique_ptr<base::ListValue> pairings) {
    GetAllPairingsCallbackPtr(pairings.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPairingRegistryCallbacks);
};

// Verify that a pairing Dictionary has correct entries, but doesn't include
// any shared secret.
void VerifyPairing(PairingRegistry::Pairing expected,
                   const base::DictionaryValue& actual) {
  std::string value;
  EXPECT_TRUE(actual.GetString(PairingRegistry::kClientNameKey, &value));
  EXPECT_EQ(expected.client_name(), value);
  EXPECT_TRUE(actual.GetString(PairingRegistry::kClientIdKey, &value));
  EXPECT_EQ(expected.client_id(), value);

  EXPECT_FALSE(actual.HasKey(PairingRegistry::kSharedSecretKey));
}

}  // namespace

namespace remoting {
namespace protocol {

class PairingRegistryTest : public testing::Test {
 public:
  void SetUp() override { callback_count_ = 0; }

  void set_pairings(std::unique_ptr<base::ListValue> pairings) {
    pairings_ = std::move(pairings);
  }

  void ExpectSecret(const std::string& expected,
                    PairingRegistry::Pairing actual) {
    EXPECT_EQ(expected, actual.shared_secret());
    ++callback_count_;
  }

  void ExpectSaveSuccess(bool success) {
    EXPECT_TRUE(success);
    ++callback_count_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;

  int callback_count_;
  std::unique_ptr<base::ListValue> pairings_;
};

TEST_F(PairingRegistryTest, CreateAndGetPairings) {
  scoped_refptr<PairingRegistry> registry = new SynchronousPairingRegistry(
      std::make_unique<MockPairingRegistryDelegate>());
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("my_client");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("my_client");

  EXPECT_NE(pairing_1.shared_secret(), pairing_2.shared_secret());

  registry->GetPairing(pairing_1.client_id(),
                       base::Bind(&PairingRegistryTest::ExpectSecret,
                                  base::Unretained(this),
                                  pairing_1.shared_secret()));
  EXPECT_EQ(1, callback_count_);

  // Check that the second client is paired with a different shared secret.
  registry->GetPairing(pairing_2.client_id(),
                       base::Bind(&PairingRegistryTest::ExpectSecret,
                                  base::Unretained(this),
                                  pairing_2.shared_secret()));
  EXPECT_EQ(2, callback_count_);
}

TEST_F(PairingRegistryTest, GetAllPairings) {
  scoped_refptr<PairingRegistry> registry = new SynchronousPairingRegistry(
      std::make_unique<MockPairingRegistryDelegate>());
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");

  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));

  ASSERT_EQ(2u, pairings_->GetSize());
  const base::DictionaryValue* actual_pairing_1;
  const base::DictionaryValue* actual_pairing_2;
  ASSERT_TRUE(pairings_->GetDictionary(0, &actual_pairing_1));
  ASSERT_TRUE(pairings_->GetDictionary(1, &actual_pairing_2));

  // Ordering is not guaranteed, so swap if necessary.
  std::string actual_client_id;
  ASSERT_TRUE(actual_pairing_1->GetString(PairingRegistry::kClientIdKey,
                                          &actual_client_id));
  if (actual_client_id != pairing_1.client_id()) {
    std::swap(actual_pairing_1, actual_pairing_2);
  }

  VerifyPairing(pairing_1, *actual_pairing_1);
  VerifyPairing(pairing_2, *actual_pairing_2);
}

TEST_F(PairingRegistryTest, DeletePairing) {
  scoped_refptr<PairingRegistry> registry = new SynchronousPairingRegistry(
      std::make_unique<MockPairingRegistryDelegate>());
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");

  registry->DeletePairing(
      pairing_1.client_id(),
      base::Bind(&PairingRegistryTest::ExpectSaveSuccess,
                 base::Unretained(this)));

  // Re-read the list, and verify it only has the pairing_2 client.
  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));

  ASSERT_EQ(1u, pairings_->GetSize());
  const base::DictionaryValue* actual_pairing_2;
  ASSERT_TRUE(pairings_->GetDictionary(0, &actual_pairing_2));
  std::string actual_client_id;
  ASSERT_TRUE(actual_pairing_2->GetString(PairingRegistry::kClientIdKey,
                                          &actual_client_id));
  EXPECT_EQ(pairing_2.client_id(), actual_client_id);
}

TEST_F(PairingRegistryTest, ClearAllPairings) {
  scoped_refptr<PairingRegistry> registry = new SynchronousPairingRegistry(
      std::make_unique<MockPairingRegistryDelegate>());
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");

  registry->ClearAllPairings(
      base::Bind(&PairingRegistryTest::ExpectSaveSuccess,
                 base::Unretained(this)));

  // Re-read the list, and verify it is empty.
  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));

  EXPECT_TRUE(pairings_->empty());
}

ACTION_P(QuitMessageLoop, callback) {
  callback.Run();
}

MATCHER_P(EqualsClientName, client_name, "") {
  return arg.client_name() == client_name;
}

MATCHER(NoPairings, "") {
  return arg->empty();
}

TEST_F(PairingRegistryTest, SerializedRequests) {
  MockPairingRegistryCallbacks callbacks;
  Sequence s;
  EXPECT_CALL(callbacks, GetPairingCallback(EqualsClientName("client1")))
      .InSequence(s);
  EXPECT_CALL(callbacks, GetPairingCallback(EqualsClientName("client2")))
      .InSequence(s);
  EXPECT_CALL(callbacks, DoneCallback(true))
      .InSequence(s);
  EXPECT_CALL(callbacks, GetPairingCallback(EqualsClientName("client1")))
      .InSequence(s);
  EXPECT_CALL(callbacks, GetPairingCallback(EqualsClientName("")))
      .InSequence(s);
  EXPECT_CALL(callbacks, DoneCallback(true))
      .InSequence(s);
  EXPECT_CALL(callbacks, GetAllPairingsCallbackPtr(NoPairings()))
      .InSequence(s);
  EXPECT_CALL(callbacks, GetPairingCallback(EqualsClientName("client3")))
      .InSequence(s)
      .WillOnce(QuitMessageLoop(run_loop_.QuitClosure()));

  scoped_refptr<PairingRegistry> registry =
      new PairingRegistry(base::ThreadTaskRunnerHandle::Get(),
                          std::make_unique<MockPairingRegistryDelegate>());
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");
  registry->GetPairing(
      pairing_1.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::GetPairingCallback,
                 base::Unretained(&callbacks)));
  registry->GetPairing(
      pairing_2.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::GetPairingCallback,
                 base::Unretained(&callbacks)));
  registry->DeletePairing(
      pairing_2.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::DoneCallback,
                 base::Unretained(&callbacks)));
  registry->GetPairing(
      pairing_1.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::GetPairingCallback,
                 base::Unretained(&callbacks)));
  registry->GetPairing(
      pairing_2.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::GetPairingCallback,
                 base::Unretained(&callbacks)));
  registry->ClearAllPairings(
      base::Bind(&MockPairingRegistryCallbacks::DoneCallback,
                 base::Unretained(&callbacks)));
  registry->GetAllPairings(
      base::Bind(&MockPairingRegistryCallbacks::GetAllPairingsCallback,
                 base::Unretained(&callbacks)));
  PairingRegistry::Pairing pairing_3 = registry->CreatePairing("client3");
  registry->GetPairing(
      pairing_3.client_id(),
      base::Bind(&MockPairingRegistryCallbacks::GetPairingCallback,
                 base::Unretained(&callbacks)));

  run_loop_.Run();
}

}  // namespace protocol
}  // namespace remoting
