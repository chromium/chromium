// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/persistent_pref_store_client.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/unittest_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prefs {
namespace {

constexpr char kChildKey[] = "child";
constexpr char kOtherDictionaryKey[] = "other_key";

struct UpdateOrAck {
  std::vector<mojom::PrefUpdatePtr> updates;
  bool is_ack;
};

struct Request {
  std::string key;
  std::vector<std::string> path;
};

struct UpdateOrRequest {
  std::vector<mojom::PrefUpdatePtr> updates;
  Request request;
};

// A client connection to a between a PersistentPrefStoreImpl and a
// PersistentPrefStoreClient that provides control over when messages between
// the two are delivered.
class PrefServiceConnection : public mojom::PrefStoreObserver,
                              public mojom::PersistentPrefStore {
 public:
  explicit PrefServiceConnection(PersistentPrefStoreImpl* pref_store) {
    auto connection = pref_store->CreateConnection({
        kKey, kOtherDictionaryKey, kDictionaryKey,
    });
    observer_receiver_.Bind(
        std::move(connection->pref_store_connection->observer));
    connection->pref_store_connection->observer =
        observer_.BindNewPipeAndPassReceiver();

    pref_store_.Bind(std::move(connection->pref_store));
    pref_store_receiver_.Bind(
        connection->pref_store.InitWithNewPipeAndPassReceiver());

    pref_store_client_ =
        base::MakeRefCounted<PersistentPrefStoreClient>(std::move(connection));
    auto pref_notifier = std::make_unique<PrefNotifierImpl>();
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry->RegisterIntegerPref(kKey, kInitialValue);
    pref_registry->RegisterIntegerPref(kOtherDictionaryKey, kInitialValue);
    pref_registry->RegisterDictionaryPref(kDictionaryKey);
    auto pref_value_store = std::make_unique<PrefValueStore>(
        nullptr, nullptr, nullptr, nullptr, pref_store_client_.get(), nullptr,
        pref_registry->defaults().get(), pref_notifier.get());
    pref_service_ = std::make_unique<PrefService>(
        std::move(pref_notifier), std::move(pref_value_store),
        pref_store_client_.get(), pref_registry.get(), base::DoNothing(), true);
  }

  ~PrefServiceConnection() override {
    observer_receiver_.FlushForTesting();
    pref_store_receiver_.FlushForTesting();
    EXPECT_TRUE(writes_.empty());
    EXPECT_TRUE(updates_.empty());
  }

  PrefService& pref_service() { return *pref_service_; }

  void WaitForWrites(size_t num_writes) {
    if (writes_.size() >= num_writes)
      return;

    expected_writes_ = num_writes;
    base::RunLoop run_loop;
    stop_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  void ForwardWrites(size_t num_writes) {
    WaitForWrites(num_writes);
    for (size_t i = 0; i < num_writes; ++i) {
      auto& write = writes_.front();
      if (write.updates.empty()) {
        pref_store_->RequestValue(write.request.key, write.request.path);
      } else {
        pref_store_->SetValues(std::move(write.updates));
      }
      writes_.pop_front();
    }
    pref_store_.FlushForTesting();
  }
  void WaitForUpdates(size_t num_updates) {
    if (updates_.size() >= num_updates)
      return;

    expected_updates_ = num_updates;
    base::RunLoop run_loop;
    stop_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ForwardUpdates(size_t num_updates) {
    WaitForUpdates(num_updates);
    for (size_t i = 0; i < num_updates; ++i) {
      auto& update = updates_.front();
      if (update.is_ack) {
        observer_->OnPrefChangeAck();
      } else {
        observer_->OnPrefsChanged(std::move(update.updates));
      }
      updates_.pop_front();
    }
    observer_.FlushForTesting();
  }

  // mojom::PersistentPrefStore
  void SetValues(std::vector<mojom::PrefUpdatePtr> updates) override {
    writes_.push_back({std::move(updates)});
    if (writes_.size() == expected_writes_) {
      expected_writes_ = 0;
      std::move(stop_).Run();
    }
  }
  void RequestValue(const std::string& key,
                    const std::vector<std::string>& path) override {
    writes_.push_back({std::vector<mojom::PrefUpdatePtr>(), {key, path}});
    if (writes_.size() == expected_writes_) {
      expected_writes_ = 0;
      std::move(stop_).Run();
    }
  }
  void CommitPendingWrite(base::OnceClosure) override {}
  void SchedulePendingLossyWrites() override {}
  void ClearMutableValues() override {}
  void OnStoreDeletionFromDisk() override {}

  // mojom::PrefStoreObserver
  void OnPrefsChanged(std::vector<mojom::PrefUpdatePtr> updates) override {
    updates_.push_back({std::move(updates), false});
    if (updates_.size() == expected_updates_) {
      expected_updates_ = 0;
      std::move(stop_).Run();
    }
  }
  void OnInitializationCompleted(bool succeeded) override { NOTREACHED(); }
  void OnPrefChangeAck() override {
    updates_.push_back({std::vector<mojom::PrefUpdatePtr>(), true});
    acks_.push_back(updates_.size());
  }

 private:
  scoped_refptr<PersistentPrefStoreClient> pref_store_client_;
  std::unique_ptr<PrefService> pref_service_;
  mojo::Remote<mojom::PrefStoreObserver> observer_;
  mojo::Remote<mojom::PersistentPrefStore> pref_store_;
  mojo::Receiver<mojom::PrefStoreObserver> observer_receiver_{this};
  mojo::Receiver<mojom::PersistentPrefStore> pref_store_receiver_{this};

  base::OnceClosure stop_;
  size_t expected_writes_ = 0;
  size_t expected_updates_ = 0;

  base::circular_deque<UpdateOrRequest> writes_;
  base::circular_deque<UpdateOrAck> updates_;
  base::circular_deque<size_t> acks_;
};

class PersistentPrefStoreConsistencyTest : public testing::Test {
 public:
  void SetUp() override {
    pref_store_ = base::MakeRefCounted<InMemoryPrefStore>();
    pref_store_impl_ = std::make_unique<PersistentPrefStoreImpl>(
        pref_store_, base::DoNothing());
  }

  PersistentPrefStore* pref_store() { return pref_store_.get(); }

  std::unique_ptr<PrefServiceConnection> CreateConnection() {
    return std::make_unique<PrefServiceConnection>(pref_store_impl_.get());
  }

 private:
  scoped_refptr<PersistentPrefStore> pref_store_;
  std::unique_ptr<PersistentPrefStoreImpl> pref_store_impl_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PersistentPrefStoreConsistencyTest, TwoPrefs) {
  pref_store()->SetValue(kKey, std::make_unique<base::Value>(kInitialValue), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();

  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();

  EXPECT_EQ(kInitialValue, pref_service.GetInteger(kKey));
  EXPECT_EQ(kInitialValue, pref_service2.GetInteger(kKey));

  pref_service.SetInteger(kKey, 2);
  pref_service2.SetInteger(kKey, 3);

  // The writes arrive in the order: 2, 3.
  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  connection->ForwardUpdates(1);
  EXPECT_EQ(2, pref_service.GetInteger(kKey));

  // This connection already has a later value. It should not move backwards in
  // time.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(3, pref_service2.GetInteger(kKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(3, pref_service.GetInteger(kKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(3, pref_service2.GetInteger(kKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, TwoSubPrefs) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 2);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  // The writes arrive in the order: 2, 3.
  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue two_dict;
  two_dict.SetInteger(kKey, 2);
  base::DictionaryValue three_dict;
  three_dict.SetInteger(kKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_EQ(two_dict, *pref_service.GetDictionary(kDictionaryKey));

  // This connection already has a later value. It should not move backwards in
  // time.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, DifferentSubPrefs) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 2);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kOtherDictionaryKey, 3);
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue two_dict;
  two_dict.SetInteger(kKey, 2);
  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 2);
  expected_dict.SetInteger(kOtherDictionaryKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_EQ(two_dict, *pref_service.GetDictionary(kDictionaryKey));

  // This connection already has a later value. It should not move backwards in
  // time.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, WriteParentThenChild) {
  auto initial_value = std::make_unique<base::DictionaryValue>();
  initial_value->SetDictionary(kDictionaryKey,
                               std::make_unique<base::DictionaryValue>());
  pref_store()->SetValue(kDictionaryKey, std::move(initial_value), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 4);
    update->Clear();
    update->SetInteger(kKey, 2);
    update->SetInteger(kOtherDictionaryKey, 4);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue two_and_four_dict;
  two_and_four_dict.SetInteger(kKey, 2);
  two_and_four_dict.SetInteger(kOtherDictionaryKey, 4);
  base::DictionaryValue three_dict;
  three_dict.SetInteger(kKey, 3);
  three_dict.SetDictionary(kDictionaryKey,
                           std::make_unique<base::DictionaryValue>());
  base::Value five_dict = three_dict.Clone();
  five_dict.SetKey(kKey, base::Value(5));
  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 3);
  expected_dict.SetInteger(kOtherDictionaryKey, 4);

  connection->ForwardUpdates(1);
  EXPECT_EQ(two_and_four_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  // Applying the other client's write would lose this client's write so is
  // ignored. Instead, this client requests an updated value from the service.
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));
  connection2->ForwardWrites(1);

  // Perform another update while waiting for the updated value from the
  // service.
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 5);
  }
  connection2->ForwardWrites(1);

  expected_dict.SetInteger(kKey, 5);
  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  // Ack for the original write.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(five_dict, *pref_service2.GetDictionary(kDictionaryKey));

  // New value that is ignored due to the second in-flight write.
  connection2->ForwardUpdates(1);
  // Sending another request for the value from the service.
  connection2->ForwardWrites(1);
  EXPECT_EQ(five_dict, *pref_service2.GetDictionary(kDictionaryKey));

  // Ack for the second write.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(five_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, WriteChildThenParent) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 4);
    update->Clear();
    update->SetInteger(kKey, 2);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  connection2->ForwardWrites(1);
  connection->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 2);
  base::DictionaryValue three_dict;
  three_dict.SetInteger(kKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, WriteChildThenDeleteParent) {
  pref_store()->SetValue(kDictionaryKey,
                         std::make_unique<base::DictionaryValue>(), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  pref_service.ClearPref(kDictionaryKey);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  connection2->ForwardWrites(1);
  connection->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 2);
  base::DictionaryValue three_dict;
  three_dict.SetInteger(kKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_FALSE(pref_service2.HasPrefPath(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, DeleteParentThenWriteChild) {
  auto initial_value = std::make_unique<base::DictionaryValue>();
  initial_value->SetInteger(kOtherDictionaryKey, 5);
  pref_store()->SetValue(kDictionaryKey, std::move(initial_value), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  pref_service.ClearPref(kDictionaryKey);
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue intermediate_dict;
  intermediate_dict.SetInteger(kKey, 3);
  intermediate_dict.SetInteger(kOtherDictionaryKey, 5);

  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardWrites(1);
  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, WriteParentThenDeleteChild) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 1);
    update->Clear();
    update->SetInteger(kKey, 2);
    update->SetInteger(kOtherDictionaryKey, 4);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
    update->RemovePath(kKey, nullptr);
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue two_and_four_dict;
  two_and_four_dict.SetInteger(kKey, 2);
  two_and_four_dict.SetInteger(kOtherDictionaryKey, 4);
  base::DictionaryValue empty_dict;
  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kOtherDictionaryKey, 4);

  connection->ForwardUpdates(1);
  EXPECT_EQ(two_and_four_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));

  // Applying the other client's write would lose this client's write so is
  // ignored. Instead, this client requests an updated value from the service.
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));
  connection2->ForwardWrites(1);

  // Ack for the original write.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, DeleteChildThenWriteParent) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 4);
    update->Clear();
    update->SetInteger(kKey, 2);
    update->SetInteger(kOtherDictionaryKey, 4);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
    update->RemovePath(kKey, nullptr);
  }

  connection2->ForwardWrites(1);
  connection->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue empty_dict;
  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 2);
  expected_dict.SetInteger(kOtherDictionaryKey, 4);

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, ReplaceParentThenWriteChild) {
  auto initial_value = std::make_unique<base::DictionaryValue>();
  initial_value->SetPath({kKey, kOtherDictionaryKey}, base::Value(5));
  pref_store()->SetValue(kDictionaryKey, std::move(initial_value), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 1);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetPath({kKey, kChildKey}, base::Value(2));
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue simple_dict;
  simple_dict.SetInteger(kKey, 1);
  base::DictionaryValue nested_dict;
  nested_dict.SetPath({kKey, kChildKey}, base::Value(2));
  nested_dict.SetPath({kKey, kOtherDictionaryKey}, base::Value(5));
  base::DictionaryValue expected_dict;
  expected_dict.SetPath({kKey, kChildKey}, base::Value(2));

  connection->ForwardUpdates(1);
  EXPECT_EQ(simple_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(nested_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardWrites(1);
  connection2->ForwardUpdates(1);
  EXPECT_EQ(nested_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, WriteChildThenReplaceParent) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->SetInteger(kKey, 1);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetPath({kKey, kChildKey}, base::Value(2));
  }

  connection2->ForwardWrites(1);
  connection->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue simple_dict;
  simple_dict.SetInteger(kKey, 1);
  base::DictionaryValue nested_dict;
  nested_dict.SetPath({kKey, kChildKey}, base::Value(2));

  connection->ForwardUpdates(1);
  EXPECT_EQ(simple_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection->ForwardUpdates(1);
  EXPECT_EQ(simple_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(nested_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(simple_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, NestedWriteParentThenChild) {
  pref_store()->SetValue(kKey, std::make_unique<base::DictionaryValue>(), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    auto nested_dict =
        update->SetDictionary(kKey, std::make_unique<base::DictionaryValue>());
    nested_dict->SetInteger(kChildKey, 2);
    nested_dict->SetInteger(kOtherDictionaryKey, 4);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetPath({kKey, kChildKey}, base::Value(3));
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue two_and_four_dict;
  two_and_four_dict.SetPath({kKey, kChildKey}, base::Value(2));
  two_and_four_dict.SetPath({kKey, kOtherDictionaryKey}, base::Value(4));
  base::DictionaryValue three_dict;
  three_dict.SetPath({kKey, kChildKey}, base::Value(3));
  base::Value expected_dict = two_and_four_dict.Clone();
  expected_dict.SetPath({kKey, kChildKey}, base::Value(3));

  connection->ForwardUpdates(1);
  EXPECT_EQ(two_and_four_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  // Applying the other client's write would lose this client's write so is
  // ignored. Instead, this client requests an updated value from the service.
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));
  connection2->ForwardWrites(1);

  // Ack for the original write.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  // Updated value from the service.
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest, NestedWriteChildThenParent) {
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    auto nested_dict =
        update->SetDictionary(kKey, std::make_unique<base::DictionaryValue>());
    nested_dict->SetInteger(kChildKey, 2);
    nested_dict->SetInteger(kOtherDictionaryKey, 4);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetPath({kKey, kChildKey}, base::Value(3));
  }

  connection2->ForwardWrites(1);
  connection->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue expected_dict;
  expected_dict.SetPath({kKey, kChildKey}, base::Value(2));
  expected_dict.SetPath({kKey, kOtherDictionaryKey}, base::Value(4));
  base::DictionaryValue three_dict;
  three_dict.SetPath({kKey, kChildKey}, base::Value(3));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(three_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));
  connection2->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest,
       DeleteParentThenWriteChildThenDeleteParent) {
  auto initial_value = std::make_unique<base::DictionaryValue>();
  initial_value->SetInteger(kOtherDictionaryKey, 5);
  pref_store()->SetValue(kDictionaryKey, std::move(initial_value), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  pref_service.ClearPref(kDictionaryKey);
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetInteger(kKey, 3);
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue intermediate_dict;
  intermediate_dict.SetInteger(kKey, 3);
  intermediate_dict.SetInteger(kOtherDictionaryKey, 5);

  base::DictionaryValue expected_dict;
  expected_dict.SetInteger(kKey, 3);

  connection->ForwardUpdates(1);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  pref_service.ClearPref(kDictionaryKey);
  connection->ForwardWrites(1);
  connection->ForwardUpdates(1);
  EXPECT_FALSE(pref_service.HasPrefPath(kDictionaryKey));

  connection2->ForwardWrites(1);
  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_FALSE(pref_service2.HasPrefPath(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_FALSE(pref_service2.HasPrefPath(kDictionaryKey));
}

TEST_F(PersistentPrefStoreConsistencyTest,
       NestedDeleteParentThenWriteChildThenDeleteChild) {
  auto initial_value = std::make_unique<base::DictionaryValue>();
  initial_value->SetPath({kKey, kOtherDictionaryKey}, base::Value(5));
  pref_store()->SetValue(kDictionaryKey, std::move(initial_value), 0);
  auto connection = CreateConnection();
  auto connection2 = CreateConnection();
  auto& pref_service = connection->pref_service();
  auto& pref_service2 = connection2->pref_service();
  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->RemovePath(kKey, nullptr);
  }
  {
    ScopedDictionaryPrefUpdate update(&pref_service2, kDictionaryKey);
    update->SetPath({kKey, kChildKey}, base::Value(3));
  }

  connection->ForwardWrites(1);
  connection2->ForwardWrites(1);

  connection->WaitForUpdates(2);
  connection2->WaitForUpdates(2);

  base::DictionaryValue intermediate_dict;
  intermediate_dict.SetPath({kKey, kChildKey}, base::Value(3));
  intermediate_dict.SetPath({kKey, kOtherDictionaryKey}, base::Value(5));

  base::DictionaryValue expected_dict;
  expected_dict.SetPath({kKey, kChildKey}, base::Value(3));

  base::DictionaryValue empty_dict;

  connection->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection->ForwardUpdates(1);
  EXPECT_EQ(expected_dict, *pref_service.GetDictionary(kDictionaryKey));

  {
    ScopedDictionaryPrefUpdate update(&pref_service, kDictionaryKey);
    update->RemovePath(kKey, nullptr);
  }
  connection->ForwardWrites(1);
  connection->ForwardUpdates(1);
  EXPECT_TRUE(pref_service.HasPrefPath(kDictionaryKey));
  EXPECT_EQ(empty_dict, *pref_service.GetDictionary(kDictionaryKey));

  connection2->ForwardWrites(1);
  connection2->ForwardUpdates(1);
  EXPECT_EQ(intermediate_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));

  connection2->ForwardUpdates(1);
  EXPECT_EQ(empty_dict, *pref_service2.GetDictionary(kDictionaryKey));
}

}  // namespace
}  // namespace prefs
