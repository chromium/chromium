// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_filter.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_browsing/core/common/features.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/preferences/public/cpp/tracked/configuration.h"
#include "services/preferences/public/cpp/tracked/mock_validation_delegate.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/tracked/features.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using EnforcementLevel =
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel;
using PrefTrackingStrategy =
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy;
using ValueType = prefs::mojom::TrackedPreferenceMetadata::ValueType;
using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

const char kAtomicPref[] = "atomic_pref";
const char kAtomicPref2[] = "atomic_pref2";
const char kAtomicPref3[] = "pref3";
const char kAtomicPref4[] = "pref4";
const char kDeprecatedTrackedDictionaryEntry[] = "dictionary.pref";
const char kDeprecatedUntrackedDictionary[] = "dictionary";
const char kReportOnlyPref[] = "report_only";
const char kReportOnlySplitPref[] = "report_only_split_pref";
const char kSplitPref[] = "split_pref";
const char kScheduleToFlushToDisk[] = "schedule_to_flush_to_disk";

const prefs::TrackedPreferenceMetadata kTestTrackedPrefs[] = {
    {0, kAtomicPref, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
    {1, kReportOnlyPref, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {2, kSplitPref, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::SPLIT, ValueType::IMPERSONAL},
    {3, kReportOnlySplitPref, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::SPLIT, ValueType::IMPERSONAL},
    {4, kAtomicPref2, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {5, kAtomicPref3, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {6, kAtomicPref4, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
};

// Defines a test case for a tracked preference type mismatch.
struct TypeMismatchTestCase {
  size_t reporting_id;
  const char* pref_name;
  PrefTrackingStrategy strategy;
  base::Value::Type registered_type;
  base::Value::Type mismatch_type;
  const char* mismatch_string_value = nullptr;
};

// A helper function to return the test cases for type mismatch tests.
std::vector<TypeMismatchTestCase> GetTypeMismatchTestCases() {
  return {
      {0, "browser.show_home_button", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::BOOLEAN, base::Value::Type::DICT},
      {1, "homepage_is_newtabpage", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::BOOLEAN, base::Value::Type::DICT},
      {2, "homepage", PrefTrackingStrategy::ATOMIC, base::Value::Type::STRING,
       base::Value::Type::DICT},
      {3, "session.restore_on_startup", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::INTEGER, base::Value::Type::DICT},
      {4, "session.startup_urls", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::LIST, base::Value::Type::DICT},
      {6, "google.services.last_syncing_username", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::STRING, base::Value::Type::DICT},
      {7, "search_provider_overrides", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::LIST, base::Value::Type::DICT},
      {11, "pinned_tabs", PrefTrackingStrategy::ATOMIC, base::Value::Type::LIST,
       base::Value::Type::DICT},
      {14, "default_search_provider_data.template_url_data",
       PrefTrackingStrategy::ATOMIC, base::Value::Type::DICT,
       base::Value::Type::LIST},
      {15, "profile.preference_reset_time", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::STRING, base::Value::Type::DICT},
      {18, "safebrowsing.incidents_sent", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::DICT, base::Value::Type::LIST},
      {23, "google.services.account_id", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::STRING, base::Value::Type::DICT},
      {29, "media.storage_id_salt", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::STRING, base::Value::Type::DICT},
      {32, "media.cdm.origin_data", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::DICT, base::Value::Type::LIST},
      {33, "google.services.last_signed_in_username",
       PrefTrackingStrategy::ATOMIC, base::Value::Type::STRING,
       base::Value::Type::DICT},
      {34, "enterprise_signin.policy_recovery_token",
       PrefTrackingStrategy::ATOMIC, base::Value::Type::STRING,
       base::Value::Type::DICT},
      {35, "extensions.ui.developer_mode", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::BOOLEAN, base::Value::Type::DICT},
      {36, "preference_reset_schedule_to_flush_to_disk",
       PrefTrackingStrategy::ATOMIC, base::Value::Type::STRING,
       base::Value::Type::DICT},
      {37, "extensions.install.init_list", PrefTrackingStrategy::ATOMIC,
       base::Value::Type::LIST, base::Value::Type::DICT},
      {38, "extensions.install.init_provider_name",
       PrefTrackingStrategy::ATOMIC, base::Value::Type::STRING,
       base::Value::Type::DICT},
  };
}

// Helper to duplicate the logic in pref_hash_filter.cc for test verification
const char* GetValueTypeStringForTest(base::Value::Type type) {
  using Type = base::Value::Type;
  switch (type) {
    case Type::NONE:
      return "None";
    case Type::BOOLEAN:
      return "Boolean";
    case Type::INTEGER:
      return "Integer";
    case Type::DOUBLE:
      return "Double";
    case Type::STRING:
      return "String";
    case Type::BINARY:
      return "Binary";
    case Type::DICT:
      return "Dictionary";
    case Type::LIST:
      return "List";
  }
  return "Unknown";
}

}  // namespace

// A PrefHashStore that allows simulation of CheckValue results and captures
// checked and stored values.
class MockPrefHashStore : public PrefHashStore {
 public:
  typedef std::pair<const void*, PrefTrackingStrategy> ValuePtrStrategyPair;

  MockPrefHashStore()
      : stamp_super_mac_result_(false),
        is_super_mac_valid_result_(false),
        store_encrypted_hash_called_(false),
        transactions_performed_(0),
        transaction_active_(false) {}

  MockPrefHashStore(const MockPrefHashStore&) = delete;
  MockPrefHashStore& operator=(const MockPrefHashStore&) = delete;

  ~MockPrefHashStore() override { EXPECT_FALSE(transaction_active_); }

  // Set the result that will be returned when |path| is passed to
  // |CheckValue/CheckSplitValue|.
  void SetCheckResult(const std::string& path, ValueState result);

  // Set the invalid_keys that will be returned when |path| is passed to
  // |CheckSplitValue|. SetCheckResult should already have been called for
  // |path| with |result == CHANGED| for this to make any sense.
  void SetInvalidKeysResult(
      const std::string& path,
      const std::vector<std::string>& invalid_keys_result);

  // Sets the value that will be returned from
  // PrefHashStoreTransaction::StampSuperMAC().
  void set_stamp_super_mac_result(bool result) {
    stamp_super_mac_result_ = result;
  }

  bool StoreEncryptedHashCalled() const { return store_encrypted_hash_called_; }

  void ClearTestState() { checked_values_.clear(); }

  // Sets the value that will be returned from
  // PrefHashStoreTransaction::IsSuperMACValid().
  void set_is_super_mac_valid_result(bool result) {
    is_super_mac_valid_result_ = result;
  }

  // Returns the number of transactions that were performed.
  size_t transactions_performed() { return transactions_performed_; }

  // Returns the number of paths checked.
  size_t checked_paths_count() const { return checked_values_.size(); }

  // Returns the number of paths stored.
  size_t stored_paths_count() const { return owned_stored_values_.size(); }

  // Returns the pointer value and strategy that was passed to
  // |CheckHash/CheckSplitHash| for |path|. The returned pointer could since
  // have been freed and is thus not safe to dereference.
  ValuePtrStrategyPair checked_value(const std::string& path) const {
    std::map<std::string, ValuePtrStrategyPair>::const_iterator value =
        checked_values_.find(path);
    if (value != checked_values_.end())
      return value->second;
    return std::make_pair(reinterpret_cast<void*>(0xBAD),
                          static_cast<PrefTrackingStrategy>(-1));
  }

  // Returns the pointer value that was passed to |StoreHash/StoreSplitHash| for
  // |path|. The returned pointer could since have been freed and is thus not
  // safe to dereference.
  ValuePtrStrategyPair stored_value(const std::string& path) const {
    auto it = owned_stored_values_.find(path);
    if (it != owned_stored_values_.end()) {
      return std::make_pair(it->second.first.get(), it->second.second);
    }
    return std::make_pair(reinterpret_cast<void*>(0xBAD),
                          static_cast<PrefTrackingStrategy>(-1));
  }

  // PrefHashStore implementation.
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      HashStoreContents* storage,
      const os_crypt_async::Encryptor* encryptor_ptr) override;
  std::string ComputeMac(const std::string& path,
                         const base::Value* new_value) override;
  base::Value::Dict ComputeSplitMacs(
      const std::string& path,
      const base::Value::Dict* split_values) override;
  std::string ComputeEncryptedHash(
      const std::string& path,
      const base::Value* value,
      const os_crypt_async::Encryptor* encryptor_ptr) override;
  std::string ComputeEncryptedHash(
      const std::string& path,
      const base::Value::Dict* dict,
      const os_crypt_async::Encryptor* encryptor_ptr) override;
  base::Value::Dict ComputeSplitEncryptedHashes(
      const std::string& path,
      const base::Value::Dict* split_values,
      const os_crypt_async::Encryptor* encryptor_ptr) override;

  void SetTransactionCompletionCallback(base::OnceClosure callback);

 private:
  // A MockPrefHashStoreTransaction is handed to the caller on
  // MockPrefHashStore::BeginTransaction(). It then stores state in its
  // underlying MockPrefHashStore about calls it receives from that same caller
  // which can later be verified in tests.
  class MockPrefHashStoreTransaction : public PrefHashStoreTransaction {
   public:
    explicit MockPrefHashStoreTransaction(MockPrefHashStore* outer)
        : outer_(outer) {}

    MockPrefHashStoreTransaction(const MockPrefHashStoreTransaction&) = delete;
    MockPrefHashStoreTransaction& operator=(
        const MockPrefHashStoreTransaction&) = delete;

    ~MockPrefHashStoreTransaction() override {
      outer_->transaction_active_ = false;
      ++outer_->transactions_performed_;
      if (outer_->completion_callback_) {
        // Post a task to avoid potential re-entrancy issues.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(outer_->completion_callback_));
      }
    }

    // PrefHashStoreTransaction implementation.
    std::string_view GetStoreUMASuffix() const override;
    ValueState CheckValue(const std::string& path,
                          const base::Value* value) const override;
    void StoreHash(const std::string& path,
                   const base::Value* new_value) override;
    ValueState CheckSplitValue(
        const std::string& path,
        const base::Value::Dict* initial_split_value,
        std::vector<std::string>* invalid_keys) const override;
    void StoreSplitHash(const std::string& path,
                        const base::Value::Dict* split_value) override;
    bool HasHash(const std::string& path) const override;
    void ImportHash(const std::string& path, const base::Value* hash) override;
    void ClearHash(const std::string& path) override;
    void ClearEncryptedHash(const std::string& path) override;
    bool IsSuperMACValid() const override;
    bool StampSuperMac() override;
    void StoreEncryptedHash(const std::string& path,
                            const base::Value* value) override {
      outer_->store_encrypted_hash_called_ = true;

      // Record this like a normal store operation in this simple mock.
      // Pass the base value pointer directly.
      outer_->RecordStoreHash(path, value, PrefTrackingStrategy::ATOMIC);
      // Note: This doesn't store under the derived key, which might affect
      // tests checking internal state directly, but suffices for making the
      // class non-abstract and allowing PrefHashFilter calls.
    }
    void StoreSplitEncryptedHash(const std::string& path,
                                 const base::Value::Dict* value) override {
      outer_->store_encrypted_hash_called_ = true;
      // Record this like a normal store operation in this simple mock.
      // Pass the base value pointer directly.
      outer_->RecordStoreHash(path, value, PrefTrackingStrategy::SPLIT);
    }
    std::optional<std::string> GetEncryptedHash(
        const std::string& path) const override {
      return std::nullopt;
    }
    std::optional<std::string> GetMac(const std::string& path) const override {
      return std::nullopt;
    }
    bool HasEncryptedHash(const std::string& path) const override {
      return false;
    }

   private:
    raw_ptr<MockPrefHashStore> outer_;
  };

  // Records a call to this mock's CheckValue/CheckSplitValue methods.
  ValueState RecordCheckValue(const std::string& path,
                              const void* value,
                              PrefTrackingStrategy strategy);

  // Records a call to this mock's StoreHash/StoreSplitHash methods.
  void RecordStoreHash(const std::string& path,
                       const void* new_value,
                       PrefTrackingStrategy strategy);
  void ClearStoreHash(const std::string& path);

  std::map<std::string, ValueState> check_results_;
  std::map<std::string, std::vector<std::string>> invalid_keys_results_;

  bool stamp_super_mac_result_;
  bool is_super_mac_valid_result_;
  bool store_encrypted_hash_called_;
  base::OnceClosure completion_callback_;

  std::map<std::string, ValuePtrStrategyPair> checked_values_;
  std::map<std::string, ValuePtrStrategyPair> stored_values_;
  std::map<std::string,
           std::pair<std::unique_ptr<base::Value>, PrefTrackingStrategy>>
      owned_stored_values_;

  // Number of transactions that were performed via this MockPrefHashStore.
  size_t transactions_performed_;

  // Whether a transaction is currently active (only one transaction should be
  // active at a time).
  bool transaction_active_;
};

void MockPrefHashStore::SetCheckResult(const std::string& path,
                                       ValueState result) {
  // Allow overwriting existing values. This is necessary for tests that need
  // to set the check result for the same preference multiple times
  // (e.g., once for the synchronous pass and once for the asynchronous pass).
  check_results_[path] = result;
}

void MockPrefHashStore::SetInvalidKeysResult(
    const std::string& path,
    const std::vector<std::string>& invalid_keys_result) {
  std::map<std::string, ValueState>::const_iterator result =
      check_results_.find(path);
  ASSERT_TRUE(result != check_results_.end());
  ValueState value_state = result->second;
  ASSERT_TRUE(value_state == ValueState::CHANGED ||
              value_state == ValueState::CHANGED_ENCRYPTED ||
              value_state == ValueState::CHANGED_VIA_HMAC_FALLBACK);

  invalid_keys_results_.insert(std::make_pair(path, invalid_keys_result));
}

std::unique_ptr<PrefHashStoreTransaction> MockPrefHashStore::BeginTransaction(
    HashStoreContents* storage,
    const os_crypt_async::Encryptor* encryptor_ptr) {
  EXPECT_FALSE(transaction_active_);
  transaction_active_ = true;
  // Pass this mock store instance to the nested transaction mock
  return std::unique_ptr<PrefHashStoreTransaction>(
      new MockPrefHashStoreTransaction(this));
}

std::string MockPrefHashStore::ComputeMac(const std::string& path,
                                          const base::Value* new_value) {
  return "atomic mac for: " + path;
}

base::Value::Dict MockPrefHashStore::ComputeSplitMacs(
    const std::string& path,
    const base::Value::Dict* split_values) {
  base::Value::Dict macs_dict;
  if (!split_values)
    return macs_dict;
  for (const auto item : *split_values) {
    macs_dict.Set(item.first,
                  base::Value("split mac for: " + path + "/" + item.first));
  }
  return macs_dict;
}

std::string MockPrefHashStore::ComputeEncryptedHash(
    const std::string& path,
    const base::Value* value,
    const os_crypt_async::Encryptor* encryptor_ptr) {
  // Return a dummy value, actual calculation not needed for filter test
  return "encrypted atomic hash for: " + path;
}

std::string MockPrefHashStore::ComputeEncryptedHash(
    const std::string& path,
    const base::Value::Dict* dict,
    const os_crypt_async::Encryptor* encryptor_ptr) {
  return "encrypted atomic hash for dict: " + path;
}

base::Value::Dict MockPrefHashStore::ComputeSplitEncryptedHashes(
    const std::string& path,
    const base::Value::Dict* split_values,
    const os_crypt_async::Encryptor* encryptor_ptr) {
  base::Value::Dict hashes_dict;
  if (!split_values) {
    return hashes_dict;
  }
  for (const auto item : *split_values) {
    hashes_dict.Set(
        item.first,
        base::Value("encrypted split hash for: " + path + "/" + item.first));
  }
  return hashes_dict;
}

ValueState MockPrefHashStore::RecordCheckValue(const std::string& path,
                                               const void* value,
                                               PrefTrackingStrategy strategy) {
  // Record that |path| was checked. Allow it to be checked multiple times.
  // This is required for tests that simulate both a synchronous and an
  // asynchronous validation pass.
  checked_values_[path] = std::make_pair(value, strategy);
  auto result = check_results_.find(path);
  if (result != check_results_.end())
    return result->second;
  return ValueState::UNCHANGED;
}

void MockPrefHashStore::RecordStoreHash(const std::string& path,
                                        const void* new_value_ptr,
                                        PrefTrackingStrategy strategy) {
  const base::Value* value_to_store =
      static_cast<const base::Value*>(new_value_ptr);
  std::unique_ptr<base::Value> owned_copy;

  if (value_to_store) {
    owned_copy = std::make_unique<base::Value>(
        value_to_store->Clone());  // Store a CLONE
  }

  // Store the unique_ptr (which might be null if new_value_ptr was null)
  // and the strategy. For simplicity, assuming overwrite is fine or paths are
  // unique per transaction.
  owned_stored_values_[path] = std::make_pair(std::move(owned_copy), strategy);
}

void MockPrefHashStore::ClearStoreHash(const std::string& path) {
  owned_stored_values_.erase(path);
}

std::string_view
MockPrefHashStore::MockPrefHashStoreTransaction ::GetStoreUMASuffix() const {
  return "unused";
}

ValueState MockPrefHashStore::MockPrefHashStoreTransaction::CheckValue(
    const std::string& path,
    const base::Value* value) const {
  return outer_->RecordCheckValue(path, value, PrefTrackingStrategy::ATOMIC);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreHash(
    const std::string& path,
    const base::Value* new_value) {
  outer_->RecordStoreHash(path, new_value, PrefTrackingStrategy::ATOMIC);
}

ValueState MockPrefHashStore::MockPrefHashStoreTransaction::CheckSplitValue(
    const std::string& path,
    const base::Value::Dict* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  EXPECT_TRUE(invalid_keys && invalid_keys->empty());

  std::map<std::string, std::vector<std::string>>::const_iterator
      invalid_keys_result = outer_->invalid_keys_results_.find(path);
  if (invalid_keys_result != outer_->invalid_keys_results_.end()) {
    invalid_keys->insert(invalid_keys->begin(),
                         invalid_keys_result->second.begin(),
                         invalid_keys_result->second.end());
  }

  return outer_->RecordCheckValue(path, initial_split_value,
                                  PrefTrackingStrategy::SPLIT);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreSplitHash(
    const std::string& path,
    const base::Value::Dict* new_value) {
  outer_->RecordStoreHash(path, new_value, PrefTrackingStrategy::SPLIT);
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::HasHash(
    const std::string& path) const {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

void MockPrefHashStore::MockPrefHashStoreTransaction::ImportHash(
    const std::string& path,
    const base::Value* hash) {
  ADD_FAILURE() << "Unexpected call.";
}

void MockPrefHashStore::MockPrefHashStoreTransaction::ClearHash(
    const std::string& path) {
  // Allow this to be called by PrefHashFilter's deprecated tracked prefs
  // cleanup tasks.
  outer_->ClearStoreHash(path);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::ClearEncryptedHash(
    const std::string& encrypted_path) {
  // Allow this to be called by PrefHashFilter's deprecated tracked prefs
  // cleanup tasks.
  outer_->ClearStoreHash(encrypted_path);
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::IsSuperMACValid() const {
  return outer_->is_super_mac_valid_result_;
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::StampSuperMac() {
  return outer_->stamp_super_mac_result_;
}

void MockPrefHashStore::SetTransactionCompletionCallback(
    base::OnceClosure callback) {
  completion_callback_ = std::move(callback);
}

std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> GetConfiguration(
    EnforcementLevel max_enforcement_level) {
  auto configuration = prefs::ConstructTrackedConfiguration(kTestTrackedPrefs);
  for (const auto& metadata : configuration) {
    if (metadata->enforcement_level > max_enforcement_level)
      metadata->enforcement_level = max_enforcement_level;
  }
  return configuration;
}

class MockHashStoreContents : public HashStoreContents {
 public:
  MockHashStoreContents() {}

  MockHashStoreContents(const MockHashStoreContents&) = delete;
  MockHashStoreContents& operator=(const MockHashStoreContents&) = delete;

  // Returns the number of hashes stored.
  size_t stored_hashes_count() const { return dictionary_.size(); }

  // Returns the number of paths cleared.
  size_t cleared_paths_count() const { return removed_entries_.size(); }

  // Returns the stored MAC for an Atomic preference.
  std::string GetStoredMac(const std::string& path) const;
  // Returns the stored MAC for a Split preference.
  std::string GetStoredSplitMac(const std::string& path,
                                const std::string& split_path) const;

  // HashStoreContents implementation.
  bool IsCopyable() const override;
  std::unique_ptr<HashStoreContents> MakeCopy() const override;
  std::string_view GetUMASuffix() const override;
  void Reset() override;
  bool GetMac(const std::string& path, std::string* out_value) override;
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) override;
  void SetMac(const std::string& path, const std::string& value) override;
  void SetSplitMac(const std::string& path,
                   const std::string& split_path,
                   const std::string& value) override;
  void ImportEntry(const std::string& path,
                   const base::Value* in_value) override;
  bool RemoveEntry(const std::string& path) override;
  const base::Value::Dict* GetContents() const override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  explicit MockHashStoreContents(MockHashStoreContents* origin_mock);

  // Records calls to this mock's SetMac/SetSplitMac methods.
  void RecordSetMac(const std::string& path, const std::string& mac) {
    dictionary_.Set(path, mac);
  }
  void RecordSetSplitMac(const std::string& path,
                         const std::string& split_path,
                         const std::string& mac) {
    dictionary_.SetByDottedPath(base::StrCat({path, ".", split_path}), mac);
  }

  // Records a call to this mock's RemoveEntry method.
  void RecordRemoveEntry(const std::string& path) {
    // Don't expect the same pref to be cleared more than once.
    EXPECT_EQ(removed_entries_.end(), removed_entries_.find(path));
    removed_entries_.insert(path);
  }

  base::Value::Dict dictionary_;
  std::set<std::string> removed_entries_;

  // The code being tested copies its HashStoreContents for use in a callback
  // which can be executed during shutdown. To be able to capture the behavior
  // of the copy, we make it forward calls to the mock it was created from.
  // Once set, |origin_mock_| must outlive this instance.
  raw_ptr<MockHashStoreContents> origin_mock_;
};

std::string MockHashStoreContents::GetStoredMac(const std::string& path) const {
  const base::Value* out_value = dictionary_.Find(path);
  if (out_value) {
    EXPECT_TRUE(out_value->is_string());

    return out_value->GetString();
  }

  return std::string();
}

std::string MockHashStoreContents::GetStoredSplitMac(
    const std::string& path,
    const std::string& split_path) const {
  const base::Value* out_value = dictionary_.Find(path);
  if (out_value) {
    EXPECT_TRUE(out_value->is_dict());

    out_value = out_value->GetDict().Find(split_path);
    if (out_value) {
      EXPECT_TRUE(out_value->is_string());

      return out_value->GetString();
    }
  }

  return std::string();
}

MockHashStoreContents::MockHashStoreContents(MockHashStoreContents* origin_mock)
    : origin_mock_(origin_mock) {}

bool MockHashStoreContents::IsCopyable() const {
  return true;
}

std::unique_ptr<HashStoreContents> MockHashStoreContents::MakeCopy() const {
  // Return a new MockHashStoreContents which forwards all requests to this
  // mock instance.
  return std::unique_ptr<HashStoreContents>(
      new MockHashStoreContents(const_cast<MockHashStoreContents*>(this)));
}

std::string_view MockHashStoreContents::GetUMASuffix() const {
  return "Unused";
}

void MockHashStoreContents::Reset() {
  ADD_FAILURE() << "Unexpected call.";
}

bool MockHashStoreContents::GetMac(const std::string& path,
                                   std::string* out_value) {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

bool MockHashStoreContents::GetSplitMacs(
    const std::string& path,
    std::map<std::string, std::string>* split_macs) {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

void MockHashStoreContents::SetMac(const std::string& path,
                                   const std::string& value) {
  if (origin_mock_)
    origin_mock_->RecordSetMac(path, value);
  else
    RecordSetMac(path, value);
}

void MockHashStoreContents::SetSplitMac(const std::string& path,
                                        const std::string& split_path,
                                        const std::string& value) {
  if (origin_mock_)
    origin_mock_->RecordSetSplitMac(path, split_path, value);
  else
    RecordSetSplitMac(path, split_path, value);
}

void MockHashStoreContents::ImportEntry(const std::string& path,
                                        const base::Value* in_value) {
  ADD_FAILURE() << "Unexpected call.";
}

bool MockHashStoreContents::RemoveEntry(const std::string& path) {
  if (origin_mock_)
    origin_mock_->RecordRemoveEntry(path);
  else
    RecordRemoveEntry(path);
  return true;
}

const base::Value::Dict* MockHashStoreContents::GetContents() const {
  ADD_FAILURE() << "Unexpected call.";
  return nullptr;
}

std::string MockHashStoreContents::GetSuperMac() const {
  ADD_FAILURE() << "Unexpected call.";
  return std::string();
}

void MockHashStoreContents::SetSuperMac(const std::string& super_mac) {
  ADD_FAILURE() << "Unexpected call.";
}

class PrefHashFilterTest : public testing::TestWithParam<EnforcementLevel>,
                           public prefs::mojom::ResetOnLoadObserver {
 public:
  PrefHashFilterTest()
      : mock_pref_hash_store_(nullptr),
        mock_validation_delegate_record_(new MockValidationDelegateRecord),
        mock_validation_delegate_(mock_validation_delegate_record_),
        validation_delegate_receiver_(&mock_validation_delegate_),
        reset_recorded_(false) {
    // Register prefs that are accessed by tasks posted from
    // FinalizeFilterOnLoad (like UpdateTrackedPreferencesResetListInPrefStore
    // and DeferredEncryptorRevalidation).
    pref_service_.registry()->RegisterStringPref(
        user_prefs::kPreferenceResetTime, "0");
    pref_service_.registry()->RegisterListPref(
        user_prefs::kTrackedPreferencesReset);
    pref_service_.registry()->RegisterStringPref(kScheduleToFlushToDisk, "0");
  }

  PrefHashFilterTest(const PrefHashFilterTest&) = delete;
  PrefHashFilterTest& operator=(const PrefHashFilterTest&) = delete;

  void SetUp() override {
    // By default, initialize with a synchronous OSCrypt instance for existing
    // tests.
    InitializeSyncOSCrypt();
    Reset();
    pref_hash_filter_->SetPrefService(&pref_service_);
  }

  // Resets to the default state (feature off).
  void Reset() {
    ResetImpl(false /* enable_encrypted_hashing_feature */,
              test_os_crypt_async_.get());
  }

  // The main reset implementation, allowing tests to control the feature flag
  // and the OSCrypt instance.
  void ResetImpl(bool enable_encrypted_hashing_feature,
                 os_crypt_async::OSCryptAsync* os_crypt_instance_to_use) {
    feature_list_.Reset();  // Clear previous ScopedFeatureList state
    if (enable_encrypted_hashing_feature) {
      feature_list_.InitAndEnableFeature(tracked::kEncryptedPrefHashing);
    } else {
      feature_list_.InitAndDisableFeature(tracked::kEncryptedPrefHashing);
    }
    InitializePrefHashFilter(GetConfiguration(GetParam()),
                             os_crypt_instance_to_use);
  }

  // Stores |prefs| back in |pref_store_contents| and ensure
  // |expected_schedule_write| matches the reported |schedule_write|.
  void GetPrefsBack(bool expected_schedule_write,
                    base::Value::Dict prefs,
                    bool schedule_write) {
    pref_store_contents_ = std::move(prefs);
    EXPECT_EQ(expected_schedule_write, schedule_write);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> test_os_crypt_async_;

 protected:
  // Initializes a TestOSCryptAsync instance that will provide its encryptor
  // asynchronously.
  void InitializeAsyncOSCrypt() {
    test_os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        false /* is_sync_default */);
  }

  // Initializes a TestOSCryptAsync instance that provides its encryptor
  // synchronously upon request.
  void InitializeSyncOSCrypt() {
    test_os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        true /* is_sync_default */);
  }

  // Initializes |pref_hash_filter_| with a PrefHashFilter that uses a
  // MockPrefHashStore. The raw pointer to the MockPrefHashStore (owned by the
  // PrefHashFilter) is stored in |mock_pref_hash_store_|.
  void InitializePrefHashFilter(
      std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> configuration,
      os_crypt_async::OSCryptAsync* os_crypt) {
    std::unique_ptr<MockPrefHashStore> temp_mock_pref_hash_store(
        new MockPrefHashStore);
    std::unique_ptr<MockPrefHashStore>
        temp_mock_external_validation_pref_hash_store(new MockPrefHashStore);
    std::unique_ptr<MockHashStoreContents>
        temp_mock_external_validation_hash_store_contents(
            new MockHashStoreContents);
    mock_pref_hash_store_ = temp_mock_pref_hash_store.get();
    mock_external_validation_pref_hash_store_ =
        temp_mock_external_validation_pref_hash_store.get();
    mock_external_validation_hash_store_contents_ =
        temp_mock_external_validation_hash_store_contents.get();

    validation_delegate_receiver_.reset();
    reset_on_load_observer_receivers_.Clear();

    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer;
    reset_on_load_observer_receivers_.Add(
        this, reset_on_load_observer.InitWithNewPipeAndPassReceiver());
    mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate_remote(
            validation_delegate_receiver_.BindNewPipeAndPassRemote());
    auto validation_delegate_remote_ref =
        base::MakeRefCounted<base::RefCountedData<
            mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>(
            std::move(validation_delegate_remote));
    pref_hash_filter_ = std::make_unique<PrefHashFilter>(
        std::move(temp_mock_pref_hash_store),
        PrefHashFilter::StoreContentsPair(
            std::move(temp_mock_external_validation_pref_hash_store),
            std::move(temp_mock_external_validation_hash_store_contents)),
        std::move(configuration), std::move(reset_on_load_observer),
        std::move(validation_delegate_remote_ref), std::size(kTestTrackedPrefs),
        os_crypt);
  }

  // Initializes |pref_hash_filter_| with a PrefHashFilter that uses a
  // MockPrefHashStore. The raw pointer to the MockPrefHashStore (owned by the
  // PrefHashFilter) is stored in |mock_pref_hash_store_|. The configuration is
  // built from |custom_metadata|. This function is a helper function similar to
  // the one above.
  void InitializePrefHashFilterWithCustomConfig(
      const std::vector<prefs::TrackedPreferenceMetadata>& custom_metadata,
      os_crypt_async::OSCryptAsync* os_crypt) {
    auto configuration = prefs::ConstructTrackedConfiguration(custom_metadata);

    auto temp_mock_pref_hash_store = std::make_unique<MockPrefHashStore>();
    auto temp_mock_external_validation_pref_hash_store =
        std::make_unique<MockPrefHashStore>();
    auto temp_mock_external_validation_hash_store_contents =
        std::make_unique<MockHashStoreContents>();
    mock_pref_hash_store_ = temp_mock_pref_hash_store.get();
    mock_external_validation_pref_hash_store_ =
        temp_mock_external_validation_pref_hash_store.get();
    mock_external_validation_hash_store_contents_ =
        temp_mock_external_validation_hash_store_contents.get();

    validation_delegate_receiver_.reset();
    reset_on_load_observer_receivers_.Clear();

    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer;
    reset_on_load_observer_receivers_.Add(
        this, reset_on_load_observer.InitWithNewPipeAndPassReceiver());

    mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate_remote(
            validation_delegate_receiver_.BindNewPipeAndPassRemote());
    auto validation_delegate_remote_ref =
        base::MakeRefCounted<base::RefCountedData<
            mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>(
            std::move(validation_delegate_remote));

    // Initialize the filter with the CUSTOM configuration and size.
    pref_hash_filter_ = std::make_unique<PrefHashFilter>(
        std::move(temp_mock_pref_hash_store),
        PrefHashFilter::StoreContentsPair(
            std::move(temp_mock_external_validation_pref_hash_store),
            std::move(temp_mock_external_validation_hash_store_contents)),
        std::move(configuration), std::move(reset_on_load_observer),
        std::move(validation_delegate_remote_ref), custom_metadata.size(),
        os_crypt);
  }

  // Verifies whether a reset was reported by the PrefHashFiler. Also verifies
  // that kPreferenceResetTime was set (or not) accordingly.
  void VerifyRecordedReset(bool reset_expected) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(reset_expected, reset_recorded_);
    EXPECT_EQ(reset_expected, !!pref_store_contents_.FindByDottedPath(
                                  user_prefs::kPreferenceResetTime));
  }

  // Calls FilterOnLoad() on |pref_hash_Filter_|. |pref_store_contents_| is
  // handed off, but should be given back to us synchronously through
  // GetPrefsBack() as there is no FilterOnLoadInterceptor installed on
  // |pref_hash_filter_|.
  void DoFilterOnLoad(bool expected_final_prefs_altered) {
    pref_hash_filter_->FilterOnLoad(
        base::BindOnce(&PrefHashFilterTest::GetPrefsBack,
                       base::Unretained(this), expected_final_prefs_altered),
        std::move(pref_store_contents_));
    task_environment_.RunUntilIdle();
  }

  raw_ptr<MockPrefHashStore, DanglingUntriaged> mock_pref_hash_store_;
  raw_ptr<MockPrefHashStore, DanglingUntriaged>
      mock_external_validation_pref_hash_store_;
  raw_ptr<MockHashStoreContents, DanglingUntriaged>
      mock_external_validation_hash_store_contents_;
  base::Value::Dict pref_store_contents_;
  scoped_refptr<MockValidationDelegateRecord> mock_validation_delegate_record_;
  std::unique_ptr<PrefHashFilter> pref_hash_filter_;
  TestingPrefServiceSimple pref_service_;

 private:
  void OnResetOnLoad() override {
    // As-is |reset_recorded_| is only designed to remember a single reset, make
    // sure none was previously recorded.
    EXPECT_FALSE(reset_recorded_);
    reset_recorded_ = true;
  }

  base::test::ScopedFeatureList feature_list_;
  MockValidationDelegate mock_validation_delegate_;
  mojo::Receiver<prefs::mojom::TrackedPreferenceValidationDelegate>
      validation_delegate_receiver_;
  mojo::ReceiverSet<prefs::mojom::ResetOnLoadObserver>
      reset_on_load_observer_receivers_;
  bool reset_recorded_;
};

TEST_P(PrefHashFilterTest, EmptyAndUnchanged) {
  DoFilterOnLoad(false);
  // All paths checked.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  // No paths stored, since they all return |UNCHANGED|.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  // Since there was nothing in |pref_store_contents_| the checked value should
  // have been nullptr for all tracked preferences.
  for (const auto& pref : kTestTrackedPrefs) {
    ASSERT_FALSE(mock_pref_hash_store_->checked_value(pref.name).first);
  }
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);

  // Delegate saw all paths, and all unchanged.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));
}

TEST_P(PrefHashFilterTest, StampSuperMACAltersStore) {
  mock_pref_hash_store_->set_stamp_super_mac_result(true);
  DoFilterOnLoad(true);
  // No paths stored, since they all return |UNCHANGED|. The StampSuperMAC
  // result is the only reason the prefs were considered altered.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefUpdate) {
  base::Value::Dict root_dict;
  std::string expected_string_content = "string value";
  root_dict.Set(kAtomicPref, expected_string_content);

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  base::RunLoop run_loop;
  mock_pref_hash_store_->SetTransactionCompletionCallback(
      run_loop.QuitClosure());
  pref_hash_filter_->FilterSerializeData(root_dict);
  run_loop.Run();  // Ensure any posted tasks are run
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  // Compare value content, not pointer
  ASSERT_TRUE(stored_value.first);
  const base::Value* actual_stored_value =
      static_cast<const base::Value*>(stored_value.first);
  ASSERT_TRUE(actual_stored_value->is_string());
  EXPECT_EQ(expected_string_content, actual_stored_value->GetString());
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefClearing) {
  base::Value::Dict root_dict;
  // We don't actually add the pref's value to root_dict to simulate that
  // it was just cleared in the PrefStore.

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData, with no value.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterSplitPrefUpdate) {
  base::Value::Dict root_dict;
  // Store expected content
  base::Value::Dict expected_dict_content;
  expected_dict_content.Set("a", "foo");
  expected_dict_content.Set("b", 1234);
  root_dict.Set(kSplitPref, expected_dict_content.Clone());
  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  base::RunLoop run_loop;
  mock_pref_hash_store_->SetTransactionCompletionCallback(
      run_loop.QuitClosure());
  pref_hash_filter_->FilterSerializeData(root_dict);
  run_loop.Run();
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value_info =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_TRUE(stored_value_info.first);
  const base::Value* actual_stored_value =
      static_cast<const base::Value*>(stored_value_info.first);
  ASSERT_TRUE(actual_stored_value->is_dict());
  // Compare dictionary contents directly.
  EXPECT_EQ(expected_dict_content, actual_stored_value->GetDict());
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value_info.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterTrackedSplitPrefClearing) {
  base::Value::Dict root_dict;
  // We don't actually add the pref's value to root_dict to simulate that
  // it was just cleared in the PrefStore.

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData, with no value.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterUntrackedPrefUpdate) {
  base::Value::Dict root_dict;
  root_dict.Set("untracked", "some value");
  pref_hash_filter_->FilterUpdate("untracked");

  // No paths should be stored on FilterUpdate.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Nor on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // No transaction should even be started on FilterSerializeData() if there are
  // no updates to perform.
  ASSERT_EQ(0u, mock_pref_hash_store_->transactions_performed());
}

TEST_P(PrefHashFilterTest, MultiplePrefsFilterSerializeData) {
  base::Value::Dict root_dict;
  int expected_atomic_val = 1;
  int expected_atomic3_val = 5;
  base::Value::Dict expected_split_dict_content;
  expected_split_dict_content.Set("a", true);

  root_dict.Set(kAtomicPref, expected_atomic_val);
  root_dict.Set(kAtomicPref2, 2);
  root_dict.Set(kAtomicPref3, 3);
  root_dict.Set("untracked", 4);
  root_dict.Set(kSplitPref, expected_split_dict_content.Clone());
  // Only update kAtomicPref, kAtomicPref3, and kSplitPref.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  pref_hash_filter_->FilterUpdate(kAtomicPref3);
  pref_hash_filter_->FilterUpdate(kSplitPref);
  root_dict.Set(kAtomicPref3, expected_atomic3_val);

  base::RunLoop run_loop;
  mock_pref_hash_store_->SetTransactionCompletionCallback(
      run_loop.QuitClosure());
  pref_hash_filter_->FilterSerializeData(root_dict);
  run_loop.Run();

  ASSERT_EQ(3u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Verify kAtomicPref
  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic1_info =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_TRUE(stored_value_atomic1_info.first);
  const base::Value* actual_atomic1 =
      static_cast<const base::Value*>(stored_value_atomic1_info.first);
  ASSERT_TRUE(actual_atomic1->is_int());
  EXPECT_EQ(expected_atomic_val, actual_atomic1->GetInt());
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value_atomic1_info.second);

  // Verify kAtomicPref3
  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic3_info =
      mock_pref_hash_store_->stored_value(kAtomicPref3);
  ASSERT_TRUE(stored_value_atomic3_info.first);
  const base::Value* actual_atomic3 =
      static_cast<const base::Value*>(stored_value_atomic3_info.first);
  ASSERT_TRUE(actual_atomic3->is_int());
  EXPECT_EQ(expected_atomic3_val, actual_atomic3->GetInt());
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value_atomic3_info.second);

  // Verify kSplitPref
  MockPrefHashStore::ValuePtrStrategyPair stored_value_split_info =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_TRUE(stored_value_split_info.first);
  const base::Value* actual_split =
      static_cast<const base::Value*>(stored_value_split_info.first);
  ASSERT_TRUE(actual_split->is_dict());
  EXPECT_EQ(expected_split_dict_content, actual_split->GetDict());
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value_split_info.second);
}

TEST_P(PrefHashFilterTest, UnknownNullValue) {
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  // nullptr values are always trusted by the PrefHashStore.
  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::TRUSTED_NULL_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::TRUSTED_NULL_VALUE);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_atomic_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_split_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::TRUSTED_NULL_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  const MockValidationDelegateRecord::ValidationEvent* validated_split_pref =
      mock_validation_delegate_record_->GetEventForPath(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, validated_split_pref->strategy);
  ASSERT_FALSE(validated_split_pref->is_personal);
  const MockValidationDelegateRecord::ValidationEvent* validated_atomic_pref =
      mock_validation_delegate_record_->GetEventForPath(kAtomicPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, validated_atomic_pref->strategy);
  ASSERT_TRUE(validated_atomic_pref->is_personal);
}

TEST_P(PrefHashFilterTest, InitialValueUnknown) {
  std::string expected_atomic_string_content = "string value";
  base::Value::Dict expected_split_dict_content;
  expected_split_dict_content.Set("a", "foo");
  expected_split_dict_content.Set("b", 1234);

  pref_store_contents_.Set(kAtomicPref, expected_atomic_string_content);
  pref_store_contents_.Set(kSplitPref, expected_split_dict_content.Clone());

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::UNTRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::UNTRUSTED_UNKNOWN_VALUE);
  // If we are enforcing, expect this to report changes.
  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::UNTRUSTED_UNKNOWN_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value_info =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value_info =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value_info.second);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value_info.second);
  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(stored_atomic_value_info.first);
    ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
    ASSERT_FALSE(stored_split_value_info.first);
    VerifyRecordedReset(true);
  } else {  // NO_ENFORCEMENT
    const base::Value* atomic_value_in_store =
        pref_store_contents_.Find(kAtomicPref);
    ASSERT_TRUE(atomic_value_in_store);
    ASSERT_TRUE(atomic_value_in_store->is_string());
    EXPECT_EQ(expected_atomic_string_content,
              atomic_value_in_store->GetString());

    ASSERT_TRUE(stored_atomic_value_info.first);
    const base::Value* actual_stored_atomic =
        static_cast<const base::Value*>(stored_atomic_value_info.first);
    ASSERT_TRUE(actual_stored_atomic->is_string());
    EXPECT_EQ(expected_atomic_string_content,
              actual_stored_atomic->GetString());

    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_TRUE(split_value_in_store->is_dict());
    EXPECT_EQ(expected_split_dict_content, split_value_in_store->GetDict());

    ASSERT_TRUE(stored_split_value_info.first);
    const base::Value* actual_stored_split =
        static_cast<const base::Value*>(stored_split_value_info.first);
    ASSERT_TRUE(actual_stored_split->is_dict());
    EXPECT_EQ(expected_split_dict_content, actual_stored_split->GetDict());

    VerifyRecordedReset(false);
  }
}

TEST_P(PrefHashFilterTest, InitialValueTrustedUnknown) {
  std::string expected_atomic_string_content = "test";
  base::Value::Dict expected_split_dict_content;
  expected_split_dict_content.Set("a", "foo");
  expected_split_dict_content.Set("b", 1234);

  pref_store_contents_.Set(kAtomicPref, expected_atomic_string_content);
  pref_store_contents_.Set(kSplitPref, expected_split_dict_content.Clone());

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::TRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::TRUSTED_UNKNOWN_VALUE);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::TRUSTED_UNKNOWN_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // Seeding is always allowed for trusted unknown values.
  const base::Value* atomic_value_in_store =
      pref_store_contents_.Find(kAtomicPref);
  ASSERT_TRUE(atomic_value_in_store);
  ASSERT_TRUE(atomic_value_in_store->is_string());
  EXPECT_EQ(expected_atomic_string_content, atomic_value_in_store->GetString());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value_info =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_TRUE(stored_atomic_value_info.first);
  const base::Value* actual_stored_atomic =
      static_cast<const base::Value*>(stored_atomic_value_info.first);
  ASSERT_TRUE(actual_stored_atomic->is_string());
  EXPECT_EQ(expected_atomic_string_content, actual_stored_atomic->GetString());
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value_info.second);

  const base::Value* split_value_in_store =
      pref_store_contents_.Find(kSplitPref);
  ASSERT_TRUE(split_value_in_store);
  ASSERT_TRUE(split_value_in_store->is_dict());
  EXPECT_EQ(expected_split_dict_content, split_value_in_store->GetDict());

  MockPrefHashStore::ValuePtrStrategyPair stored_split_value_info =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_TRUE(stored_split_value_info.first);
  const base::Value* actual_stored_split =
      static_cast<const base::Value*>(stored_split_value_info.first);
  ASSERT_TRUE(actual_stored_split->is_dict());
  EXPECT_EQ(expected_split_dict_content, actual_stored_split->GetDict());
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value_info.second);
}

