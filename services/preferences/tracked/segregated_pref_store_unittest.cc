// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/segregated_pref_store.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSelectedPref[] = "selected_pref";
const char kUnselectedPref[] = "unselected_pref";
const char kSharedPref[] = "shared_pref";

const char kValue1[] = "value1";
const char kValue2[] = "value2";

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  struct Data {
    Data(bool invoked_in, PersistentPrefStore::PrefReadError read_error_in)
        : invoked(invoked_in), read_error(read_error_in) {}

    bool invoked;
    PersistentPrefStore::PrefReadError read_error;
  };

  explicit MockReadErrorDelegate(Data* data) : data_(data) {
    DCHECK(data_);
    EXPECT_FALSE(data_->invoked);
  }

  // PersistentPrefStore::ReadErrorDelegate implementation
  void OnError(PersistentPrefStore::PrefReadError read_error) override {
    EXPECT_FALSE(data_->invoked);
    data_->invoked = true;
    data_->read_error = read_error;
  }

 private:
  Data* data_;
};

enum class CommitPendingWriteMode {
  // Basic mode.
  WITHOUT_CALLBACK,
  // With reply callback.
  WITH_CALLBACK,
  // With synchronous notify callback (synchronous after the write -- shouldn't
  // require pumping messages to observe).
  WITH_SYNCHRONOUS_CALLBACK,
};

class SegregatedPrefStoreTest
    : public testing::TestWithParam<CommitPendingWriteMode> {
 public:
  SegregatedPrefStoreTest()
      : read_error_delegate_data_(false,
                                  PersistentPrefStore::PREF_READ_ERROR_NONE),
        read_error_delegate_(
            new MockReadErrorDelegate(&read_error_delegate_data_)) {}

  void SetUp() override {
    selected_store_ = new TestingPrefStore;
    default_store_ = new TestingPrefStore;

    std::set<std::string> selected_pref_names;
    selected_pref_names.insert(kSelectedPref);
    selected_pref_names.insert(kSharedPref);

    segregated_store_ = new SegregatedPrefStore(
        default_store_, selected_store_, selected_pref_names,
        mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>());

    segregated_store_->AddObserver(&observer_);
  }

  void TearDown() override { segregated_store_->RemoveObserver(&observer_); }

 protected:
  std::unique_ptr<PersistentPrefStore::ReadErrorDelegate>
  GetReadErrorDelegate() {
    EXPECT_TRUE(read_error_delegate_);
    return std::move(read_error_delegate_);
  }

  base::test::TaskEnvironment task_environment_;

  PrefStoreObserverMock observer_;

  scoped_refptr<TestingPrefStore> default_store_;
  scoped_refptr<TestingPrefStore> selected_store_;
  scoped_refptr<SegregatedPrefStore> segregated_store_;

  MockReadErrorDelegate::Data read_error_delegate_data_;

 private:
  std::unique_ptr<MockReadErrorDelegate> read_error_delegate_;
};

}  // namespace

