// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/in_memory_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/persistent_pref_store_client.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/unittest_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prefs {
namespace {

class PersistentPrefStoreMock : public InMemoryPrefStore {
 public:
  void CommitPendingWrite(
      base::OnceClosure reply_callback,
      base::OnceClosure synchronous_done_callback) override {
    CommitPendingWriteMock();
    InMemoryPrefStore::CommitPendingWrite(std::move(reply_callback),
                                          std::move(synchronous_done_callback));
  }

  MOCK_METHOD0(CommitPendingWriteMock, void());
  MOCK_METHOD0(SchedulePendingLossyWrites, void());
  MOCK_METHOD0(ClearMutableValues, void());

 private:
  ~PersistentPrefStoreMock() override = default;
};

class PersistentPrefStoreImplTest : public testing::Test {
 public:
  PersistentPrefStoreImplTest() = default;

  // testing::Test:
  void TearDown() override {
    pref_store_ = nullptr;
    base::RunLoop().RunUntilIdle();
    impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateImpl(scoped_refptr<PersistentPrefStore> backing_pref_store) {
    base::RunLoop run_loop;
    bool initialized = backing_pref_store->IsInitializationComplete();
    impl_ = std::make_unique<PersistentPrefStoreImpl>(
        std::move(backing_pref_store), run_loop.QuitClosure());
    if (!initialized)
      run_loop.Run();
    pref_store_ = CreateConnection();
  }

  scoped_refptr<PersistentPrefStore> CreateConnection(
      PersistentPrefStoreImpl::ObservedPrefs observed_prefs =
          PersistentPrefStoreImpl::ObservedPrefs()) {
    if (observed_prefs.empty())
      observed_prefs.insert({kKey, kOtherDictionaryKey});
    return base::MakeRefCounted<PersistentPrefStoreClient>(
        impl_->CreateConnection(std::move(observed_prefs)));
  }

  PersistentPrefStore* pref_store() { return pref_store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<PersistentPrefStoreImpl> impl_;

  scoped_refptr<PersistentPrefStore> pref_store_;

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreImplTest);
};

TEST_F(PersistentPrefStoreImplTest, InitialValue) {
  constexpr char kUnregisteredKey[] = "path.to.unregistered_key";
  constexpr char kUnregisteredTopLevelKey[] = "unregistered_key";
  constexpr char kUnregisteredPrefixKey[] = "p";
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  backing_pref_store->SetValue(kUnregisteredKey, value.CreateDeepCopy(), 0);
  backing_pref_store->SetValue(kUnregisteredPrefixKey, value.CreateDeepCopy(),
                               0);
  backing_pref_store->SetValue(kUnregisteredTopLevelKey, value.CreateDeepCopy(),
                               0);
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));

  EXPECT_FALSE(pref_store()->GetValue(kUnregisteredKey, nullptr));
  EXPECT_FALSE(pref_store()->GetValue(kUnregisteredTopLevelKey, nullptr));
  EXPECT_FALSE(pref_store()->GetValue(kUnregisteredPrefixKey, nullptr));
}

TEST_F(PersistentPrefStoreImplTest, InitialValueWithoutPathExpansion) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, WriteObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value value("value");
  pref_store()->SetValue(kKey, value.CreateDeepCopy(), 0);

  ExpectPrefChange(other_pref_store.get(), kKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, UnregisteredPrefNotObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  PersistentPrefStoreImpl::ObservedPrefs observed_prefs;
  observed_prefs.insert(kKey);

  auto other_pref_store = CreateConnection(std::move(observed_prefs));
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  pref_store()->SetValue(kOtherDictionaryKey,
                         std::make_unique<base::Value>(123), 0);
  pref_store()->SetValue(kKey, std::make_unique<base::Value>("value"), 0);

  ExpectPrefChange(other_pref_store.get(), kKey);
  EXPECT_FALSE(other_pref_store->GetValue(kOtherDictionaryKey, nullptr));
}

TEST_F(PersistentPrefStoreImplTest,
       WriteWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  pref_store()->SetValue(kKey, dict.CreateDeepCopy(), 0);

  ExpectPrefChange(other_pref_store.get(), kKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PersistentPrefStoreImplTest, RemoveObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  const base::Value value("value");
  backing_pref_store->SetValue(kKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(value.Equals(output));
  pref_store()->RemoveValue(kKey, 0);

  // This should be a no-op and shouldn't trigger a notification for the other
  // client.
  pref_store()->RemoveValue(kKey, 0);

  ExpectPrefChange(other_pref_store.get(), kKey);
  EXPECT_FALSE(other_pref_store->GetValue(kKey, &output));
}

TEST_F(PersistentPrefStoreImplTest,
       RemoveWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  base::DictionaryValue dict;
  dict.SetKey(kKey, base::Value("value"));
  backing_pref_store->SetValue(kKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  auto other_pref_store = CreateConnection();
  EXPECT_TRUE(other_pref_store->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  EXPECT_TRUE(dict.Equals(output));

  base::Value* mutable_value = nullptr;
  dict.SetKey(kKey, base::Value("value"));
  ASSERT_TRUE(pref_store()->GetMutableValue(kKey, &mutable_value));
  base::DictionaryValue* mutable_dict = nullptr;
  ASSERT_TRUE(mutable_value->GetAsDictionary(&mutable_dict));
  mutable_dict->RemoveWithoutPathExpansion(kKey, nullptr);
  pref_store()->ReportValueChanged(kKey, 0);

  ExpectPrefChange(other_pref_store.get(), kKey);
  ASSERT_TRUE(other_pref_store->GetValue(kKey, &output));
  const base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(output->GetAsDictionary(&dict_value));
  EXPECT_TRUE(dict_value->empty());
}

TEST_F(PersistentPrefStoreImplTest, CommitPendingWrite) {
  auto backing_store = base::MakeRefCounted<PersistentPrefStoreMock>();
  CreateImpl(backing_store);
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, CommitPendingWriteMock()).Times(2);
  pref_store()->CommitPendingWrite(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(PersistentPrefStoreImplTest, SchedulePendingLossyWrites) {
  auto backing_store = base::MakeRefCounted<PersistentPrefStoreMock>();
  CreateImpl(backing_store);
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, SchedulePendingLossyWrites())
      .WillOnce(testing::WithoutArgs(
          testing::Invoke([&run_loop]() { run_loop.Quit(); })));
  EXPECT_CALL(*backing_store, CommitPendingWriteMock()).Times(1);
  pref_store()->SchedulePendingLossyWrites();
  run_loop.Run();
}

TEST_F(PersistentPrefStoreImplTest, ClearMutableValues) {
  auto backing_store = base::MakeRefCounted<PersistentPrefStoreMock>();
  CreateImpl(backing_store);
  base::RunLoop run_loop;
  EXPECT_CALL(*backing_store, ClearMutableValues())
      .WillOnce(testing::WithoutArgs(
          testing::Invoke([&run_loop]() { run_loop.Quit(); })));
  EXPECT_CALL(*backing_store, CommitPendingWriteMock()).Times(1);
  pref_store()->ClearMutableValues();
  run_loop.Run();
}

}  // namespace
}  // namespace prefs