TEST_P(PrefHashFilterTest, InitialValueChanged) {
  int expected_atomic_int_content = 1234;
  base::Value::Dict initial_split_dict_content;
  initial_split_dict_content.Set("a", "foo");
  initial_split_dict_content.Set("b", 1234);
  initial_split_dict_content.Set("c", 56);
  initial_split_dict_content.Set("d", false);

  base::Value::Dict expected_final_split_dict_content;
  expected_final_split_dict_content.Set("b", 1234);
  expected_final_split_dict_content.Set("d", false);

  pref_store_contents_.Set(kAtomicPref, expected_atomic_int_content);
  pref_store_contents_.Set(kSplitPref, initial_split_dict_content.Clone());

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);

  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, mock_invalid_keys);

  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value_info =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value_info =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value_info.second);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value_info.second);
  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    // Ensure the atomic pref was cleared and the hash for nullptr was restored
    // if the current enforcement level prevents changes.
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(stored_atomic_value_info.first);

    // The split pref on the other hand should only have been stripped of its
    // invalid keys.
    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_TRUE(split_value_in_store->is_dict());
    EXPECT_EQ(expected_final_split_dict_content,
              split_value_in_store->GetDict());

    ASSERT_TRUE(stored_split_value_info.first);
    const base::Value* actual_stored_split =
        static_cast<const base::Value*>(stored_split_value_info.first);
    ASSERT_TRUE(actual_stored_split->is_dict());
    EXPECT_EQ(expected_final_split_dict_content,
              actual_stored_split->GetDict());

    VerifyRecordedReset(true);
  } else {  // NO_ENFORCEMENT
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* atomic_value_in_store =
        pref_store_contents_.Find(kAtomicPref);
    ASSERT_TRUE(atomic_value_in_store);
    ASSERT_TRUE(atomic_value_in_store->is_int());
    EXPECT_EQ(expected_atomic_int_content, atomic_value_in_store->GetInt());

    ASSERT_TRUE(stored_atomic_value_info.first);
    const base::Value* actual_stored_atomic =
        static_cast<const base::Value*>(stored_atomic_value_info.first);
    ASSERT_TRUE(actual_stored_atomic->is_int());
    EXPECT_EQ(expected_atomic_int_content, actual_stored_atomic->GetInt());

    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_TRUE(split_value_in_store->is_dict());
    EXPECT_EQ(initial_split_dict_content, split_value_in_store->GetDict());

    ASSERT_TRUE(stored_split_value_info.first);
    const base::Value* actual_stored_split =
        static_cast<const base::Value*>(stored_split_value_info.first);
    ASSERT_TRUE(actual_stored_split->is_dict());
    EXPECT_EQ(initial_split_dict_content, actual_stored_split->GetDict());

    VerifyRecordedReset(false);
  }
}