TEST_P(SegregatedPrefStoreTest, StoreValues) {
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->ReadPrefs());

  // Properly stores new values.
  segregated_store_->SetValue(kSelectedPref,
                              std::make_unique<base::Value>(kValue1),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  segregated_store_->SetValue(kUnselectedPref,
                              std::make_unique<base::Value>(kValue2),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  ASSERT_TRUE(selected_store_->GetValue(kSelectedPref, NULL));
  ASSERT_FALSE(selected_store_->GetValue(kUnselectedPref, NULL));
  ASSERT_FALSE(default_store_->GetValue(kSelectedPref, NULL));
  ASSERT_TRUE(default_store_->GetValue(kUnselectedPref, NULL));

  ASSERT_TRUE(segregated_store_->GetValue(kSelectedPref, NULL));
  ASSERT_TRUE(segregated_store_->GetValue(kUnselectedPref, NULL));

  ASSERT_FALSE(selected_store_->committed());
  ASSERT_FALSE(default_store_->committed());

  switch (GetParam()) {
    case CommitPendingWriteMode::WITHOUT_CALLBACK: {
      segregated_store_->CommitPendingWrite();
      base::RunLoop().RunUntilIdle();
      break;
    }

    case CommitPendingWriteMode::WITH_CALLBACK: {
      base::RunLoop run_loop;
      segregated_store_->CommitPendingWrite(run_loop.QuitClosure());
      run_loop.Run();
      break;
    }

    case CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK: {
      base::WaitableEvent written;
      segregated_store_->CommitPendingWrite(
          base::OnceClosure(),
          base::BindOnce(&base::WaitableEvent::Signal, Unretained(&written)));
      written.Wait();
      break;
    }
  }

  ASSERT_TRUE(selected_store_->committed());
  ASSERT_TRUE(default_store_->committed());
}

TEST_F(SegregatedPrefStoreTest, ReadValues) {
  selected_store_->SetValue(kSelectedPref,
                            std::make_unique<base::Value>(kValue1),
                            WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  default_store_->SetValue(kUnselectedPref,
                           std::make_unique<base::Value>(kValue2),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Works properly with values that are already there.
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->ReadPrefs());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->GetReadError());

  ASSERT_TRUE(selected_store_->GetValue(kSelectedPref, NULL));
  ASSERT_FALSE(selected_store_->GetValue(kUnselectedPref, NULL));
  ASSERT_FALSE(default_store_->GetValue(kSelectedPref, NULL));
  ASSERT_TRUE(default_store_->GetValue(kUnselectedPref, NULL));

  ASSERT_TRUE(segregated_store_->GetValue(kSelectedPref, NULL));
  ASSERT_TRUE(segregated_store_->GetValue(kUnselectedPref, NULL));
}

TEST_F(SegregatedPrefStoreTest, RemoveValuesByPrefix) {
  const std::string subpref_name1 = kSelectedPref;
  const std::string subpref_name2 = std::string(kSelectedPref) + "b";
  const std::string other_name = kUnselectedPref;
  const std::string prefix = kSelectedPref;

  selected_store_->SetValue(subpref_name1,
                            std::make_unique<base::Value>(kValue1),
                            WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  default_store_->SetValue(subpref_name2,
                           std::make_unique<base::Value>(kValue2),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  default_store_->SetValue(other_name, std::make_unique<base::Value>(kValue2),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  ASSERT_TRUE(selected_store_->GetValue(subpref_name1, nullptr));
  ASSERT_TRUE(default_store_->GetValue(subpref_name2, nullptr));
  ASSERT_TRUE(default_store_->GetValue(other_name, nullptr));

  segregated_store_->RemoveValuesByPrefixSilently(kSelectedPref);

  ASSERT_FALSE(selected_store_->GetValue(subpref_name1, nullptr));
  ASSERT_FALSE(default_store_->GetValue(subpref_name2, nullptr));
  ASSERT_TRUE(default_store_->GetValue(other_name, nullptr));
}

TEST_F(SegregatedPrefStoreTest, Observer) {
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->ReadPrefs());
  EXPECT_TRUE(observer_.initialized);
  EXPECT_TRUE(observer_.initialization_success);
  EXPECT_TRUE(observer_.changed_keys.empty());
  segregated_store_->SetValue(kSelectedPref,
                              std::make_unique<base::Value>(kValue1),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  observer_.VerifyAndResetChangedKey(kSelectedPref);
  segregated_store_->SetValue(kUnselectedPref,
                              std::make_unique<base::Value>(kValue2),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  observer_.VerifyAndResetChangedKey(kUnselectedPref);
}

TEST_F(SegregatedPrefStoreTest, SelectedPrefReadNoFileError) {
  // PREF_READ_ERROR_NO_FILE for the selected prefs file is silently converted
  // to PREF_READ_ERROR_NONE.
  selected_store_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->ReadPrefs());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, SelectedPrefReadError) {
  selected_store_->set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED,
            segregated_store_->ReadPrefs());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, SelectedPrefReadNoFileErrorAsync) {
  // PREF_READ_ERROR_NO_FILE for the selected prefs file is silently converted
  // to PREF_READ_ERROR_NONE.
  selected_store_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);

  default_store_->SetBlockAsyncRead(true);

  EXPECT_FALSE(read_error_delegate_data_.invoked);

  segregated_store_->ReadPrefsAsync(GetReadErrorDelegate().release());

  EXPECT_FALSE(read_error_delegate_data_.invoked);

  default_store_->SetBlockAsyncRead(false);

  // ReadErrorDelegate is not invoked for ERROR_NONE.
  EXPECT_FALSE(read_error_delegate_data_.invoked);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->GetReadError());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, UnselectedPrefReadNoFileError) {
  default_store_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->ReadPrefs());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, UnselectedPrefReadError) {
  default_store_->set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED,
            segregated_store_->ReadPrefs());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, BothPrefReadError) {
  default_store_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);
  selected_store_->set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->ReadPrefs());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, BothPrefReadErrorAsync) {
  default_store_->set_read_error(PersistentPrefStore::PREF_READ_ERROR_NO_FILE);
  selected_store_->set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);

  selected_store_->SetBlockAsyncRead(true);

  EXPECT_FALSE(read_error_delegate_data_.invoked);

  segregated_store_->ReadPrefsAsync(GetReadErrorDelegate().release());

  EXPECT_FALSE(read_error_delegate_data_.invoked);

  selected_store_->SetBlockAsyncRead(false);

  EXPECT_TRUE(read_error_delegate_data_.invoked);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->GetReadError());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            segregated_store_->GetReadError());
}

