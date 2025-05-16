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

using ValidationResult = PrefHashCalculator::ValidationResult;
using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

// Suffix used to distinguish encrypted hash keys from MAC keys in storage.
const char kEncryptedHashKeySuffix[] = "_encrypted_hash";

// Keys expected in the dictionary passed to ImportHash if it contains
// structured data.
const char kImportMacKey[] = "mac";
const char kImportEncryptedHashKey[] = "encrypted_hash";

// Helper to create the key used for storing encrypted hashes.
std::string GetEncryptedHashKey(const std::string& path) {
  return path + kEncryptedHashKeySuffix;
}

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
                               HashStoreContents* storage,
                               const os_crypt_async::Encryptor* encryptor);

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

  void StoreEncryptedHash(const std::string& path,
                          const base::Value* value) override;
  std::optional<std::string> GetEncryptedHash(
      const std::string& path) const override;
  std::optional<std::string> GetMac(const std::string& path) const override;
  bool HasEncryptedHash(const std::string& path) const override;

  // Stores the new split Encrypted Hashes. Requires the encryptor.
  void StoreSplitEncryptedHash(const std::string& path,
                               const base::Value::Dict* split_value);

  // Clears only the Encrypted Hash for the path.
  void ClearEncryptedHash(const std::string& path);

  // Gets the stored split encrypted hashes if they exist. Returns false
  // otherwise.
  bool GetSplitEncryptedHashes(
      const std::string& path,
      std::map<std::string, std::string>* split_encrypted_hashes) const;

 private:
  // Helper for CheckValue to handle validation logic.
  ValueState CheckValueInternal(
      const std::string& path,
      const base::Value* value,
      const std::optional<std::string>& stored_encrypted_hash,
      const std::optional<std::string>& stored_mac) const;

  // Helper for CheckSplitValue to handle validation logic.
  ValueState CheckSplitValueInternal(
      const std::string& path,
      const base::Value::Dict* initial_split_value,
      bool has_encrypted_hashes,
      const std::map<std::string, std::string>& split_encrypted_hashes,
      bool has_mac_hashes,
      const std::map<std::string, std::string>& split_macs,
      std::vector<std::string>* invalid_keys) const;

 private:
  raw_ptr<PrefHashStoreImpl> outer_;
  raw_ptr<HashStoreContents> contents_;
  raw_ptr<const os_crypt_async::Encryptor> encryptor_;

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
    HashStoreContents* storage,
    const os_crypt_async::Encryptor* encryptor) {
  return std::make_unique<PrefHashStoreTransactionImpl>(this, storage,
                                                        encryptor);
}

// Computes the legacy MAC.
std::string PrefHashStoreImpl::ComputeMac(const std::string& path,
                                          const base::Value* value) {
  return pref_hash_calculator_.Calculate(path, value);
}

// Computes the legacy MAC for a dictionary.
std::string PrefHashStoreImpl::ComputeMac(const std::string& path,
                                          const base::Value::Dict* dict) {
  return pref_hash_calculator_.Calculate(path, dict);
}

// Computes the split legacy MACs.
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

// Computes the encrypted hash.
std::string PrefHashStoreImpl::ComputeEncryptedHash(
    const std::string& path,
    const base::Value* value,
    const os_crypt_async::Encryptor* encryptor) {
  DCHECK(encryptor);
  std::optional<std::string> result_opt =
      pref_hash_calculator_.CalculateEncryptedHash(path, value, encryptor);

  return result_opt.value_or(std::string());
}

// Computes the encrypted hash for a dictionary.
std::string PrefHashStoreImpl::ComputeEncryptedHash(
    const std::string& path,
    const base::Value::Dict* dict,
    const os_crypt_async::Encryptor* encryptor) {
  DCHECK(encryptor);
  std::optional<std::string> result_opt =
      pref_hash_calculator_.CalculateEncryptedHash(path, dict, encryptor);

  return result_opt.value_or(std::string());
}

