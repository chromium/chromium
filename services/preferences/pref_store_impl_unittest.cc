// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/value_map_pref_store.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/preferences/public/cpp/pref_store_client.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/unittest_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prefs {
namespace {

class MockPrefStore : public ValueMapPrefStore {
 public:
  bool IsInitializationComplete() const override {
    return initialized_ && success_;
  }

  void AddObserver(PrefStore::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(PrefStore::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void CompleteInitialization(bool success) {
    initialized_ = true;
    success_ = success;
    for (auto& observer : observers_) {
      // Some pref stores report completing initialization more than once. Test
      // that additional calls are ignored.
      observer.OnInitializationCompleted(success);
      observer.OnInitializationCompleted(success);
    }
  }

  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override {
    ValueMapPrefStore::SetValue(key, std::move(value), flags);
    for (auto& observer : observers_) {
      observer.OnPrefValueChanged(key);
    }
  }

 private:
  ~MockPrefStore() override = default;

  bool initialized_ = false;
  bool success_ = false;
  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
};

void ExpectInitializationComplete(PrefStore* pref_store, bool success) {
  PrefStoreObserverMock observer;
  pref_store->AddObserver(&observer);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnPrefValueChanged("")).Times(0);
  EXPECT_CALL(observer, OnInitializationCompleted(success))
      .WillOnce(testing::WithoutArgs(
          testing::Invoke([&run_loop]() { run_loop.Quit(); })));
  run_loop.Run();
  pref_store->RemoveObserver(&observer);
}

class PrefStoreImplTest : public testing::Test {
 public:
  PrefStoreImplTest() = default;