TEST_F(SegregatedPrefStoreTest, IsInitializationComplete) {
  EXPECT_FALSE(segregated_store_->IsInitializationComplete());
  segregated_store_->ReadPrefs();
  EXPECT_TRUE(segregated_store_->IsInitializationComplete());
}

TEST_F(SegregatedPrefStoreTest, IsInitializationCompleteAsync) {
  selected_store_->SetBlockAsyncRead(true);
  default_store_->SetBlockAsyncRead(true);
  EXPECT_FALSE(segregated_store_->IsInitializationComplete());
  segregated_store_->ReadPrefsAsync(NULL);
  EXPECT_FALSE(segregated_store_->IsInitializationComplete());
  selected_store_->SetBlockAsyncRead(false);
  EXPECT_FALSE(segregated_store_->IsInitializationComplete());
  default_store_->SetBlockAsyncRead(false);
  EXPECT_TRUE(segregated_store_->IsInitializationComplete());
}

TEST_F(SegregatedPrefStoreTest, GetValues) {
  // To check merge behavior, create selected and default stores so each has a
  // key the other doesn't have and they have one key in common.
  selected_store_->SetValue(kSelectedPref,
                            std::make_unique<base::Value>(kValue1),
                            WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  default_store_->SetValue(kUnselectedPref,
                           std::make_unique<base::Value>(kValue2),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  selected_store_->SetValue(kSharedPref, std::make_unique<base::Value>(kValue1),
                            WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  auto values = segregated_store_->GetValues();
  const base::Value* value = nullptr;
  // Check that a selected preference is returned.
  ASSERT_TRUE(values->Get(kSelectedPref, &value));
  EXPECT_TRUE(base::Value(kValue1).Equals(value));

  // Check that a a default preference is returned.
  ASSERT_TRUE(values->Get(kUnselectedPref, &value));
  EXPECT_TRUE(base::Value(kValue2).Equals(value));

  // Check that the selected preference is preferred.
  ASSERT_TRUE(values->Get(kSharedPref, &value));
  EXPECT_TRUE(base::Value(kValue1).Equals(value));
}

INSTANTIATE_TEST_SUITE_P(
    WithoutCallback,
    SegregatedPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITHOUT_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithCallback,
    SegregatedPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITH_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithSynchronousCallback,
    SegregatedPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK));