TEST_P(PrefHashFilterTest, EmptyCleared) {
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CLEARED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CLEARED);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::CLEARED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // Regardless of the enforcement level, the only thing that should be done is
  // to restore the hash for nullptr. The value itself should still be nullptr.
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_atomic_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);

  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_split_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
}

TEST_P(PrefHashFilterTest, DontResetReportOnly) {
  int expected_atomic_val = 1;
  int expected_atomic2_val = 2;
  int expected_report_only_val = 3;
  base::Value::Dict expected_report_only_split_val_content;
  expected_report_only_split_val_content.Set("a", 1234);

  pref_store_contents_.Set(kAtomicPref, expected_atomic_val);
  pref_store_contents_.Set(kAtomicPref2, expected_atomic2_val);
  pref_store_contents_.Set(kReportOnlyPref, expected_report_only_val);
  pref_store_contents_.Set(kReportOnlySplitPref,
                           expected_report_only_split_val_content.Clone());

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kAtomicPref2, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlyPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlySplitPref,
                                        ValueState::CHANGED);

  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  // All prefs should be checked and a new hash should be stored for each tested
  // pref.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(4u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, four of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(4u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::CHANGED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 4u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // No matter what the enforcement level is, the report only pref should never
  // be reset.
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlyPref));
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlySplitPref));

  // Compare content for report-only prefs
  MockPrefHashStore::ValuePtrStrategyPair stored_report_only_info =
      mock_pref_hash_store_->stored_value(kReportOnlyPref);
  ASSERT_TRUE(stored_report_only_info.first);
  const base::Value* actual_stored_report_only =
      static_cast<const base::Value*>(stored_report_only_info.first);
  ASSERT_TRUE(actual_stored_report_only->is_int());
  EXPECT_EQ(expected_report_only_val, actual_stored_report_only->GetInt());

  MockPrefHashStore::ValuePtrStrategyPair stored_report_only_split_info =
      mock_pref_hash_store_->stored_value(kReportOnlySplitPref);
  ASSERT_TRUE(stored_report_only_split_info.first);
  const base::Value* actual_stored_report_only_split =
      static_cast<const base::Value*>(stored_report_only_split_info.first);
  ASSERT_TRUE(actual_stored_report_only_split->is_dict());
  EXPECT_EQ(expected_report_only_split_val_content,
            actual_stored_report_only_split->GetDict());

  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref2));
    ASSERT_FALSE(mock_pref_hash_store_->stored_value(kAtomicPref).first);
    ASSERT_FALSE(mock_pref_hash_store_->stored_value(kAtomicPref2).first);

    VerifyRecordedReset(true);
  } else {  // NO_ENFORCEMENT
    // Compare content for enforced prefs if not reset
    const base::Value* atomic_val_in_store =
        pref_store_contents_.Find(kAtomicPref);
    ASSERT_TRUE(atomic_val_in_store && atomic_val_in_store->is_int());
    EXPECT_EQ(expected_atomic_val, atomic_val_in_store->GetInt());
    MockPrefHashStore::ValuePtrStrategyPair stored_atomic_info =
        mock_pref_hash_store_->stored_value(kAtomicPref);
    ASSERT_TRUE(stored_atomic_info.first);
    const base::Value* actual_stored_atomic =
        static_cast<const base::Value*>(stored_atomic_info.first);
    ASSERT_TRUE(actual_stored_atomic->is_int());
    EXPECT_EQ(expected_atomic_val, actual_stored_atomic->GetInt());

    const base::Value* atomic2_val_in_store =
        pref_store_contents_.Find(kAtomicPref2);
    ASSERT_TRUE(atomic2_val_in_store && atomic2_val_in_store->is_int());
    EXPECT_EQ(expected_atomic2_val, atomic2_val_in_store->GetInt());
    MockPrefHashStore::ValuePtrStrategyPair stored_atomic2_info =
        mock_pref_hash_store_->stored_value(kAtomicPref2);
    ASSERT_TRUE(stored_atomic2_info.first);
    const base::Value* actual_stored_atomic2 =
        static_cast<const base::Value*>(stored_atomic2_info.first);
    ASSERT_TRUE(actual_stored_atomic2->is_int());
    EXPECT_EQ(expected_atomic2_val, actual_stored_atomic2->GetInt());

    VerifyRecordedReset(false);
  }
}

