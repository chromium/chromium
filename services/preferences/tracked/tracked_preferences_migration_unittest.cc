// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_preferences_migration.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/testing_pref_service.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/interceptable_pref_filter.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/pref_hash_store_impl.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// An unprotected pref.
const char kUnprotectedPref[] = "unprotected";
// A protected pref.
const char kProtectedPref[] = "protected";
// A protected pref which is initially stored in the unprotected store.
const char kPreviouslyUnprotectedPref[] = "previously.unprotected";
// An unprotected pref which is initially stored in the protected store.
const char kPreviouslyProtectedPref[] = "previously.protected";

const char kUnprotectedPrefValue[] = "unprotected_value";
const char kProtectedPrefValue[] = "protected_value";
const char kPreviouslyUnprotectedPrefValue[] = "previously_unprotected_value";
const char kPreviouslyProtectedPrefValue[] = "previously_protected_value";

// A simple InterceptablePrefFilter which doesn't do anything but hand the prefs
// back downstream in FinalizeFilterOnLoad.
class SimpleInterceptablePrefFilter final : public InterceptablePrefFilter {
 public:
  // PrefFilter remaining implementation.
  void FilterUpdate(std::string_view path) override { ADD_FAILURE(); }
  OnWriteCallbackPair FilterSerializeData(
      base::DictValue& pref_store_contents) override {
    ADD_FAILURE();
    return std::make_pair(base::OnceClosure(),
                          base::OnceCallback<void(bool success)>());
  }

  void SetPrefService(PrefService* pref_service) override {}

  void SetMigratedPaths(base::span<const std::string> migrated_paths) override {
    migrated_paths_.assign(migrated_paths.begin(), migrated_paths.end());
  }

  const std::vector<std::string>& migrated_paths() const {
    return migrated_paths_;
  }

  void ClearMigratedPaths() { migrated_paths_.clear(); }

 private:
  // InterceptablePrefFilter implementation.
  void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      base::DictValue pref_store_contents,
      bool prefs_altered) override {
    std::move(post_filter_on_load_callback)
        .Run(std::move(pref_store_contents), prefs_altered);
  }

  void OnEncryptorReceived(
      scoped_refptr<os_crypt_async::Encryptor> encryptor) override {}

  base::WeakPtr<InterceptablePrefFilter> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::vector<std::string> migrated_paths_;
  base::WeakPtrFactory<InterceptablePrefFilter> weak_ptr_factory_{this};
};

// A test fixture designed to be used like this:
//  1) Set up initial store prefs with PresetStoreValue().
//  2) Hand both sets of prefs to the migrator via HandPrefsToMigrator().
//  3) Migration completes synchronously when the second store hands its prefs
//     over.
//  4) Verifications can be made via various methods of this fixture.
// Call Reset() to perform a second migration.
class TrackedPreferencesMigrationTest : public testing::Test {
 public:
  enum MockPrefStoreID {
    MOCK_UNPROTECTED_PREF_STORE,
    MOCK_PROTECTED_PREF_STORE,
  };

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  TrackedPreferencesMigrationTest()
      : unprotected_prefs_(new base::DictValue),
        protected_prefs_(new base::DictValue),
        migration_modified_unprotected_store_(false),
        migration_modified_protected_store_(false),
        unprotected_store_migration_complete_(false),
        protected_store_migration_complete_(false) {}

  TrackedPreferencesMigrationTest(const TrackedPreferencesMigrationTest&) =
      delete;
  TrackedPreferencesMigrationTest& operator=(
      const TrackedPreferencesMigrationTest&) = delete;

  void SetUp() override { Reset(); }