  // testing::Test:
  void TearDown() override {
    pref_store_ = nullptr;
    base::RunLoop().RunUntilIdle();
    impl_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateImpl(
      scoped_refptr<PrefStore> backing_pref_store,
      std::vector<std::string> observed_prefs = std::vector<std::string>()) {
    impl_ = std::make_unique<PrefStoreImpl>(std::move(backing_pref_store));

    if (observed_prefs.empty())
      observed_prefs.insert(observed_prefs.end(),
                            {kDictionaryKey, kOtherDictionaryKey});
    pref_store_ = base::MakeRefCounted<PrefStoreClient>(
        impl_->AddObserver(observed_prefs));
  }

  PrefStore* pref_store() { return pref_store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<PrefStoreImpl> impl_;

  scoped_refptr<PrefStore> pref_store_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreImplTest);
};

TEST_F(PrefStoreImplTest, InitializationSuccess) {
  auto backing_pref_store = base::MakeRefCounted<MockPrefStore>();
  backing_pref_store->SetValue(kDictionaryKey,
                               std::make_unique<base::Value>("value"), 0);
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  backing_pref_store->CompleteInitialization(true);
  ExpectInitializationComplete(pref_store(), true);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
}

TEST_F(PrefStoreImplTest, InitializationFailure) {
  auto backing_pref_store = base::MakeRefCounted<MockPrefStore>();
  backing_pref_store->SetValue(kDictionaryKey,
                               std::make_unique<base::Value>("value"), 0);
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  backing_pref_store->CompleteInitialization(false);
  ExpectInitializationComplete(pref_store(), false);

  // TODO(sammc): Should IsInitializationComplete() return false?
  EXPECT_TRUE(pref_store()->IsInitializationComplete());
}

TEST_F(PrefStoreImplTest, ValueChangesBeforeInitializationCompletes) {
  auto backing_pref_store = base::MakeRefCounted<MockPrefStore>();
  CreateImpl(backing_pref_store);
  EXPECT_FALSE(pref_store()->IsInitializationComplete());

  const base::Value value("value");
  backing_pref_store->SetValue(kDictionaryKey, value.CreateDeepCopy(), 0);
  backing_pref_store->CompleteInitialization(true);

  // The update occurs before initialization has completed, so should not
  // trigger notifications to client observers, but the value should be
  // observable once initialization completes.
  ExpectInitializationComplete(pref_store(), true);
  EXPECT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, InitialValue) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  const base::Value value("value");
  backing_pref_store->SetValue(kDictionaryKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, InitialValueWithoutPathExpansion) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  base::DictionaryValue dict;
  dict.SetKey(kDictionaryKey, base::Value("value"));
  backing_pref_store->SetValue(kDictionaryKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PrefStoreImplTest, WriteObservedByClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value value("value");
  backing_pref_store->SetValue(kDictionaryKey, value.CreateDeepCopy(), 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(value.Equals(output));
}

TEST_F(PrefStoreImplTest, WriteToUnregisteredPrefNotObservedByClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  CreateImpl(backing_pref_store, {kDictionaryKey});
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  backing_pref_store->SetValue(kOtherDictionaryKey,
                               std::make_unique<base::Value>(123), 0);
  backing_pref_store->SetValue(kDictionaryKey,
                               std::make_unique<base::Value>("value"), 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
  EXPECT_FALSE(pref_store()->GetValue(kOtherDictionaryKey, nullptr));
}

TEST_F(PrefStoreImplTest, WriteWithoutPathExpansionObservedByClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  base::DictionaryValue dict;
  dict.SetKey(kDictionaryKey, base::Value("value"));
  backing_pref_store->SetValue(kDictionaryKey, dict.CreateDeepCopy(), 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(dict.Equals(output));
}

TEST_F(PrefStoreImplTest, RemoveObservedByClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  const base::Value value("value");
  backing_pref_store->SetValue(kDictionaryKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(value.Equals(output));
  backing_pref_store->RemoveValue(kDictionaryKey, 0);

  // This should be a no-op and shouldn't trigger a notification for the other
  // client.
  backing_pref_store->RemoveValue(kDictionaryKey, 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
  EXPECT_FALSE(pref_store()->GetValue(kDictionaryKey, &output));
}

TEST_F(PrefStoreImplTest, RemoveOfUnregisteredPrefNotObservedByClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  const base::Value value("value");
  backing_pref_store->SetValue(kDictionaryKey, value.CreateDeepCopy(), 0);
  backing_pref_store->SetValue(kOtherDictionaryKey, value.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store, {kDictionaryKey});
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  backing_pref_store->RemoveValue(kOtherDictionaryKey, 0);
  backing_pref_store->RemoveValue(kDictionaryKey, 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
}

TEST_F(PrefStoreImplTest, RemoveWithoutPathExpansionObservedByOtherClient) {
  auto backing_pref_store = base::MakeRefCounted<ValueMapPrefStore>();
  base::DictionaryValue dict;
  dict.SetKey(kDictionaryKey, base::Value("value"));
  backing_pref_store->SetValue(kDictionaryKey, dict.CreateDeepCopy(), 0);
  CreateImpl(backing_pref_store);
  ASSERT_TRUE(pref_store()->IsInitializationComplete());

  const base::Value* output = nullptr;
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  EXPECT_TRUE(dict.Equals(output));

  base::Value* mutable_value = nullptr;
  dict.SetKey(kDictionaryKey, base::Value("value"));
  ASSERT_TRUE(
      backing_pref_store->GetMutableValue(kDictionaryKey, &mutable_value));
  base::DictionaryValue* mutable_dict = nullptr;
  ASSERT_TRUE(mutable_value->GetAsDictionary(&mutable_dict));
  mutable_dict->RemoveWithoutPathExpansion(kDictionaryKey, nullptr);
  backing_pref_store->ReportValueChanged(kDictionaryKey, 0);

  ExpectPrefChange(pref_store(), kDictionaryKey);
  ASSERT_TRUE(pref_store()->GetValue(kDictionaryKey, &output));
  const base::DictionaryValue* dict_value = nullptr;
  ASSERT_TRUE(output->GetAsDictionary(&dict_value));
  EXPECT_TRUE(dict_value->empty());
}

}  // namespace
}  // namespace prefs