TEST_P(PrefHashFilterTest, CallFilterSerializeDataCallbacks) {
  base::Value::Dict root_dict;
  base::Value::Dict dict_value;
  dict_value.Set("a", true);
  root_dict.Set(kAtomicPref, 1);
  root_dict.Set(kAtomicPref2, 2);
  root_dict.Set(kSplitPref, std::move(dict_value));

  // Skip updating kAtomicPref2.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  pref_hash_filter_->FilterUpdate(kSplitPref);

  PrefHashFilter::OnWriteCallbackPair callbacks =
      pref_hash_filter_->FilterSerializeData(root_dict);

  ASSERT_FALSE(callbacks.first.is_null());

  // Prefs should be cleared from the external validation store only once the
  // before-write callback is run.
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->cleared_paths_count());
  std::move(callbacks.first).Run();
  ASSERT_EQ(
      2u, mock_external_validation_hash_store_contents_->cleared_paths_count());

  // No pref write should occur before the after-write callback is run.
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->stored_hashes_count());

  std::move(callbacks.second).Run(true);

  ASSERT_EQ(
      2u, mock_external_validation_hash_store_contents_->stored_hashes_count());
  ASSERT_EQ(
      "atomic mac for: atomic_pref",
      mock_external_validation_hash_store_contents_->GetStoredMac(kAtomicPref));
  ASSERT_EQ("split mac for: split_pref/a",
            mock_external_validation_hash_store_contents_->GetStoredSplitMac(
                kSplitPref, "a"));

  // The callbacks should write directly to the contents without going through
  // a pref hash store.
  ASSERT_EQ(0u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, CallFilterSerializeDataCallbacksWithFailure) {
  base::Value::Dict root_dict;
  root_dict.Set(kAtomicPref, 1);

  // Only update kAtomicPref.
  pref_hash_filter_->FilterUpdate(kAtomicPref);

  PrefHashFilter::OnWriteCallbackPair callbacks =
      pref_hash_filter_->FilterSerializeData(root_dict);

  ASSERT_FALSE(callbacks.first.is_null());

  std::move(callbacks.first).Run();

  // The pref should have been cleared from the external validation store.
  ASSERT_EQ(
      1u, mock_external_validation_hash_store_contents_->cleared_paths_count());

  std::move(callbacks.second).Run(false);

  // Expect no writes to the external validation hash store contents.
  ASSERT_EQ(0u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->stored_hashes_count());
}

