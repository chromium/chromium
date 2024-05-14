// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_store_impl.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "services/preferences/tracked/device_id.h"
#include "services/preferences/tracked/hash_store_contents.h"

namespace {

using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

// Returns a deterministic ID for this machine.
std::string GenerateDeviceId() {
  static base::NoDestructor<std::string> cached_device_id;
  if (!cached_device_id->empty())
    return *cached_device_id;

  std::string device_id;
  MachineIdStatus status = GetDeterministicMachineSpecificId(&device_id);
  DCHECK(status == MachineIdStatus::NOT_IMPLEMENTED ||
         status == MachineIdStatus::SUCCESS);

  if (status == MachineIdStatus::SUCCESS) {
    *cached_device_id = device_id;
    return device_id;
  }

  return std::string();
}

}  // namespace

class PrefHashStoreImpl::PrefHashStoreTransactionImpl
    : public PrefHashStoreTransaction {
 public:
  // Constructs a PrefHashStoreTransactionImpl which can use the private
  // members of its |outer| PrefHashStoreImpl.
  PrefHashStoreTransactionImpl(PrefHashStoreImpl* outer,
                               HashStoreContents* storage);

  PrefHashStoreTransactionImpl(const PrefHashStoreTransactionImpl&) = delete;
  PrefHashStoreTransactionImpl& operator=(const PrefHashStoreTransactionImpl&) =
      delete;

  ~PrefHashStoreTransactionImpl() override;

  // PrefHashStoreTransaction implementation.
  std::string_view GetStoreUMASuffix() const override;
  ValueState CheckValue(const std::string& path,
                        const base::Value* value) const override;
  void StoreHash(const std::string& path, const base::Value* value) override;
  ValueState CheckSplitValue(
      const std::string& path,
      const base::Value::Dict* initial_split_value,
      std::vector<std::string>* invalid_keys) const override;
  void StoreSplitHash(const std::string& path,
                      const base::Value::Dict* split_value) override;
  bool HasHash(const std::string& path) const override;
  void ImportHash(const std::string& path, const base::Value* hash) override;
  void ClearHash(const std::string& path) override;
  bool IsSuperMACValid() const override;
  bool StampSuperMac() override;

 private:
  raw_ptr<PrefHashStoreImpl> outer_;
  raw_ptr<HashStoreContents> contents_;

  bool super_mac_valid_;
  bool super_mac_dirty_;
};

PrefHashStoreImpl::PrefHashStoreImpl(const std::string& seed,
                                     const std::string& legacy_device_id,
                                     bool use_super_mac)
    : pref_hash_calculator_(seed, GenerateDeviceId(), legacy_device_id),
      use_super_mac_(use_super_mac) {}

PrefHashStoreImpl::~PrefHashStoreImpl() {}

std::unique_ptr<PrefHashStoreTransaction> PrefHashStoreImpl::BeginTransaction(
    HashStoreContents* storage) {
  return std::unique_ptr<PrefHashStoreTransaction>(
      new PrefHashStoreTransactionImpl(this, storage));
}

std::string PrefHashStoreImpl::ComputeMac(const std::string& path,
                                          const base::Value* value) {
  return pref_hash_calculator_.Calculate(path, value);
}

std::string PrefHashStoreImpl::ComputeMac(const std::string& path,
                                          const base::Value::Dict* dict) {
  return pref_hash_calculator_.Calculate(path, dict);
}