// Computes split encrypted hashes.
base::Value::Dict PrefHashStoreImpl::ComputeSplitEncryptedHashes(
    const std::string& path,
    const base::Value::Dict* split_values,
    const os_crypt_async::Encryptor* encryptor) {
  if (!encryptor) {
    return base::Value::Dict();
  }
  if (!split_values || split_values->empty()) {
    return base::Value::Dict();
  }

  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();

  base::Value::Dict split_encrypted_hashes;
  for (const auto item : *split_values) {
    keyed_path.replace(common_part_length, std::string::npos, item.first);

    std::optional<std::string> result_opt =
        pref_hash_calculator_.CalculateEncryptedHash(keyed_path, &item.second,
                                                     encryptor);

    if (result_opt.has_value()) {
      split_encrypted_hashes.Set(item.first, std::move(*result_opt));
    }
  }
  return split_encrypted_hashes;
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer,
    HashStoreContents* storage,
    const os_crypt_async::Encryptor* encryptor_ptr)
    : outer_(outer),
      contents_(storage),
      encryptor_(encryptor_ptr),
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

std::optional<std::string>
PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetEncryptedHash(
    const std::string& path) const {
  std::string encrypted_hash;
  if (contents_->GetMac(GetEncryptedHashKey(path), &encrypted_hash)) {
    return encrypted_hash;
  }
  return std::nullopt;
}