TEST_P(PrefHashFilterTest, ExternalValidationValueChanged) {
  pref_store_contents_.Set(kAtomicPref, 1234);

  base::Value::Dict dict_value;
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);
  dict_value.Set("c", 56);
  dict_value.Set("d", false);
  pref_store_contents_.Set(kSplitPref, std::move(dict_value));

  mock_external_validation_pref_hash_store_->SetCheckResult(
      kAtomicPref, ValueState::CHANGED);
  mock_external_validation_pref_hash_store_->SetCheckResult(
      kSplitPref, ValueState::CHANGED);

  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_external_validation_pref_hash_store_->SetInvalidKeysResult(
      kSplitPref, mock_invalid_keys);

  DoFilterOnLoad(false);

  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_external_validation_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(
      1u, mock_external_validation_pref_hash_store_->transactions_performed());

  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());

  // Regular validation should not have any CHANGED prefs.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // External validation should have two CHANGED prefs (kAtomic and kSplit).
  ASSERT_EQ(2u,
            mock_validation_delegate_record_->CountExternalValidationsOfState(
                ValueState::CHANGED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountExternalValidationsOfState(
                ValueState::UNCHANGED));
}

TEST_P(PrefHashFilterTest, TrackedPreferenceResetStored) {
  // This test is only relevant for platforms where ENFORCE_ON_LOAD is a real
  // enforcement level.
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  int expected_atomic_int_content = 1234;
  pref_store_contents_.Set(kAtomicPref, expected_atomic_int_content);
  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);
  DoFilterOnLoad(true);
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  const base::Value::List* reset_prefs =
      pref_store_contents_.FindList(user_prefs::kTrackedPreferencesReset);
  ASSERT_TRUE(reset_prefs);
  ASSERT_EQ(1u, reset_prefs->size());
  ASSERT_EQ(base::Value(kAtomicPref), (*reset_prefs)[0]);
}