base::Value::Dict PrefHashStoreImpl::ComputeSplitMacs(
    const std::string& path,
    const base::Value::Dict* split_values) {
  if (!split_values)
    return base::Value::Dict();

  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();

  base::Value::Dict split_macs;

  for (const auto item : *split_values) {
    // Keep the common part from the old |keyed_path| and replace the key to
    // get the new |keyed_path|.
    keyed_path.replace(common_part_length, std::string::npos, item.first);

    split_macs.Set(item.first, ComputeMac(keyed_path, &item.second));
  }

  return split_macs;
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer,
    HashStoreContents* storage)
    : outer_(outer),
      contents_(storage),
      super_mac_valid_(false),
      super_mac_dirty_(false) {
  if (!outer_->use_super_mac_)
    return;

  // The store must have a valid super MAC to be trusted.
  std::string super_mac = contents_->GetSuperMac();
  if (super_mac.empty())
    return;

  super_mac_valid_ =
      outer_->pref_hash_calculator_.Validate(
          "", contents_->GetContents(), super_mac) == PrefHashCalculator::VALID;
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::
    ~PrefHashStoreTransactionImpl() {
  if (super_mac_dirty_ && outer_->use_super_mac_) {
    // Get the dictionary of hashes (or NULL if it doesn't exist).
    const base::Value::Dict* hashes_dict = contents_->GetContents();
    contents_->SetSuperMac(outer_->ComputeMac("", hashes_dict));
  }
}

std::string_view
PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetStoreUMASuffix() const {
  return contents_->GetUMASuffix();
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path,
    const base::Value* initial_value) const {
  std::string last_hash;
  contents_->GetMac(path, &last_hash);

  if (last_hash.empty()) {
    // In the absence of a hash for this pref, always trust a NULL value, but
    // only trust an existing value if the initial hashes dictionary is trusted.
    if (!initial_value)
      return ValueState::TRUSTED_NULL_VALUE;
    else if (super_mac_valid_)
      return ValueState::TRUSTED_UNKNOWN_VALUE;
    else
      return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  PrefHashCalculator::ValidationResult validation_result =
      outer_->pref_hash_calculator_.Validate(path, initial_value, last_hash);
  switch (validation_result) {
    case PrefHashCalculator::VALID:
      return ValueState::UNCHANGED;
    case PrefHashCalculator::VALID_SECURE_LEGACY:
      return ValueState::SECURE_LEGACY;
    case PrefHashCalculator::INVALID:
      return initial_value ? ValueState::CHANGED : ValueState::CLEARED;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unexpected PrefHashCalculator::ValidationResult: "
      << validation_result;
  return ValueState::UNTRUSTED_UNKNOWN_VALUE;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHash(
    const std::string& path,
    const base::Value* new_value) {
  const std::string mac = outer_->ComputeMac(path, new_value);
  contents_->SetMac(path, mac);
  super_mac_dirty_ = true;
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValue(
    const std::string& path,
    const base::Value::Dict* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  DCHECK(invalid_keys && invalid_keys->empty());

  std::map<std::string, std::string> split_macs;
  const bool has_hashes = contents_->GetSplitMacs(path, &split_macs);

  // Treat NULL and empty the same; otherwise we would need to store a hash for
  // the entire dictionary (or some other special beacon) to differentiate these
  // two cases which are really the same for dictionaries.
  if (!initial_split_value || initial_split_value->empty()) {
    return has_hashes ? ValueState::CLEARED : ValueState::UNCHANGED;
  }

  if (!has_hashes)
    return super_mac_valid_ ? ValueState::TRUSTED_UNKNOWN_VALUE
                            : ValueState::UNTRUSTED_UNKNOWN_VALUE;

  bool has_secure_legacy_id_hashes = false;
  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();
  for (const auto item : *initial_split_value) {
    std::map<std::string, std::string>::iterator entry =
        split_macs.find(item.first);
    if (entry == split_macs.end()) {
      invalid_keys->push_back(item.first);
    } else {
      // Keep the common part from the old |keyed_path| and replace the key to
      // get the new |keyed_path|.
      keyed_path.replace(common_part_length, std::string::npos, item.first);
      switch (outer_->pref_hash_calculator_.Validate(keyed_path, &item.second,
                                                     entry->second)) {
        case PrefHashCalculator::VALID:
          break;
        case PrefHashCalculator::VALID_SECURE_LEGACY:
          // Secure legacy device IDs based hashes are still accepted, but we
          // should make sure to notify the caller for them to update the legacy
          // hashes.
          has_secure_legacy_id_hashes = true;
          break;
        case PrefHashCalculator::INVALID:
          invalid_keys->push_back(item.first);
          break;
      }
      // Remove processed MACs, remaining MACs at the end will also be
      // considered invalid.
      split_macs.erase(entry);
    }
  }

  // Anything left in the map is missing from the data.
  for (std::map<std::string, std::string>::const_iterator it =
           split_macs.begin();
       it != split_macs.end(); ++it) {
    invalid_keys->push_back(it->first);
  }

  return invalid_keys->empty()
             ? (has_secure_legacy_id_hashes ? ValueState::SECURE_LEGACY
                                            : ValueState::UNCHANGED)
             : ValueState::CHANGED;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitHash(
    const std::string& path,
    const base::Value::Dict* split_value) {
  contents_->RemoveEntry(path);

  if (split_value) {
    base::Value::Dict split_macs = outer_->ComputeSplitMacs(path, split_value);

    for (const auto item : split_macs) {
      DCHECK(item.second.is_string());

      contents_->SetSplitMac(path, item.first, item.second.GetString());
    }
  }
  super_mac_dirty_ = true;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasHash(
    const std::string& path) const {
  std::string out_value;
  std::map<std::string, std::string> out_values;
  return contents_->GetMac(path, &out_value) ||
         contents_->GetSplitMacs(path, &out_values);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ImportHash(
    const std::string& path,
    const base::Value* hash) {
  DCHECK(hash);

  contents_->ImportEntry(path, hash);

  if (super_mac_valid_)
    super_mac_dirty_ = true;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearHash(
    const std::string& path) {
  if (contents_->RemoveEntry(path) && super_mac_valid_) {
    super_mac_dirty_ = true;
  }
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::IsSuperMACValid() const {
  return super_mac_valid_;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::StampSuperMac() {
  if (!outer_->use_super_mac_ || super_mac_valid_)
    return false;
  super_mac_dirty_ = true;
  return true;
}