  void Reset(bool has_init_prefs = false) {
    PrefNameSet unprotected_pref_names;
    PrefNameSet protected_pref_names;
    unprotected_pref_names.insert(kUnprotectedPref);
    unprotected_pref_names.insert(kPreviouslyProtectedPref);
    protected_pref_names.insert(kProtectedPref);
    protected_pref_names.insert(kPreviouslyUnprotectedPref);

    migration_modified_unprotected_store_ = false;
    migration_modified_protected_store_ = false;
    unprotected_store_migration_complete_ = false;
    protected_store_migration_complete_ = false;

    unprotected_store_successful_write_callback_.Reset();
    protected_store_successful_write_callback_.Reset();

    mock_unprotected_pref_filter_ =
        std::make_unique<SimpleInterceptablePrefFilter>();
    mock_protected_pref_filter_ =
        std::make_unique<SimpleInterceptablePrefFilter>();

    SetupTrackedPreferencesMigration(
        unprotected_pref_names, protected_pref_names,
        base::BindRepeating(
            &TrackedPreferencesMigrationTest::RemovePathFromStore,
            base::Unretained(this), MOCK_UNPROTECTED_PREF_STORE),
        base::BindRepeating(
            &TrackedPreferencesMigrationTest::RemovePathFromStore,
            base::Unretained(this), MOCK_PROTECTED_PREF_STORE),
        base::BindRepeating(
            &TrackedPreferencesMigrationTest::RegisterSuccessfulWriteClosure,
            base::Unretained(this), MOCK_UNPROTECTED_PREF_STORE),
        base::BindRepeating(
            &TrackedPreferencesMigrationTest::RegisterSuccessfulWriteClosure,
            base::Unretained(this), MOCK_PROTECTED_PREF_STORE),
        std::unique_ptr<PrefHashStore>(
            new PrefHashStoreImpl(kSeed, false, false)),
        std::unique_ptr<PrefHashStore>(
            new PrefHashStoreImpl(kSeed, true, true)),
        mock_unprotected_pref_filter_.get(), mock_protected_pref_filter_.get(),
        has_init_prefs);

    // Verify initial expectations are met.
    EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
    EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
    EXPECT_FALSE(
        WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
    EXPECT_FALSE(
        WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  }

 protected:
  // Sets |key| to |value| in the test store identified by |store_id| before
  // migration begins. Also sets the corresponding hash in the same store.
  void PresetStoreValue(MockPrefStoreID store_id,
                        const std::string& key,
                        const std::string value) {
    PresetStoreValueOnly(store_id, key, value);
    PresetStoreValueHmac(store_id, key, value);
  }

  // Sets |key| to |value| in the test store identified by |store_id| before
  // migration begins. Does not store a preference hash.
  void PresetStoreValueOnly(MockPrefStoreID store_id,
                            const std::string& key,
                            const std::string value) {
    base::DictValue* store = nullptr;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);

    store->SetByDottedPath(key, value);
  }

  bool ContainsValue(MockPrefStoreID store_id, const std::string& key) {
    base::DictValue* store = nullptr;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);
    return store->FindByDottedPath(key) != nullptr;
  }

  const std::vector<std::string>& GetMigratedProtectedPaths() const {
    return mock_protected_pref_filter_->migrated_paths();
  }

  // Stores an HMAC for |key| and |value| in the hash store identified by
  // |store_id| before migration begins.
  void PresetStoreValueHmac(MockPrefStoreID store_id,
                            const std::string& key,
                            const std::string value) {
    base::DictValue* store = nullptr;
    std::unique_ptr<PrefHashStore> pref_hash_store;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        pref_hash_store =
            std::make_unique<PrefHashStoreImpl>(kSeed, false, false);
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        pref_hash_store =
            std::make_unique<PrefHashStoreImpl>(kSeed, true, true);
        break;
    }
    DCHECK(store);