TEST_P(PrefHashFilterTest, TrackedSplitPreferenceResetStored) {
  // This test is only relevant for platforms where ENFORCE_ON_LOAD is a real
  // enforcement level.
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  base::Value::Dict initial_split_dict_content;
  initial_split_dict_content.Set("a", "foo");
  initial_split_dict_content.Set("b", 1234);
  initial_split_dict_content.Set("c", 56);
  initial_split_dict_content.Set("d", false);

  pref_store_contents_.Set(kSplitPref, initial_split_dict_content.Clone());
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);
  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, mock_invalid_keys);

  DoFilterOnLoad(true);

  const base::Value::List* reset_prefs =
      pref_store_contents_.FindList(user_prefs::kTrackedPreferencesReset);
  ASSERT_TRUE(reset_prefs);
  ASSERT_EQ(2u, reset_prefs->size());
  EXPECT_TRUE(base::Contains(*reset_prefs,
                             base::Value(std::string(kSplitPref) + ".a")));
  EXPECT_TRUE(base::Contains(*reset_prefs,
                             base::Value(std::string(kSplitPref) + ".c")));
}

TEST_P(PrefHashFilterTest, CleanupDeprecatedTrackedDictionary) {
  // Fake a preference value and stored hash from an old version of Chrome.
  base::Value pref_value(1234);
  pref_store_contents_.SetByDottedPath(kDeprecatedTrackedDictionaryEntry, 1234);
  {
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        mock_pref_hash_store_->PrefHashStore::BeginTransaction(nullptr));
    transaction->StoreHash(kDeprecatedTrackedDictionaryEntry, &pref_value);
  }

  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  std::vector<const char*> test_deprecated_prefs{
      kDeprecatedTrackedDictionaryEntry,
      kDeprecatedUntrackedDictionary,
  };
  PrefHashFilter::SetDeprecatedPrefsForTesting(test_deprecated_prefs);

  DoFilterOnLoad(false);

  EXPECT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  EXPECT_FALSE(pref_store_contents_.contains("dictionary.pref"));
  EXPECT_FALSE(pref_store_contents_.contains("dictionary"));
}

TEST_P(PrefHashFilterTest, RecordTrackedPreferenceResetCount_NoResets) {
  base::HistogramTester histogram_tester;
  pref_hash_filter_->MaybeRecordTrackedPreferenceResetCount(
      pref_store_contents_);
  histogram_tester.ExpectUniqueSample("Settings.TrackedPreferenceResets.Count",
                                      0, 1);
}

TEST_P(PrefHashFilterTest, RecordTrackedPreferenceResetCount_WithResets) {
  base::Value::List reset_list;
  reset_list.Append("path.to.some.pref");
  reset_list.Append("path.to.another.pref");
  pref_store_contents_.Set(user_prefs::kTrackedPreferencesReset,
                           std::move(reset_list));

  base::HistogramTester histogram_tester;
  pref_hash_filter_->MaybeRecordTrackedPreferenceResetCount(
      pref_store_contents_);
  histogram_tester.ExpectUniqueSample("Settings.TrackedPreferenceResets.Count",
                                      2, 1);
}

TEST_P(PrefHashFilterTest, TrackedSplitPreferenceResetMissingDict) {
  // This test is only relevant for platforms where ENFORCE_ON_LOAD applies.
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, {"z", "a", "c", "k"});

  // This the code should run without crashing.
  DoFilterOnLoad(true);

  // The preference should still be missing, as it was reset from a non existent
  // state.
  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));

  // Since the original value was missing, nothing should be stored.
  const base::Value::Dict* reset_prefs =
      pref_store_contents_.FindDict(user_prefs::kTrackedPreferencesReset);
  ASSERT_FALSE(reset_prefs && reset_prefs->contains(kSplitPref));
}

INSTANTIATE_TEST_SUITE_P(PrefHashFilterTestInstance,
                         PrefHashFilterTest,
                         testing::Values(EnforcementLevel::NO_ENFORCEMENT,
                                         EnforcementLevel::ENFORCE_ON_LOAD));

// A test fixture for the PrefHashFilter that specifically handles testing the
// new encrypted hashing and deferred validation logic.
class PrefHashFilterEncryptedTest : public PrefHashFilterTest {
 public:
  // Helper function to handle the deferred revalidation callback.
  void OnDeferredRevalidationComplete(bool* callback_ran_flag,
                                      base::OnceClosure quit_closure) {
    *callback_ran_flag = true;
    std::move(quit_closure).Run();
  }

