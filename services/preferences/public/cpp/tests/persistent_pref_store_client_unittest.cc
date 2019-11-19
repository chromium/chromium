// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/persistent_pref_store_client.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prefs {
namespace {

constexpr char kDictionaryKey[] = "path.to.key";
constexpr char kUninitializedDictionaryKey[] = "path.to.an.uninitialized.dict";

class PersistentPrefStoreClientTest : public testing::Test,
                                      public mojom::PersistentPrefStore {
 public:
  PersistentPrefStoreClientTest() = default;

  // testing::Test:
  void SetUp() override {
    auto persistent_pref_store_client =
        base::MakeRefCounted<PersistentPrefStoreClient>(
            mojom::PersistentPrefStoreConnection::New(
                mojom::PrefStoreConnection::New(
                    mojo::PendingReceiver<mojom::PrefStoreObserver>(),
                    base::Value(base::Value::Type::DICTIONARY), true),
                receiver_.BindNewPipeAndPassRemote(),
                ::PersistentPrefStore::PREF_READ_ERROR_NONE, false));
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    pref_registry->RegisterDictionaryPref(kDictionaryKey);
    pref_registry->RegisterDictionaryPref(kUninitializedDictionaryKey);
    auto pref_notifier = std::make_unique<PrefNotifierImpl>();
    auto pref_value_store = std::make_unique<PrefValueStore>(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        pref_notifier.get());
    pref_service_ = std::make_unique<PrefService>(
        std::move(pref_notifier), std::move(pref_value_store),
        persistent_pref_store_client.get(), pref_registry.get(),
        base::DoNothing(), false);
  }

  void TearDown() override {
    pref_service_ = nullptr;
    base::RunLoop().RunUntilIdle();
    receiver_.reset();
    base::RunLoop().RunUntilIdle();
  }

  PrefService* pref_service() { return pref_service_.get(); }

  mojom::PrefUpdateValuePtr WaitForUpdate() {
    base::RunLoop run_loop;
    on_update_ = run_loop.QuitClosure();
    run_loop.Run();
    EXPECT_EQ(1u, last_updates_.size());
    auto result = std::move(last_updates_[0]->value);
    last_updates_.clear();
    return result;
  }

  void ExpectNoUpdate() {
    pref_service()->CommitPendingWrite();
    receiver_.FlushForTesting();
    EXPECT_TRUE(last_updates_.empty());
  }

 private:
  void SetValues(std::vector<mojom::PrefUpdatePtr> updates) override {
    last_updates_ = std::move(updates);
    if (on_update_)
      std::move(on_update_).Run();
  }

  void RequestValue(const std::string& key,
                    const std::vector<std::string>& path) override {}

  void CommitPendingWrite(CommitPendingWriteCallback callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }

  void SchedulePendingLossyWrites() override {}
  void ClearMutableValues() override {}
  void OnStoreDeletionFromDisk() override {}

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<PrefService> pref_service_;

  mojo::Receiver<mojom::PersistentPrefStore> receiver_{this};

  std::vector<mojom::PrefUpdatePtr> last_updates_;
  base::OnceClosure on_update_;

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreClientTest);
};

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_Basic) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::Value(1), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_BasicWithoutPathExpansion) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetKey("key.for.integer", base::Value(1));
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  EXPECT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::Value(1), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"key.for.integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_Remove) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.another_integer", 1);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->Remove("path.to.integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_RemoveWithoutPathExpansion) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetKey("path.to.another_integer", base::Value(1));
    update->SetKey("path.to.integer", base::Value(1));
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemoveWithoutPathExpansion("path.to.integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path.to.integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_MultipleUpdates) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetKey("a.double", base::Value(1.0));
  }
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 2);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(2u, split_updates.size());
  EXPECT_EQ(base::Value(1.0), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"a.double"}), split_updates[0]->path);
  EXPECT_EQ(base::Value(2), *split_updates[1]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "integer"}),
            split_updates[1]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_NestedUpdateAfterSet) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    std::unique_ptr<DictionaryValueUpdate> dict;
    ASSERT_TRUE(update->GetDictionary("path.to", &dict));
    dict->Clear();
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::DictionaryValue(), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_NestedUpdateBeforeSet) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->Set("path.to", std::make_unique<base::DictionaryValue>());
  }
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  base::DictionaryValue expected_value;
  expected_value.SetInteger("integer", 1);
  EXPECT_EQ(expected_value, *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_DoubleNestedUpdate) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    std::unique_ptr<DictionaryValueUpdate> dict;
    ASSERT_TRUE(update->GetDictionary("path", &dict));
    dict->Clear();
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::DictionaryValue(), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_ManualNesting) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    std::unique_ptr<DictionaryValueUpdate> dict;
    ASSERT_TRUE(update->GetDictionary("path.to", &dict));
    dict->SetString("string", "string value");
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::Value("string value"), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "string"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_ManualDictCreationAndNesting) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    auto dict = update->SetDictionary(
        "path.to", std::make_unique<base::DictionaryValue>());
    dict->SetString("string", "string value");
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  base::DictionaryValue expected_value;
  expected_value.SetString("string", "string value");
  EXPECT_EQ(expected_value, *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_ManualDictCreationWithoutPathExpansionAndNesting) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    auto dict = update->SetDictionaryWithoutPathExpansion(
        "a.dictionary", std::make_unique<base::DictionaryValue>());
    dict->SetKey("a.string", base::Value("string value"));
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  base::DictionaryValue expected_value;
  expected_value.SetKey("a.string", base::Value("string value"));
  EXPECT_EQ(expected_value, *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"a.dictionary"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_AsDictionaryTriggersFullWrite) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
    update->AsDictionary();
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_atomic_update());
  base::DictionaryValue expected_value;
  expected_value.SetInteger("path.to.integer", 1);
  EXPECT_EQ(expected_value, *update->get_atomic_update());
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_AsConstDictionaryIsNoOp) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
    update->AsConstDictionary();
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::Value(1), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_RemovePath_Basic) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
    update->SetInteger("path.to.something.else", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemovePath("path.to.integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to", "integer"}),
            split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_RemovePath_RemoveContainingDict) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
    update->SetInteger("path.for.something.else", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemovePath("path.to.integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path", "to"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_RemovePath_SinglePathComponent) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("integer", 1);
    update->SetInteger("something_else", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemovePath("integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"integer"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_RemovePath_FullPref) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemovePath("path.to.integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest,
       SubPrefUpdates_RemovePathSinglePathComponent_FullPref) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->RemovePath("integer", nullptr);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_FALSE(split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"integer"}), split_updates[0]->path);
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_NoChange) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  ScopedDictionaryPrefUpdate(pref_service(), kDictionaryKey).Get();
  ExpectNoUpdate();
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_SetToExistingValue) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  ExpectNoUpdate();
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_ClearEmptyDictionary) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->Clear();
  }
  ExpectNoUpdate();
}

TEST_F(PersistentPrefStoreClientTest, SubPrefUpdates_ReplaceDictionary) {
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path.to.integer", 1);
  }
  WaitForUpdate();
  {
    ScopedDictionaryPrefUpdate update(pref_service(), kDictionaryKey);
    update->SetInteger("path", 2);
  }
  auto update = WaitForUpdate();
  ASSERT_TRUE(update->is_split_updates());
  auto& split_updates = update->get_split_updates();
  ASSERT_EQ(1u, split_updates.size());
  EXPECT_EQ(base::Value(2), *split_updates[0]->value);
  EXPECT_EQ((std::vector<std::string>{"path"}), split_updates[0]->path);
}

}  // namespace
}  // namespace prefs