    base::Value string_value(value);
    DictionaryHashStoreContents contents(*store);
    pref_hash_store->BeginTransaction(&contents)->StoreHmac(key, &string_value);
  }

  // Returns true if the store opposite to |store_id| is observed for its next
  // successful write.
  bool WasOnSuccessfulWriteCallbackRegistered(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return !protected_store_successful_write_callback_.is_null();
      case MOCK_PROTECTED_PREF_STORE:
        return !unprotected_store_successful_write_callback_.is_null();
    }
    NOTREACHED();
  }

  // Verifies that the (key, value) pairs in |expected_prefs_in_store| are found
  // in the store identified by |store_id|.
  void VerifyValuesStored(MockPrefStoreID store_id,
                          const base::StringPairs& expected_prefs_in_store) {
    base::DictValue* store = nullptr;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);

    for (base::StringPairs::const_iterator it = expected_prefs_in_store.begin();
         it != expected_prefs_in_store.end(); ++it) {
      const std::string* val = store->FindStringByDottedPath(it->first);
      ASSERT_TRUE(val);
      EXPECT_EQ(it->second, *val);
    }
  }

  // Determines whether |expected_pref_in_hash_store| has a hash in the hash
  // store identified by |store_id|.
  bool ContainsHash(MockPrefStoreID store_id,
                    std::string expected_pref_in_hash_store) {
    base::DictValue* store = nullptr;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);
    const base::DictValue* hash_store_contents =
        DictionaryHashStoreContents(*store).GetContents();
    return hash_store_contents && hash_store_contents->FindStringByDottedPath(
                                      expected_pref_in_hash_store);
  }

  // Both stores need to hand their prefs over in order for migration to kick
  // in.
  void HandPrefsToMigrator(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE: {
        std::unique_ptr<base::DictValue> unprotected_prefs =
            std::move(unprotected_prefs_);
        mock_unprotected_pref_filter_->FilterOnLoad(
            base::BindOnce(&TrackedPreferencesMigrationTest::GetPrefsBack,
                           base::Unretained(this), MOCK_UNPROTECTED_PREF_STORE),
            std::move(*unprotected_prefs));
        break;
      }
      case MOCK_PROTECTED_PREF_STORE: {
        std::unique_ptr<base::DictValue> protected_prefs =
            std::move(protected_prefs_);
        mock_protected_pref_filter_->FilterOnLoad(
            base::BindOnce(&TrackedPreferencesMigrationTest::GetPrefsBack,
                           base::Unretained(this), MOCK_PROTECTED_PREF_STORE),
            std::move(*protected_prefs));
        break;
      }
    }
  }

  bool HasPrefs(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return !!unprotected_prefs_;
      case MOCK_PROTECTED_PREF_STORE:
        return !!protected_prefs_;
    }
    NOTREACHED();
  }

  bool StoreModifiedByMigration(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return migration_modified_unprotected_store_;
      case MOCK_PROTECTED_PREF_STORE:
        return migration_modified_protected_store_;
    }
    NOTREACHED();
  }

  bool MigrationCompleted() {
    return unprotected_store_migration_complete_ &&
           protected_store_migration_complete_;
  }

  void SimulateSuccessfulWrite(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_FALSE(unprotected_store_successful_write_callback_.is_null());
        std::move(unprotected_store_successful_write_callback_).Run();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_FALSE(protected_store_successful_write_callback_.is_null());
        std::move(protected_store_successful_write_callback_).Run();
        break;
    }
  }

 private:
  void RegisterSuccessfulWriteClosure(
      MockPrefStoreID store_id,
      base::OnceClosure successful_write_closure) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_TRUE(unprotected_store_successful_write_callback_.is_null());
        unprotected_store_successful_write_callback_ =
            std::move(successful_write_closure);
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_TRUE(protected_store_successful_write_callback_.is_null());
        protected_store_successful_write_callback_ =
            std::move(successful_write_closure);
        break;
    }
  }

  // Helper given as an InterceptablePrefFilter::FinalizeFilterOnLoadCallback
  // to the migrator to be invoked when it's done.
  void GetPrefsBack(MockPrefStoreID store_id,
                    base::DictValue prefs,
                    bool prefs_altered) {
    auto prefs_ptr = std::make_unique<base::DictValue>(std::move(prefs));
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_FALSE(unprotected_prefs_);
        unprotected_prefs_ = std::move(prefs_ptr);
        migration_modified_unprotected_store_ = prefs_altered;
        unprotected_store_migration_complete_ = true;
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_FALSE(protected_prefs_);
        protected_prefs_ = std::move(prefs_ptr);
        migration_modified_protected_store_ = prefs_altered;
        protected_store_migration_complete_ = true;
        break;
    }
  }

  // Helper given as a cleaning callback to the migrator.
  void RemovePathFromStore(MockPrefStoreID store_id, const std::string& key) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        ASSERT_TRUE(unprotected_prefs_);
        unprotected_prefs_->RemoveByDottedPath(key);
        break;
      case MOCK_PROTECTED_PREF_STORE:
        ASSERT_TRUE(protected_prefs_);
        protected_prefs_->RemoveByDottedPath(key);
        break;
    }
  }

  static const char kSeed[];

  std::unique_ptr<base::DictValue> unprotected_prefs_;
  std::unique_ptr<base::DictValue> protected_prefs_;

  std::unique_ptr<SimpleInterceptablePrefFilter> mock_unprotected_pref_filter_;
  std::unique_ptr<SimpleInterceptablePrefFilter> mock_protected_pref_filter_;

  base::OnceClosure unprotected_store_successful_write_callback_;
  base::OnceClosure protected_store_successful_write_callback_;

  bool migration_modified_unprotected_store_;
  bool migration_modified_protected_store_;

  bool unprotected_store_migration_complete_;
  bool protected_store_migration_complete_;

  TestingPrefServiceSimple local_state_;
};