 protected:
  void SetUp() override {
    // This fixture manually controls its setup, so the base SetUp is not
    // needed.
  }

  void TearDown() override {
    // Ensure the dependency is always broken after each test.
    if (pref_hash_filter_) {
      pref_hash_filter_->SetPrefService(nullptr);
    }
    PrefHashFilterTest::TearDown();
  }

  // A mock PrefService that allows us to observe calls to ClearPref.
  class MockPrefService : public TestingPrefServiceSimple {
   public:
    MockPrefService() = default;
    ~MockPrefService() override = default;

    void ClearPref(const std::string& path) {
      cleared_prefs_.insert(path);
      TestingPrefServiceSimple::ClearPref(path);
    }
    bool WasCleared(const std::string& path) const {
      return cleared_prefs_.count(path);
    }

    void ClearClearedPrefsForTesting() { cleared_prefs_.clear(); }

   private:
    std::set<std::string> cleared_prefs_;
  };
  std::unique_ptr<MockPrefService> mock_pref_service_;
};

TEST_P(PrefHashFilterEncryptedTest, FallbackPathInFilterSerializeData) {
  InitializeSyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  base::Value::Dict root_dict;
  root_dict.Set(kAtomicPref, "value");
  base::RunLoop run_loop;
  mock_pref_hash_store_->SetTransactionCompletionCallback(
      run_loop.QuitClosure());
  pref_hash_filter_->FilterSerializeData(root_dict);
  run_loop.Run();
  EXPECT_TRUE(mock_pref_hash_store_->StoreEncryptedHashCalled());
  EXPECT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterEncryptedTest, PostsDeferredTaskOnlyWhenFeatureEnabled) {
  InitializeAsyncOSCrypt();
  mock_pref_service_ = std::make_unique<MockPrefService>();
  // Also register the reset pref as a matter of good practice.
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);

  bool callback_ran = false;
  auto set_callback = [&](base::RunLoop& run_loop) {
    pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(
        base::BindLambdaForTesting([&]() {
          callback_ran = true;
          run_loop.Quit();
        }));
  };

  // 1. Test with feature DISABLED.
  {
    callback_ran = false;
    base::RunLoop completion_run_loop;
    ResetImpl(false, test_os_crypt_async_.get());
    pref_hash_filter_->SetPrefService(mock_pref_service_.get());
    set_callback(completion_run_loop);

    DoFilterOnLoad(false);
    completion_run_loop.Run();
    EXPECT_TRUE(callback_ran);
  }

  // 2. Test with feature ENABLED.
  {
    callback_ran = false;
    base::RunLoop revalidation_loop;
    ResetImpl(true, test_os_crypt_async_.get());
    pref_hash_filter_->SetPrefService(mock_pref_service_.get());
    set_callback(revalidation_loop);

    pref_hash_filter_->FilterOnLoad(
        base::BindOnce(&PrefHashFilterTest::GetPrefsBack,
                       base::Unretained(this), false /* expected_altered */),
        pref_store_contents_.Clone());

    EXPECT_FALSE(callback_ran);
    revalidation_loop.Run();
    EXPECT_TRUE(callback_ran);
  }
}

TEST_P(PrefHashFilterEncryptedTest, DeferredRevalidationSkipsIfValueChanged) {
  InitializeAsyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterStringPref(kAtomicPref, "");
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  pref_store_contents_.Set(kAtomicPref, "value_at_load");
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::UNCHANGED);

  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false),
      std::move(pref_store_contents_));

  mock_pref_service_->SetString(kAtomicPref, "value_changed_in_browser");

  // Wait for the deferred revalidation to complete.
  base::RunLoop revalidation_run_loop;
  bool was_validation_performed = false;

  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(base::BindOnce(
      &PrefHashFilterEncryptedTest::OnDeferredRevalidationComplete,
      base::Unretained(this), &was_validation_performed,
      revalidation_run_loop.QuitClosure()));
  mock_pref_hash_store_->ClearTestState();
  revalidation_run_loop.Run();

  EXPECT_FALSE(mock_pref_service_->WasCleared(kAtomicPref));
}

TEST_P(PrefHashFilterEncryptedTest, DeferredRevalidationSkipsIfValueCleared) {
  InitializeAsyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterStringPref(kAtomicPref, "");
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  pref_store_contents_.Set(kAtomicPref, "value_at_load");
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::UNCHANGED);
  // Set the encrypted hash explicitly to be invalid, so the deferred task will
  // try to reset the pref.
  mock_pref_hash_store_->SetCheckResult("prefix." + std::string(kAtomicPref),
                                        ValueState::CHANGED);

  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false),
      std::move(pref_store_contents_));
  mock_pref_service_->ClearPref(kAtomicPref);

  base::RunLoop revalidation_run_loop;
  bool was_validation_performed = false;
  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(base::BindOnce(
      &PrefHashFilterEncryptedTest::OnDeferredRevalidationComplete,
      base::Unretained(this), &was_validation_performed,
      revalidation_run_loop.QuitClosure()));

  mock_pref_service_->ClearClearedPrefsForTesting();
  revalidation_run_loop.Run();

  EXPECT_TRUE(was_validation_performed);

  // This means ClearPref should NOT be called a second time.
  EXPECT_FALSE(mock_pref_service_->WasCleared(kAtomicPref));
}

TEST_P(PrefHashFilterEncryptedTest,
       DeferredRevalidationResetsSplitPrefPartially) {
  // This test is only relevant when enforcement is on.
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  InitializeAsyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterDictionaryPref(kSplitPref);
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  // 1. Set up the initial state with a good and a bad key.
  base::Value::Dict initial_dict;
  initial_dict.Set("good_key", "good_value");
  initial_dict.Set("bad_key", "bad_value");
  pref_store_contents_.Set(kSplitPref, initial_dict.Clone());
  // Also set this initial state in the live PrefService.
  mock_pref_service_->Set(kSplitPref, base::Value(initial_dict.Clone()));

  // 2. Configure the mock for the SYNCHRONOUS pass.
  // The HMACs are all valid, so the initial check is UNCHANGED.
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::UNCHANGED);

  // 3. Run the synchronous load. This should schedule the deferred task.
  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false /* expected_altered */),
      std::move(pref_store_contents_));

  // At this point, nothing should have been cleared.
  ASSERT_FALSE(mock_pref_service_->WasCleared(kSplitPref));

  // 4. Re-configure the mock for the ASYNCHRONOUS pass.
  // Now, the encrypted hash for "bad_key" is found to be invalid.
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::CHANGED_ENCRYPTED);
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, {"bad_key"});

  // 5. Wait for the deferred task to complete.
  base::RunLoop revalidation_run_loop;
  bool callback_ran = false;
  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(base::BindOnce(
      &PrefHashFilterEncryptedTest::OnDeferredRevalidationComplete,
      base::Unretained(this), &callback_ran,
      revalidation_run_loop.QuitClosure()));
  revalidation_run_loop.Run();
  ASSERT_TRUE(callback_ran);

  // 6. VERIFY the results.
  // The whole pref should NOT have been cleared. This is the bug fix check.
  EXPECT_FALSE(mock_pref_service_->WasCleared(kSplitPref));

  // The live pref value should now be the corrected dictionary.
  const base::Value::Dict& final_dict = mock_pref_service_->GetDict(kSplitPref);
  EXPECT_TRUE(final_dict.Find("good_key"));
  EXPECT_FALSE(final_dict.Find("bad_key"));
  EXPECT_EQ(1u, final_dict.size());
}

TEST_P(PrefHashFilterTest, MetricLoggedOnceOnSyncPathFeatureDisabled) {
  // The metric is logged exactly once from the synchronous FinalizeFilterOnLoad
  // pass.
  base::HistogramTester histogram_tester;

  DoFilterOnLoad(false);

  // We expect exactly one sample, with a value of 0 (no resets).
  histogram_tester.ExpectUniqueSample("Settings.TrackedPreferenceResets.Count",
                                      0, 1);
}

TEST_P(PrefHashFilterEncryptedTest, MetricLoggedOnceOnDeferredPath) {
  InitializeAsyncOSCrypt();
  base::HistogramTester histogram_tester;
  base::RunLoop revalidation_loop;

  ResetImpl(true /* enable_encrypted_hashing_feature */,
            test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(
      revalidation_loop.QuitClosure());

  // This will run FinalizeFilterOnLoad, which should post the deferred task but
  // not log the metric.
  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false /* expected_altered */),
      pref_store_contents_.Clone());

  revalidation_loop.Run();

  // We expect exactly one sample, with a value of 0 (no resets).
  histogram_tester.ExpectUniqueSample("Settings.TrackedPreferenceResets.Count",
                                      0, 1);
}

TEST_P(PrefHashFilterTest, MaybeRecordTrackedPreferenceResetCount_LogsOnce) {
  base::HistogramTester histogram_tester;
  pref_hash_filter_->MaybeRecordTrackedPreferenceResetCount(
      pref_store_contents_);
  // Call a second time.
  pref_hash_filter_->MaybeRecordTrackedPreferenceResetCount(
      pref_store_contents_);

  // Should only have one sample.
  histogram_tester.ExpectUniqueSample("Settings.TrackedPreferenceResets.Count",
                                      0, 1);
}

TEST_P(PrefHashFilterEncryptedTest, ResetSplitPrefThenDeferredValidation) {
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }
  InitializeAsyncOSCrypt();
  ResetImpl(true /* enable_encrypted_hashing_feature */,
            test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterDictionaryPref(kSplitPref);
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  base::Value::Dict tampered_dict;
  tampered_dict.Set("some_invalid_key", "this_is_the_tampered_value");
  pref_store_contents_.Set(kSplitPref, std::move(tampered_dict));

  // Configure the mock to report an invalid unencrypted hash.
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, {"some_invalid_key"});

  base::RunLoop run_loop;
  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(
      run_loop.QuitClosure());

  // This will trigger the synchronous reset and schedule the deferred task.
  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     true /* expected_altered */),
      std::move(pref_store_contents_));

  // The invalid key should have been immediately removed, leaving an empty
  // dict.
  const base::Value::Dict* dict_after_sync_reset =
      pref_store_contents_.FindDict(kSplitPref);
  ASSERT_TRUE(dict_after_sync_reset);
  EXPECT_TRUE(dict_after_sync_reset->empty());

  // Clear the mock's call history, then reconfigure it to report that the
  // encrypted hash for the same preference is also invalid.
  mock_pref_hash_store_->ClearTestState();
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);

  run_loop.Run();

  // The `already_reset_paths` logic should have caused the deferred task to
  // completely skip re-validating `kSplitPref`. We prove this by asserting
  // that the mock store's `CheckValue` method was not called.
  EXPECT_EQ(0u, mock_pref_hash_store_->checked_paths_count());

  const base::Value::Dict* dict_after_async_pass =
      pref_store_contents_.FindDict(kSplitPref);
  ASSERT_TRUE(dict_after_async_pass);
  EXPECT_TRUE(dict_after_async_pass->empty());
}