std::optional<std::string>
PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetMac(
    const std::string& path) const {
  std::string mac_str;
  // Get the MAC string from the HashStoreContents.
  if (contents_->GetMac(path, &mac_str)) {
    return mac_str;
  }
  return std::nullopt;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetSplitEncryptedHashes(
    const std::string& path,
    std::map<std::string, std::string>* split_encrypted_hashes) const {
  DCHECK(split_encrypted_hashes);
  split_encrypted_hashes->clear();
  // Use the suffixed key to retrieve split encrypted hashes
  return contents_->GetSplitMacs(GetEncryptedHashKey(path),
                                 split_encrypted_hashes);
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValueInternal(
    const std::string& path,
    const base::Value* value,
    const std::optional<std::string>& stored_encrypted_hash,
    const std::optional<std::string>& stored_mac) const {
  // -- Priority 1: Check encrypted hash --
  if (stored_encrypted_hash.has_value()) {
    if (encryptor_) {
      ValidationResult encrypted_validation_result =
          outer_->pref_hash_calculator_.ValidateEncrypted(
              path, value, *stored_encrypted_hash, encryptor_);

      if (encrypted_validation_result == ValidationResult::VALID_ENCRYPTED) {
        return ValueState::UNCHANGED;
      } else {
        // Encrypted hash is invalid or decryption failed. Do NOT fall back.
        return value ? ValueState::CHANGED : ValueState::CLEARED;
      }
    }
  }

  // --- Priority 2: Check legacy HMAC ---
  if (stored_mac.has_value()) {
    ValidationResult mac_validation_result =
        outer_->pref_hash_calculator_.Validate(path, value, *stored_mac);
    switch (mac_validation_result) {
      case ValidationResult::VALID:
        // If we fell through from encrypted (which was unusable), a valid MAC
        // still means the value is UNCHANGED.
        return ValueState::UNCHANGED;
      // TODO(crbug.com/415789156): Remove VALID_SECURE_LEGACY from
      // ValidationResult.
      case ValidationResult::VALID_SECURE_LEGACY:
        // If we fell through from encrypted, MAC is valid => SECURE_LEGACY
        return ValueState::SECURE_LEGACY;
      case ValidationResult::INVALID:
        // If encrypted was present but unvalidatable, OR if only MAC was
        // present and invalid
        return value ? ValueState::CHANGED : ValueState::CLEARED;
      default:
        NOTREACHED() << "Unexpected PrefHashCalculator::ValidationResult: "
                     << mac_validation_result;
    }
  }

  // --- No Usable Hashes Found ---
  // Arrive here if:
  // 1. No hashes stored at all.
  // 2. ONLY encrypted hash stored, but no encryptor (fell through above).
  if (!value) {
    // Null value is always trusted if no usable hash is present
    return ValueState::TRUSTED_NULL_VALUE;
  }

  // If we got here ONLY because an encrypted hash was present but unusable
  // (due to missing encryptor), treat the value as untrusted regardless of
  // the (potentially stale) super_mac_valid_ flag.
  if (stored_encrypted_hash.has_value() && !stored_mac.has_value() &&
      !encryptor_) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // Otherwise (genuinely no hashes stored), base trust on the SuperMAC validity
  // state *cached at the start of the transaction*.
  if (super_mac_valid_) {
    return ValueState::TRUSTED_UNKNOWN_VALUE;
  } else {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path,
    const base::Value* initial_value) const {
  // Attempt to retrieve both types of hashes.
  std::optional<std::string> encrypted_hash = GetEncryptedHash(path);
  std::optional<std::string> mac;
  std::string mac_str;
  if (contents_->GetMac(path, &mac_str)) {
    mac = mac_str;
  }

  // Delegate to the internal helper.
  return CheckValueInternal(path, initial_value, encrypted_hash, mac);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHash(
    const std::string& path,
    const base::Value* new_value) {
  const std::string mac = outer_->ComputeMac(path, new_value);
  contents_->SetMac(path, mac);
  super_mac_dirty_ = true;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreEncryptedHash(
    const std::string& path,
    const base::Value* value) {
  if (!encryptor_) {
    return;
  }

  const std::string encrypted_hash_str =
      outer_->ComputeEncryptedHash(path, value, encryptor_);

  std::string enc_key = GetEncryptedHashKey(path);

  // ComputeEncryptedHash from PrefHashStoreImpl returns "" on failure.
  if (!encrypted_hash_str.empty()) {
    // Calculation and encryption were successful, store it.
    contents_->SetMac(enc_key, encrypted_hash_str);
    super_mac_dirty_ = true;
  } else {
    // Computation failed, ensure no (potentially old or empty) hash is stored.
    if (contents_->RemoveEntry(enc_key)) {
      super_mac_dirty_ = true;
    }
  }
}

ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValueInternal(
    const std::string& path,
    const base::Value::Dict* initial_split_value,
    bool has_encrypted_hashes,
    const std::map<std::string, std::string>& split_encrypted_hashes,
    bool has_mac_hashes,
    const std::map<std::string, std::string>& split_macs,
    std::vector<std::string>* invalid_keys) const {
  DCHECK(invalid_keys && invalid_keys->empty());

  const bool is_initial_value_empty =
      (!initial_split_value || initial_split_value->empty());

  bool try_mac_fallback = false;
  bool only_unusable_encrypted_present = false;

  // --- Priority 1: Check split encrypted hashes ---
  if (has_encrypted_hashes) {
    if (encryptor_) {
      if (is_initial_value_empty) {
        return ValueState::CLEARED;
      }
      bool any_invalid = false;
      std::map<std::string, std::string> current_encrypted =
          split_encrypted_hashes;
      std::string keyed_path_base = path + ".";
      if (initial_split_value) {
        for (const auto item : *initial_split_value) {
          std::string keyed_path = keyed_path_base + item.first;
          auto it = current_encrypted.find(item.first);
          if (it == current_encrypted.end() ||
              outer_->pref_hash_calculator_.ValidateEncrypted(
                  keyed_path, &item.second, it->second, encryptor_) !=
                  ValidationResult::VALID_ENCRYPTED) {
            invalid_keys->push_back(item.first);
            any_invalid = true;
          }
          if (it != current_encrypted.end()) {
            current_encrypted.erase(it);
          }
        }
      }
      for (const auto& pair : current_encrypted) {
        invalid_keys->push_back(pair.first);
        any_invalid = true;
      }
      return any_invalid ? ValueState::CHANGED : ValueState::UNCHANGED;
    } else {
      try_mac_fallback = true;
      if (!has_mac_hashes) {
        only_unusable_encrypted_present = true;
      }
    }
  }

  // --- Priority 2: Check legacy split HMACs ---
  // Proceed if no encrypted hashes were found OR if fallback was indicated
  if (has_mac_hashes && (!has_encrypted_hashes || try_mac_fallback)) {
    if (is_initial_value_empty) {
      return ValueState::CLEARED;
    }
    bool any_invalid = false;
    bool has_secure_legacy = false;
    std::map<std::string, std::string> current_macs = split_macs;
    std::string keyed_path_base = path + ".";
    if (initial_split_value) {
      for (const auto item : *initial_split_value) {
        std::string keyed_path = keyed_path_base + item.first;
        auto it = current_macs.find(item.first);
        if (it == current_macs.end()) {
          invalid_keys->push_back(item.first);
          any_invalid = true;
        } else {
          ValidationResult result = outer_->pref_hash_calculator_.Validate(
              keyed_path, &item.second, it->second);
          if (result == ValidationResult::INVALID) {
            invalid_keys->push_back(item.first);
            any_invalid = true;
          } else if (result == ValidationResult::VALID_SECURE_LEGACY) {
            has_secure_legacy = true;
          }
          current_macs.erase(it);
        }
      }
    }
    for (const auto& pair : current_macs) {
      invalid_keys->push_back(pair.first);
      any_invalid = true;
    }
    return any_invalid ? ValueState::CHANGED
                       : (has_secure_legacy ? ValueState::SECURE_LEGACY
                                            : ValueState::UNCHANGED);
  }

  // --- No Usable Hashes Found ---
  // Arrive here if:
  // 1. No hashes stored at all.
  // 2. ONLY encrypted hashes stored, but no encryptor (fell through).
  if (is_initial_value_empty) {
    return ValueState::UNCHANGED;
  }

  if (only_unusable_encrypted_present) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // Otherwise (genuinely no hashes at all, or MACs were checked and failed),
  // base trust on SuperMAC validity *cached at the start of the transaction*.
  if (super_mac_valid_) {
    return ValueState::TRUSTED_UNKNOWN_VALUE;
  } else {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValue(
    const std::string& path,
    const base::Value::Dict* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  // Attempt to retrieve both types of split hashes.
  std::map<std::string, std::string> split_encrypted_hashes;
  bool has_encrypted = GetSplitEncryptedHashes(path, &split_encrypted_hashes);

  std::map<std::string, std::string> split_macs;
  bool has_macs = contents_->GetSplitMacs(path, &split_macs);

  return CheckSplitValueInternal(path, initial_split_value, has_encrypted,
                                 split_encrypted_hashes, has_macs, split_macs,
                                 invalid_keys);
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

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitEncryptedHash(
    const std::string& path,
    const base::Value::Dict* split_value) {
  // Encrypted hash requires the encryptor.
  if (!encryptor_) {
    return;
  }

  // Remove any existing single MAC entry for the base path.
  contents_->RemoveEntry(path);
  // Also remove any existing single *encrypted hash* entry for the base path
  contents_->RemoveEntry(GetEncryptedHashKey(path));

  // Use the derived key for storing split encrypted hashes.
  const std::string encrypted_hash_base_key = GetEncryptedHashKey(path);

  if (split_value) {
    base::Value::Dict split_encrypted_hashes =
        outer_->ComputeSplitEncryptedHashes(path, split_value, encryptor_);

    for (const auto item : split_encrypted_hashes) {
      DCHECK(item.second.is_string());
      // Store using the derived base key.
      contents_->SetSplitMac(encrypted_hash_base_key, item.first,
                             item.second.GetString());
    }
  }
  super_mac_dirty_ = true;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasHash(
    const std::string& path) const {
  std::string out_value;
  std::map<std::string, std::string> out_values;
  return HasEncryptedHash(path) || contents_->GetMac(path, &out_value) ||
         contents_->GetSplitMacs(path, &out_values);
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasEncryptedHash(
    const std::string& path) const {
  std::string out_value;
  const std::string encrypted_key = GetEncryptedHashKey(path);
  std::map<std::string, std::string> out_values;
  return contents_->GetMac(encrypted_key, &out_value) ||
         contents_->GetSplitMacs(encrypted_key, &out_values);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ImportHash(
    const std::string& path,
    const base::Value* hash) {
  DCHECK(hash);
  bool changed = false;

  if (hash->is_string()) {
    // --- Case 1: Input is a string ---
    // Legacy MAC. Import it and clear any existing encrypted
    // hash.
    contents_->ImportEntry(path, hash);
    if (contents_->RemoveEntry(GetEncryptedHashKey(path))) {
      changed = true;
    }
    // ImportEntry itself implies a change, so mark dirty regardless of
    // RemoveEntry result.
    changed = true;

  } else if (hash->is_dict()) {
    // --- Case 2: Input is a dict ---
    const base::Value::Dict& dict = hash->GetDict();

    // Handle MAC part
    const std::string* mac_str_ptr = dict.FindString(kImportMacKey);
    if (mac_str_ptr) {
      // Import the MAC if found in the dictionary
      base::Value mac_value(*mac_str_ptr);
      contents_->ImportEntry(path, &mac_value);
      changed = true;
    } else {
      // If "mac" key is NOT in the dictionary, clear any existing MAC for this
      // path.
      if (contents_->RemoveEntry(path)) {
        changed = true;
      }
    }

    // Handle Encrypted Hash part
    const std::string* encrypted_hash_str_ptr =
        dict.FindString(kImportEncryptedHashKey);
    if (encrypted_hash_str_ptr) {
      // Import the encrypted hash if found in the dictionary, using the derived
      // key.
      base::Value encrypted_hash_value(*encrypted_hash_str_ptr);
      contents_->ImportEntry(GetEncryptedHashKey(path), &encrypted_hash_value);
      changed = true;
    } else {
      // If "encrypted_hash" key is NOT in the dictionary, clear any existing
      // encrypted hash for this path (using the derived key).
      if (contents_->RemoveEntry(GetEncryptedHashKey(path))) {
        changed = true;
      }
    }

  } else {
    return;
  }

  // If any import or removal happened and the store was considered valid, mark
  // super MAC as dirty.
  if (changed && super_mac_valid_) {
    super_mac_dirty_ = true;
  } else if (hash->is_string() || hash->is_dict()) {
    if (super_mac_valid_) {
      super_mac_dirty_ = true;
    }
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearHash(
    const std::string& path) {
  bool changed = false;
  std::string enc_key = GetEncryptedHashKey(path);  // Get derived key once

  // Remove atomic MAC entry OR split MAC dictionary at 'path'
  if (contents_->RemoveEntry(path)) {
    changed = true;
  }

  // Remove atomic Encrypted Hash entry OR split encrypted hash dictionary at
  // derived key
  if (contents_->RemoveEntry(enc_key)) {
    changed = true;
  }

  // Mark SuperMAC dirty only if something was actually removed AND if the
  // SuperMAC was considered valid at the start of the transaction.
  if (changed && super_mac_valid_) {
    super_mac_dirty_ = true;
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearEncryptedHash(
    const std::string& path) {
  // Clear only the Encrypted Hash (atomic and split) using the derived key.
  if (contents_->RemoveEntry(GetEncryptedHashKey(path)) && super_mac_valid_) {
    super_mac_dirty_ = true;
  }
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::IsSuperMACValid() const {
  return super_mac_valid_;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::StampSuperMac() {
  if (!outer_->use_super_mac_) {
    return false;
  }
  super_mac_dirty_ = true;
  super_mac_valid_ = true;
  return true;
}