// static
const char TrackedPreferencesMigrationTest::kSeed[] = "seed";

}  // namespace

TEST_F(TrackedPreferencesMigrationTest, NoMigrationRequired) {
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref,
                   kUnprotectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kProtectedPref,
                   kProtectedPrefValue);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  // Hand unprotected prefs to the migrator which should wait for the protected
  // prefs.
  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  // Hand protected prefs to the migrator which should proceed with the
  // migration synchronously.
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Prefs should have been handed back over.
  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  base::StringPairs expected_unprotected_values;
  expected_unprotected_values.push_back(
      std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
  VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE, expected_unprotected_values);

  base::StringPairs expected_protected_values;
  expected_protected_values.push_back(
      std::make_pair(kProtectedPref, kProtectedPrefValue));
  VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
}

TEST_F(TrackedPreferencesMigrationTest, FullMigration) {
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref,
                   kUnprotectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kProtectedPref,
                   kProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_TRUE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  // Unprotected prefs in the protected store are migrated to the unprotected
  // store (and remain in the protected store until the unprotected store is
  // written). Protected prefs in the unprotected store are not migrated because
  // initial preferences were not seeded in this process (has_init_prefs is
  // false), and are cleaned up immediately.
  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(
        std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);
    EXPECT_FALSE(
        ContainsValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(
        std::make_pair(kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
    EXPECT_FALSE(
        ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));

    EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
    EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

    EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
    EXPECT_FALSE(
        ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
    EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));
  }

  SimulateSuccessfulWrite(MOCK_UNPROTECTED_PREF_STORE);

  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(
        std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(
        std::make_pair(kProtectedPref, kProtectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
    EXPECT_FALSE(
        ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));
  }

  // Hashes are not cleaned up yet.
  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  Reset();

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Hashes are cleaned up.
  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));
}

TEST_F(TrackedPreferencesMigrationTest, CleanupOnly) {
  // Already migrated; only cleanup needed.
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref,
                   kUnprotectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kProtectedPref,
                   kProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  // Cleanup should happen synchronously if the values were already present in
  // their destination stores.
  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(
        std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(
        std::make_pair(kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
  }
}

// Verifies that an unauthenticated preference in the unprotected store is not
// migrated to the protected store even when migration is allowed and the
// protected store lacks an authenticator for that preference. See
// https://crbug.com/574078135 for context.
TEST_F(TrackedPreferencesMigrationTest,
       UnauthenticatedValueNotMigratedWhenDestinationMissingAuth) {
  Reset(/*has_init_prefs=*/true);

  PresetStoreValueOnly(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                       kPreviouslyUnprotectedPrefValue);

  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_FALSE(
      ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(GetMigratedProtectedPaths().empty());
}

// Verifies that SetMigratedPaths is invoked on the protected pref filter when
// the protected pref store was initially empty AND
// InitializeMasterPrefsTracking seeded initial_preferences in this process.
// See https://crbug.com/574078135 for context.
TEST_F(TrackedPreferencesMigrationTest,
       SetMigratedPathsWhenProtectedStoreInitiallyEmpty) {
  Reset(/*has_init_prefs=*/true);

  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_TRUE(
      ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  ASSERT_EQ(1u, GetMigratedProtectedPaths().size());
  EXPECT_EQ(kPreviouslyUnprotectedPref, GetMigratedProtectedPaths()[0]);
}

// Verifies that SetMigratedPaths is NOT invoked and protected preferences are
// not migrated when the protected pref store is empty on disk (e.g. truncated
// or deleted by an offline attacker) without InitializeMasterPrefsTracking
// having run in this process. See https://crbug.com/574078135 for context.
TEST_F(TrackedPreferencesMigrationTest,
       BlockMigrationWhenProtectedStoreTruncatedWithoutInitPrefs) {
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_FALSE(
      ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(GetMigratedProtectedPaths().empty());
}

// Verifies that SetMigratedPaths is not invoked and protected preferences are
// not migrated when the protected pref store already contains data
// (subsequent browser startups). See https://crbug.com/574078135 for context.
TEST_F(TrackedPreferencesMigrationTest,
       DoNotSetMigratedPathsWhenProtectedStoreInitiallyNonEmpty) {
  Reset(/*has_init_prefs=*/true);

  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kProtectedPref,
                   kProtectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_FALSE(
      ContainsValue(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(GetMigratedProtectedPaths().empty());
}