TEST_P(PrefHashFilterEncryptedTest,
       DeferredResetOverwritesExistingResetListInPrefService) {
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  InitializeAsyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterStringPref(kAtomicPref, "");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  pref_store_contents_.Set(kAtomicPref, "value_at_load");
  mock_pref_service_->SetString(kAtomicPref, "value_at_load");

  // Pre populate the live PrefService's reset list with a stale value.
  base::Value::List existing_resets;
  existing_resets.Append("existing.stale.pref");
  mock_pref_service_->SetList(user_prefs::kTrackedPreferencesReset,
                              std::move(existing_resets));

  // The pref_store_contents_at_load won't have the stale pref, as it's
  // only in the live service.
  pref_store_contents_.Set(user_prefs::kTrackedPreferencesReset,
                           base::Value::List());

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::UNCHANGED);
  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false /* expected_altered */),
      std::move(pref_store_contents_));

  ASSERT_EQ(
      1u,
      mock_pref_service_->GetList(user_prefs::kTrackedPreferencesReset).size());

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::CHANGED_ENCRYPTED);

  base::RunLoop revalidation_run_loop;
  bool callback_ran = false;
  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(base::BindOnce(
      &PrefHashFilterEncryptedTest::OnDeferredRevalidationComplete,
      base::Unretained(this), &callback_ran,
      revalidation_run_loop.QuitClosure()));
  revalidation_run_loop.Run();

  ASSERT_TRUE(callback_ran);
  EXPECT_TRUE(mock_pref_service_->GetString(kAtomicPref).empty());

  const base::Value::List& final_list =
      mock_pref_service_->GetList(user_prefs::kTrackedPreferencesReset);
  ASSERT_EQ(1u, final_list.size());
  EXPECT_EQ(kAtomicPref, final_list[0].GetString());
}

TEST_P(PrefHashFilterEncryptedTest,
       SyncResetIsPatchedToLivePrefServiceWhenFlagOff) {
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  InitializeAsyncOSCrypt();
  ResetImpl(false /* enable_encrypted_hashing_feature */,
            test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  mock_pref_service_->registry()->RegisterStringPref(kAtomicPref, "");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  pref_store_contents_.Set(kAtomicPref, "value_at_load");
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);

  // The live pref list should be empty before we start.
  ASSERT_TRUE(mock_pref_service_->GetList(user_prefs::kTrackedPreferencesReset)
                  .empty());

  DoFilterOnLoad(true /* expected_altered */);

  // The live PrefService list should be populated by the
  // UpdateTrackedPreferencesResetListInPrefStore task.
  const base::Value::List& final_list =
      mock_pref_service_->GetList(user_prefs::kTrackedPreferencesReset);
  ASSERT_EQ(1u, final_list.size());
  EXPECT_EQ(kAtomicPref, final_list[0].GetString());
}

TEST_P(PrefHashFilterEncryptedTest, DeferredValidation_TypeMismatch) {
  // This test is only relevant when enforcement is on.
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }

  InitializeAsyncOSCrypt();
  ResetImpl(true, test_os_crypt_async_.get());
  std::vector<prefs::TrackedPreferenceMetadata> custom_prefs = {
      {0, kAtomicPref, EnforcementLevel::ENFORCE_ON_LOAD,
       PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL}};

  InitializePrefHashFilterWithCustomConfig(custom_prefs,
                                           test_os_crypt_async_.get());

  mock_pref_service_ = std::make_unique<MockPrefService>();
  // Register kAtomicPref as an integer.
  mock_pref_service_->registry()->RegisterIntegerPref(kAtomicPref, 0);
  // Register other prefs.
  mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                     "0");
  mock_pref_service_->registry()->RegisterStringPref(
      user_prefs::kPreferenceResetTime, "0");
  mock_pref_service_->registry()->RegisterListPref(
      user_prefs::kTrackedPreferencesReset);
  pref_hash_filter_->SetPrefService(mock_pref_service_.get());

  // Set kAtomicPref as a string in the pref store contents, creating a
  // type mismatch
  pref_store_contents_.Set(kAtomicPref, "invalid_type");

  // Configure the mock for the synchronous pass.
  // The HMACs are all valid, so the initial check is UNCHANGED.
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::UNCHANGED);

  base::HistogramTester histogram_tester;

  // Run the synchronous load. This should schedule the deferred task.
  pref_hash_filter_->FilterOnLoad(
      base::BindOnce(&PrefHashFilterTest::GetPrefsBack, base::Unretained(this),
                     false /* expected_altered */),
      std::move(pref_store_contents_));

  base::RunLoop revalidation_run_loop;
  pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(
      revalidation_run_loop.QuitClosure());
  mock_pref_hash_store_->ClearTestState();
  revalidation_run_loop.Run();
  EXPECT_EQ(0u, mock_pref_hash_store_->checked_paths_count());
  histogram_tester.ExpectUniqueSample(
      "Settings.TrackedPreferences.TypeMismatch.Combination.IntegerToString", 0,
      1);
}

TEST_P(PrefHashFilterEncryptedTest, DetectsAndLogsMismatch_AllPrefs) {
  if (GetParam() != EnforcementLevel::ENFORCE_ON_LOAD) {
    return;
  }
  InitializeAsyncOSCrypt();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(tracked::kEncryptedPrefHashing);

  // We loop over all test cases defined in GetTypeMismatchTestCases to simulate
  // the real world scenarios.
  for (const auto& param : GetTypeMismatchTestCases()) {
    base::HistogramTester histogram_tester;

    std::vector<prefs::TrackedPreferenceMetadata> custom_metadata = {
        {param.reporting_id, param.pref_name, EnforcementLevel::ENFORCE_ON_LOAD,
         param.strategy, ValueType::IMPERSONAL}};

    InitializePrefHashFilterWithCustomConfig(custom_metadata,
                                             test_os_crypt_async_.get());
    // Simulate registering the pref in PrefService.
    mock_pref_service_ = std::make_unique<MockPrefService>();
    switch (param.registered_type) {
      case base::Value::Type::BOOLEAN:
        mock_pref_service_->registry()->RegisterBooleanPref(param.pref_name,
                                                            false);
        break;
      case base::Value::Type::INTEGER:
        mock_pref_service_->registry()->RegisterIntegerPref(param.pref_name, 0);
        break;
      case base::Value::Type::STRING:
        mock_pref_service_->registry()->RegisterStringPref(param.pref_name, "");
        break;
      case base::Value::Type::LIST:
        mock_pref_service_->registry()->RegisterListPref(param.pref_name);
        break;
      case base::Value::Type::DICT:
        mock_pref_service_->registry()->RegisterDictionaryPref(param.pref_name);
        break;
      default:
        FAIL() << "Unsupported registered preference type";
    }
    mock_pref_service_->registry()->RegisterStringPref(kScheduleToFlushToDisk,
                                                       "0");
    mock_pref_service_->registry()->RegisterStringPref(
        user_prefs::kPreferenceResetTime, "0");
    mock_pref_service_->registry()->RegisterListPref(
        user_prefs::kTrackedPreferencesReset);
    pref_hash_filter_->SetPrefService(mock_pref_service_.get());

    base::Value mismatch_value;
    if (param.mismatch_string_value) {
      mismatch_value = base::Value(param.mismatch_string_value);
    } else {
      switch (param.mismatch_type) {
        case base::Value::Type::DICT:
          mismatch_value = base::Value(base::Value::Dict());
          break;
        case base::Value::Type::LIST:
          mismatch_value = base::Value(base::Value::List());
          break;
        default:
          mismatch_value = base::Value("default_mismatch");
          break;
      }
    }
    pref_store_contents_.SetByDottedPath(param.pref_name,
                                         mismatch_value.Clone());
    mock_pref_hash_store_->SetCheckResult(param.pref_name,
                                          ValueState::UNCHANGED);
    pref_hash_filter_->FilterOnLoad(
        base::BindOnce(&PrefHashFilterTest::GetPrefsBack,
                       base::Unretained(this), false),
        pref_store_contents_.Clone());

    mock_pref_hash_store_->ClearTestState();

    base::RunLoop revalidation_run_loop;
    bool callback_ran = false;
    pref_hash_filter_->SetOnDeferredRevalidationCompleteForTesting(
        base::BindOnce(
            &PrefHashFilterEncryptedTest::OnDeferredRevalidationComplete,
            base::Unretained(this), &callback_ran,
            revalidation_run_loop.QuitClosure()));

    revalidation_run_loop.Run();
    ASSERT_TRUE(callback_ran);

    // Verify that CheckValue was never called as the type mismatch should skip
    // validation.
    EXPECT_EQ(0u, mock_pref_hash_store_->checked_paths_count());

    std::string combo_name =
        base::StrCat({"Settings.TrackedPreferences.TypeMismatch.Combination.",
                      GetValueTypeStringForTest(param.registered_type), "To",
                      GetValueTypeStringForTest(mismatch_value.type())});
    histogram_tester.ExpectUniqueSample(combo_name, param.reporting_id, 1);
    pref_hash_filter_->SetPrefService(nullptr);
    pref_store_contents_.clear();
  }
}
INSTANTIATE_TEST_SUITE_P(PrefHashFilterTestInstance,
                         PrefHashFilterEncryptedTest,
                         testing::Values(EnforcementLevel::NO_ENFORCEMENT,
                                         EnforcementLevel::ENFORCE_ON_LOAD));
