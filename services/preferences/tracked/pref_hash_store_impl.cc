// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_store_impl.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"
#include "services/preferences/tracked/device_id.h"
#include "services/preferences/tracked/features.h"
#include "services/preferences/tracked/hash_store_contents.h"

namespace {

using ValidationResult = PrefHashCalculator::ValidationResult;
using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

// Suffix used to distinguish encrypted hash keys from HMAC keys in storage.
const char kEncryptedHashKeySuffix[] = "_encrypted_hash";

// Keys expected in the dictionary passed to ImportAuthData if it contains
// structured data.
const char kImportHmacKey[] = "mac";
const char kImportEncryptedHashKey[] = "encrypted_hash";

// Helper to create the key used for storing encrypted hashes.
std::string GetEncryptedHashKey(const std::string& path) {
  return path + kEncryptedHashKeySuffix;
}

// Returns a deterministic ID for this machine.
std::string GenerateDeviceId() {
  static base::NoDestructor<std::string> cached_device_id;
  if (!cached_device_id->empty()) {
    return *cached_device_id;
  }

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

void MaybeReportWeakHash(ValidationResult validation_result,
                         std::optional<size_t> reporting_id) {
  if (!reporting_id.has_value()) {
    return;
  }
  if (validation_result != ValidationResult::WEAK_HASH_ENCRYPTED) {
    return;
  }
  base::UmaHistogramExactLinear("Settings.TrackedPreferences.WeakAlgorithm",
                                reporting_id.value(), /*exclusive_max=*/101);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SuperEncryptedHashResult)
enum class SuperEncryptedHashResult {
  kMatch = 0,
  kMismatch = 1,
  kMissing = 2,
  kMaxValue = kMissing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:SuperEncryptedHashResult)

}  // namespace

class PrefHashStoreImpl::PrefHashStoreTransactionImpl
    : public PrefHashStoreTransaction {
 public:
  // Constructs a PrefHashStoreTransactionImpl which can use the private
  // members of its |outer| PrefHashStoreImpl.
  PrefHashStoreTransactionImpl(
      PrefHashStoreImpl* outer,
      HashStoreContents* storage,
      scoped_refptr<const os_crypt_async::Encryptor> encryptor);

  PrefHashStoreTransactionImpl(const PrefHashStoreTransactionImpl&) = delete;
  PrefHashStoreTransactionImpl& operator=(const PrefHashStoreTransactionImpl&) =
      delete;

  ~PrefHashStoreTransactionImpl() override;

  // PrefHashStoreTransaction implementation.
  std::string_view GetStoreUMASuffix() const override;
  ValueState CheckValue(const std::string& path,
                        const base::Value* value,
                        std::optional<size_t> reporting_id) const override;
  void StoreHmac(const std::string& path, const base::Value* value) override;
  ValueState CheckSplitValue(const std::string& path,
                             const base::DictValue* initial_split_value,
                             std::vector<std::string>* invalid_keys,
                             std::optional<size_t> reporting_id) const override;
  void StoreSplitHmac(const std::string& path,
                      const base::DictValue* split_value) override;
  bool HasAuthenticator(const std::string& path) const override;
  void ImportAuthData(const std::string& path,
                      const base::Value* auth_data) override;
  void ClearAuthenticators(const std::string& path) override;
  bool IsSuperHmacValid() const override;
  bool StampSuperHmac() override;

  void StoreEncryptedHash(const std::string& path,
                          const base::Value* value) override;
  std::optional<std::string> GetEncryptedHash(
      const std::string& path) const override;
  std::optional<std::string> GetHmac(const std::string& path) const override;
  bool HasEncryptedHash(const std::string& path) const override;

  // Stores the new split Encrypted Hashes. Requires the encryptor.
  void StoreSplitEncryptedHash(const std::string& path,
                               const base::DictValue* split_value) override;

  // Clears only the Encrypted Hash for the path.
  void ClearEncryptedHash(const std::string& path) override;

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
      const std::optional<std::string>& stored_hmac,
      std::optional<size_t> reporting_id) const;

  // Helper for CheckSplitValue to handle validation logic.
  ValueState CheckSplitValueInternal(
      const std::string& path,
      const base::DictValue* initial_split_value,
      bool has_encrypted_hashes,
      const std::map<std::string, std::string>& split_encrypted_hashes,
      bool has_hmacs,
      const std::map<std::string, std::string>& split_hmacs,
      std::vector<std::string>* invalid_keys,
      std::optional<size_t> reporting_id) const;

 private:
  raw_ptr<PrefHashStoreImpl> outer_;
  raw_ptr<HashStoreContents> contents_;
  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;

  bool super_hmac_valid_ = false;
  bool super_hmac_dirty_ = false;
  bool super_encrypted_hash_valid_ = false;
  bool super_encrypted_hash_dirty_ = false;
  bool super_encrypted_hash_mismatch_ = false;
};

PrefHashStoreImpl::PrefHashStoreImpl(const std::string& seed,
                                     bool use_super_hmac,
                                     bool use_super_encrypted_hash)
    : pref_hash_calculator_(seed, GenerateDeviceId()),
      use_super_hmac_(use_super_hmac),
      use_super_encrypted_hash_(use_super_encrypted_hash) {}

PrefHashStoreImpl::~PrefHashStoreImpl() {}

std::unique_ptr<PrefHashStoreTransaction> PrefHashStoreImpl::BeginTransaction(
    HashStoreContents* storage,
    scoped_refptr<const os_crypt_async::Encryptor> encryptor) {
  return std::make_unique<PrefHashStoreTransactionImpl>(this, storage,
                                                        encryptor);
}

// Computes the legacy HMAC.
std::string PrefHashStoreImpl::ComputeHmac(const std::string& path,
                                           const base::Value* value) {
  return pref_hash_calculator_.CalculateHmac(path, value);
}

// Computes the legacy HMAC for a dictionary.
std::string PrefHashStoreImpl::ComputeHmac(const std::string& path,
                                           const base::DictValue* dict) {
  return pref_hash_calculator_.CalculateHmac(path, dict);
}

// Computes the split legacy HMACs.
base::DictValue PrefHashStoreImpl::ComputeSplitHmacs(
    const std::string& path,
    const base::DictValue* split_values) {
  if (!split_values) {
    return base::DictValue();
  }

  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();

  base::DictValue split_macs;

  for (const auto [key, split_value] : *split_values) {
    // Keep the common part from the old |keyed_path| and replace the key to
    // get the new |keyed_path|.
    keyed_path.replace(common_part_length, std::string::npos, key);

    split_macs.Set(key, ComputeHmac(keyed_path, &split_value));
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
    const base::DictValue* dict,
    const os_crypt_async::Encryptor* encryptor) {
  DCHECK(encryptor);
  std::optional<std::string> result_opt =
      pref_hash_calculator_.CalculateEncryptedHash(path, dict, encryptor);

  return result_opt.value_or(std::string());
}

// Computes split encrypted hashes.
base::DictValue PrefHashStoreImpl::ComputeSplitEncryptedHashes(
    const std::string& path,
    const base::DictValue* split_values,
    const os_crypt_async::Encryptor* encryptor) {
  if (!encryptor) {
    return base::DictValue();
  }
  if (!split_values || split_values->empty()) {
    return base::DictValue();
  }

  std::string keyed_path(path);
  keyed_path.push_back('.');
  const size_t common_part_length = keyed_path.length();

  base::DictValue split_encrypted_hashes;
  for (const auto [key, split_value] : *split_values) {
    keyed_path.replace(common_part_length, std::string::npos, key);

    std::optional<std::string> result_opt =
        pref_hash_calculator_.CalculateEncryptedHash(keyed_path, &split_value,
                                                     encryptor);

    if (result_opt.has_value()) {
      split_encrypted_hashes.Set(key, std::move(*result_opt));
    }
  }
  return split_encrypted_hashes;
}

// static
void PrefHashStoreImpl::FilterEncryptedHashesRecursive(
    const base::DictValue& src,
    base::DictValue& dest) {
  for (const auto [key, value] : src) {
    bool is_encrypted_key = key.ends_with("_encrypted_hash");

    if (is_encrypted_key) {
      dest.Set(key, value.Clone());
    } else if (value.is_dict()) {
      base::DictValue sub_dest;
      FilterEncryptedHashesRecursive(value.GetDict(), sub_dest);
      if (!sub_dest.empty()) {
        dest.Set(key, std::move(sub_dest));
      }
    }
  }
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::PrefHashStoreTransactionImpl(
    PrefHashStoreImpl* outer,
    HashStoreContents* storage,
    scoped_refptr<const os_crypt_async::Encryptor> encryptor_ptr)
    : outer_(outer), contents_(storage), encryptor_(std::move(encryptor_ptr)) {
  // Super HMAC validation is skipped if the outer store does not use it or if
  // the specific hash store contents implementation does not support it.
  if (outer_->use_super_hmac_ && contents_->SupportsSuperAuthenticator()) {
    std::string super_hmac = contents_->GetSuperHmac();
    if (!super_hmac.empty()) {
      super_hmac_valid_ = outer_->pref_hash_calculator_.ValidateHmac(
                              "", contents_->GetContents(), super_hmac) ==
                          PrefHashCalculator::VALID_HMAC;
    }
  }

  // Load and validate the new Super Encrypted Hash if enabled and the
  // encryptor is available. The flag controls verification.
  std::string super_encrypted_hash;
  // `use_super_encrypted_hash_` controls the loading/verification of the
  // Super Encrypted Hash here. It is disabled for the unprotected store.
  if (outer_->use_super_encrypted_hash_ &&
      contents_->SupportsSuperAuthenticator()) {
    super_encrypted_hash = contents_->GetSuperEncryptedHash();
  }
  if (!super_encrypted_hash.empty() && encryptor_) {
    const base::DictValue* contents = contents_->GetContents();
    if (contents) {
      base::DictValue filtered_dict;
      // Filter out legacy HMACs to compute the hash over encrypted hashes only.
      FilterEncryptedHashesRecursive(*contents, filtered_dict);
      std::string expected_hash =
          outer_->ComputeEncryptedHash("", &filtered_dict, encryptor_.get());
      if (super_encrypted_hash == expected_hash) {
        super_encrypted_hash_valid_ = true;
      } else {
        super_encrypted_hash_mismatch_ = true;
      }
    }
  }

  if (encryptor_) {
    if (super_encrypted_hash.empty()) {
      base::UmaHistogramEnumeration(
          "Settings.TrackedPreferenceSuperEncryptedHashResult",
          SuperEncryptedHashResult::kMissing);
    } else if (super_encrypted_hash_valid_) {
      base::UmaHistogramEnumeration(
          "Settings.TrackedPreferenceSuperEncryptedHashResult",
          SuperEncryptedHashResult::kMatch);
    } else {
      base::UmaHistogramEnumeration(
          "Settings.TrackedPreferenceSuperEncryptedHashResult",
          SuperEncryptedHashResult::kMismatch);
    }
  }
}

PrefHashStoreImpl::PrefHashStoreTransactionImpl::
    ~PrefHashStoreTransactionImpl() {
  if (!contents_->SupportsSuperAuthenticator()) {
    return;
  }

  bool need_super_hmac = super_hmac_dirty_ && outer_->use_super_hmac_;
  bool need_super_encrypted_hash = super_encrypted_hash_dirty_ && encryptor_;

  if (need_super_hmac || need_super_encrypted_hash) {
    // Get the dictionary of hashes (or NULL if it doesn't exist).
    const base::DictValue* hashes_dict = contents_->GetContents();

    if (need_super_hmac) {
      contents_->SetSuperHmac(outer_->ComputeHmac("", hashes_dict));
    }

    if (need_super_encrypted_hash && hashes_dict) {
      base::DictValue filtered_dict;
      FilterEncryptedHashesRecursive(*hashes_dict, filtered_dict);
      if (!filtered_dict.empty()) {
        std::string super_encrypted_hash =
            outer_->ComputeEncryptedHash("", &filtered_dict, encryptor_.get());
        if (!super_encrypted_hash.empty()) {
          contents_->SetSuperEncryptedHash(super_encrypted_hash);
        }
      }
    }
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
  if (contents_->GetAtomicPrefAuthenticator(GetEncryptedHashKey(path),
                                            &encrypted_hash)) {
    return encrypted_hash;
  }
  return std::nullopt;
}

std::optional<std::string>
PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetHmac(
    const std::string& path) const {
  std::string hmac_str;
  // Get the HMAC string from the HashStoreContents.
  if (contents_->GetAtomicPrefAuthenticator(path, &hmac_str)) {
    return hmac_str;
  }
  return std::nullopt;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::GetSplitEncryptedHashes(
    const std::string& path,
    std::map<std::string, std::string>* split_encrypted_hashes) const {
  DCHECK(split_encrypted_hashes);
  split_encrypted_hashes->clear();
  // Use the suffixed key to retrieve split encrypted hashes
  return contents_->GetSplitPrefAuthenticators(GetEncryptedHashKey(path),
                                               split_encrypted_hashes);
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValueInternal(
    const std::string& path,
    const base::Value* value,
    const std::optional<std::string>& stored_encrypted_hash,
    const std::optional<std::string>& stored_hmac,
    std::optional<size_t> reporting_id) const {
  if (encryptor_) {
    // Priority 1: Check encrypted hash.
    if (stored_encrypted_hash.has_value()) {
      const ValidationResult result =
          outer_->pref_hash_calculator_.ValidateEncryptedHash(
              path, value, *stored_encrypted_hash, encryptor_.get());
      if (result == ValidationResult::VALID_ENCRYPTED) {
        return ValueState::UNCHANGED_ENCRYPTED;
      }
      MaybeReportWeakHash(result, reporting_id);
      return value ? ValueState::CHANGED_ENCRYPTED
                   : ValueState::CLEARED_ENCRYPTED;
    }
    // Priority 2: Fallback to legacy HMAC for healing.
    if (!base::FeatureList::IsEnabled(
            tracked::kDisallowLegacyPrefMacFallback)) {
      if (stored_hmac.has_value()) {
        ValidationResult result = outer_->pref_hash_calculator_.ValidateHmac(
            path, value, *stored_hmac);
        if (result == ValidationResult::VALID_HMAC) {
          return ValueState::UNCHANGED_VIA_HMAC_FALLBACK;
        }
        return value ? ValueState::CHANGED_VIA_HMAC_FALLBACK
                     : ValueState::CLEARED_VIA_HMAC_FALLBACK;
      }
    }
  } else {
    // ---- Encryptor is NOT available: Legacy path ----
    if (stored_hmac.has_value()) {
      ValidationResult hmac_validation_result =
          outer_->pref_hash_calculator_.ValidateHmac(path, value, *stored_hmac);
      if (hmac_validation_result == ValidationResult::VALID_HMAC) {
        // If we fell through from encrypted (which was unusable), a valid HMAC
        // still means the value is UNCHANGED.
        return ValueState::UNCHANGED;
      }
      return value ? ValueState::CHANGED : ValueState::CLEARED;
    }
  }

  // --- No Usable Authenticators Found ---
  // Arrive here if:
  // 1. No authenticators stored at all.
  // 2. ONLY encrypted hash stored, but no encryptor (fell through above).
  // 3. Encryptor is present, encrypted hash missing, and legacy fallback
  // disabled.
  if (!value) {
    // Null value is always trusted if no usable authenticator is present
    return ValueState::TRUSTED_NULL_VALUE;
  }

  // If we got here ONLY because an encrypted hash was present but unusable
  // (due to missing encryptor), treat the value as untrusted regardless of
  // the (potentially stale) super_hmac_valid_ flag.
  if (stored_encrypted_hash.has_value() && !stored_hmac.has_value() &&
      !encryptor_) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // If the encryptor is present and fallback is disabled, but a legacy HMAC was
  // present (meaning an old or downgraded pref with no encrypted hash), treat
  // it as untrusted.
  if (encryptor_ && stored_hmac.has_value() &&
      base::FeatureList::IsEnabled(tracked::kDisallowLegacyPrefMacFallback)) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // Otherwise (genuinely no authenticators stored), base trust on the validity
  // state of super hash *cached at the start of the transaction*.
  // If the super encrypted hash was present but failed verification (mismatch),
  // we do not trust the state even if the legacy super HMAC was valid.
  if (super_encrypted_hash_mismatch_) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  bool is_trusted = false;
  if (encryptor_ &&
      base::FeatureList::IsEnabled(tracked::kDisallowLegacyPrefMacFallback)) {
    // When os_crypt is available and legacy fallback is disallowed, trust must
    // be anchored in the Super Encrypted Hash, not the forgeable legacy Super
    // HMAC.
    is_trusted = super_encrypted_hash_valid_;
  } else {
    is_trusted = (super_hmac_valid_ || super_encrypted_hash_valid_);
  }

  return is_trusted ? ValueState::TRUSTED_UNKNOWN_VALUE
                    : ValueState::UNTRUSTED_UNKNOWN_VALUE;
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckValue(
    const std::string& path,
    const base::Value* initial_value,
    std::optional<size_t> reporting_id) const {
  // Attempt to retrieve both types of authenticator.
  std::optional<std::string> encrypted_hash = GetEncryptedHash(path);
  std::optional<std::string> hmac;
  std::string hmac_str;
  if (contents_->GetAtomicPrefAuthenticator(path, &hmac_str)) {
    hmac = hmac_str;
  }

  // Delegate to the internal helper.
  return CheckValueInternal(path, initial_value, encrypted_hash, hmac,
                            reporting_id);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreHmac(
    const std::string& path,
    const base::Value* new_value) {
  const std::string hmac = outer_->ComputeHmac(path, new_value);
  contents_->SetAtomicPrefAuthenticator(path, hmac);
  super_hmac_dirty_ = true;

  // Maintain dual stamping behavior: if the store is being updated,
  // the super encrypted hash should also be updated unconditionally if the
  // encryptor is available.
  if (encryptor_) {
    super_encrypted_hash_dirty_ = true;
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreEncryptedHash(
    const std::string& path,
    const base::Value* value) {
  if (!encryptor_) {
    return;
  }

  const std::string encrypted_hash_str =
      outer_->ComputeEncryptedHash(path, value, encryptor_.get());

  std::string enc_key = GetEncryptedHashKey(path);

  // ComputeEncryptedHash from PrefHashStoreImpl returns "" on failure.
  if (!encrypted_hash_str.empty()) {
    // Calculation and encryption were successful, store it.
    contents_->SetAtomicPrefAuthenticator(enc_key, encrypted_hash_str);
    super_hmac_dirty_ = true;
    super_encrypted_hash_dirty_ = true;
  } else {
    // Computation failed, ensure no (potentially old or empty) hash is stored.
    if (contents_->RemoveAuthenticator(enc_key)) {
      super_hmac_dirty_ = true;
      super_encrypted_hash_dirty_ = true;
    }
  }
}

ValueState
PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValueInternal(
    const std::string& path,
    const base::DictValue* initial_split_value,
    bool has_encrypted_hashes,
    const std::map<std::string, std::string>& split_encrypted_hashes,
    bool has_hmacs,
    const std::map<std::string, std::string>& split_hmacs,
    std::vector<std::string>* invalid_keys,
    std::optional<size_t> reporting_id) const {
  DCHECK(invalid_keys && invalid_keys->empty());

  const bool is_initial_value_empty =
      (!initial_split_value || initial_split_value->empty());
  bool only_unusable_encrypted_present = false;

  if (encryptor_) {
    // --- Encryptor is available ---
    if (has_encrypted_hashes) {
      // --- Priority 1: Check split encrypted hashes ---
      std::map<std::string, std::string> current_encrypted =
          split_encrypted_hashes;
      if (initial_split_value) {
        for (const auto [key, value] : *initial_split_value) {
          auto it = current_encrypted.find(key);
          if (it == current_encrypted.end()) {
            invalid_keys->push_back(key);
          } else {
            const std::string keyed_path = path + "." + key;
            const auto validation_result =
                outer_->pref_hash_calculator_.ValidateEncryptedHash(
                    keyed_path, &value, it->second, encryptor_.get());
            if (validation_result != ValidationResult::VALID_ENCRYPTED) {
              MaybeReportWeakHash(validation_result, reporting_id);
              invalid_keys->push_back(key);
            }
            current_encrypted.erase(it);
          }
        }
      }
      for (const auto& [key, _] : current_encrypted) {
        invalid_keys->push_back(key);
      }

      if (invalid_keys->empty()) {
        return ValueState::UNCHANGED_ENCRYPTED;
      }
      return is_initial_value_empty ? ValueState::CLEARED_ENCRYPTED
                                    : ValueState::CHANGED_ENCRYPTED;
    }

    // --- Priority 2: Fallback to legacy HMACs for healing.
    if (!base::FeatureList::IsEnabled(
            tracked::kDisallowLegacyPrefMacFallback)) {
      if (has_hmacs) {
        std::map<std::string, std::string> current_macs = split_hmacs;
        if (initial_split_value) {
          for (const auto [key, value] : *initial_split_value) {
            const std::string keyed_path = path + "." + key;
            auto it = current_macs.find(key);
            if (it == current_macs.end() ||
                outer_->pref_hash_calculator_.ValidateHmac(keyed_path, &value,
                                                           it->second) !=
                    ValidationResult::VALID_HMAC) {
              invalid_keys->push_back(key);
            }
            if (it != current_macs.end()) {
              current_macs.erase(it);
            }
          }
        }
        for (const auto& [key, _] : current_macs) {
          invalid_keys->push_back(key);
        }

        if (invalid_keys->empty()) {
          return ValueState::UNCHANGED_VIA_HMAC_FALLBACK;
        }
        return is_initial_value_empty ? ValueState::CLEARED_VIA_HMAC_FALLBACK
                                      : ValueState::CHANGED_VIA_HMAC_FALLBACK;
      }
    }
  } else {
    // --- No encryptor, legacy-only path ---
    if (has_hmacs) {
      std::map<std::string, std::string> current_macs = split_hmacs;
      if (initial_split_value) {
        for (const auto [key, value] : *initial_split_value) {
          const std::string keyed_path = path + "." + key;
          auto it = current_macs.find(key);
          if (it == current_macs.end() ||
              outer_->pref_hash_calculator_.ValidateHmac(keyed_path, &value,
                                                         it->second) !=
                  ValidationResult::VALID_HMAC) {
            invalid_keys->push_back(key);
          }
          if (it != current_macs.end()) {
            current_macs.erase(it);
          }
        }
      }
      for (const auto& [key, _] : current_macs) {
        invalid_keys->push_back(key);
      }

      if (invalid_keys->empty()) {
        return ValueState::UNCHANGED;
      }
      return is_initial_value_empty ? ValueState::CLEARED : ValueState::CHANGED;
    }
    if (has_encrypted_hashes && !has_hmacs) {
      only_unusable_encrypted_present = true;
    }
  }

  // --- No Usable Authenticators Found ---
  // Arrive here if:
  // 1. No authenticators stored at all.
  // 2. ONLY encrypted hashes stored, but no encryptor (fell through).
  // 3. Encryptor is present, encrypted hashes missing, and legacy fallback
  // disabled.
  if (is_initial_value_empty) {
    return ValueState::UNCHANGED;
  }

  if (only_unusable_encrypted_present) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // If the encryptor is present and fallback is disabled, but legacy HMACs were
  // present (meaning old or downgraded split prefs with no encrypted hashes),
  // treat them as untrusted.
  if (encryptor_ && has_hmacs &&
      base::FeatureList::IsEnabled(tracked::kDisallowLegacyPrefMacFallback)) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  // Otherwise (genuinely no authenticators at all, or HMACs were checked and
  // failed), base trust on the validity state of super hash *cached at the
  // start of the transaction*.
  // If the super encrypted hash was present but failed verification (mismatch),
  // we do not trust the state even if the legacy super HMAC was valid.
  if (super_encrypted_hash_mismatch_) {
    return ValueState::UNTRUSTED_UNKNOWN_VALUE;
  }

  bool is_trusted = false;
  if (encryptor_ &&
      base::FeatureList::IsEnabled(tracked::kDisallowLegacyPrefMacFallback)) {
    // When os_crypt is available and legacy fallback is disallowed, trust must
    // be anchored in the Super Encrypted Hash, not the forgeable legacy Super
    // HMAC.
    is_trusted = super_encrypted_hash_valid_;
  } else {
    is_trusted = (super_hmac_valid_ || super_encrypted_hash_valid_);
  }

  return is_trusted ? ValueState::TRUSTED_UNKNOWN_VALUE
                    : ValueState::UNTRUSTED_UNKNOWN_VALUE;
}

ValueState PrefHashStoreImpl::PrefHashStoreTransactionImpl::CheckSplitValue(
    const std::string& path,
    const base::DictValue* initial_split_value,
    std::vector<std::string>* invalid_keys,
    std::optional<size_t> reporting_id) const {
  // Attempt to retrieve both types of split authenticators.
  std::map<std::string, std::string> split_encrypted_hashes;
  bool has_encrypted = GetSplitEncryptedHashes(path, &split_encrypted_hashes);

  std::map<std::string, std::string> split_macs;
  bool has_macs = contents_->GetSplitPrefAuthenticators(path, &split_macs);

  return CheckSplitValueInternal(path, initial_split_value, has_encrypted,
                                 split_encrypted_hashes, has_macs, split_macs,
                                 invalid_keys, reporting_id);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitHmac(
    const std::string& path,
    const base::DictValue* split_value) {
  contents_->RemoveAuthenticator(path);

  if (split_value) {
    base::DictValue split_macs = outer_->ComputeSplitHmacs(path, split_value);

    for (const auto [key, value] : split_macs) {
      DCHECK(value.is_string());
      contents_->SetSplitPrefAuthenticator(path, key, value.GetString());
    }
  }
  super_hmac_dirty_ = true;
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::StoreSplitEncryptedHash(
    const std::string& path,
    const base::DictValue* split_value) {
  // Encrypted hash requires the encryptor.
  if (!encryptor_) {
    return;
  }

  // Also remove any existing single *encrypted hash* entry for the base path
  contents_->RemoveAuthenticator(GetEncryptedHashKey(path));

  // Use the derived key for storing split encrypted hashes.
  const std::string encrypted_hash_base_key = GetEncryptedHashKey(path);

  if (split_value) {
    base::DictValue split_encrypted_hashes =
        outer_->ComputeSplitEncryptedHashes(path, split_value,
                                            encryptor_.get());

    for (const auto [key, split_encrypted_hash] : split_encrypted_hashes) {
      DCHECK(split_encrypted_hash.is_string());
      // Store using the derived base key.
      contents_->SetSplitPrefAuthenticator(encrypted_hash_base_key, key,
                                           split_encrypted_hash.GetString());
    }
  }
  super_hmac_dirty_ = true;
  super_encrypted_hash_dirty_ = true;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasAuthenticator(
    const std::string& path) const {
  std::string out_value;
  std::map<std::string, std::string> out_values;
  return HasEncryptedHash(path) ||
         contents_->GetAtomicPrefAuthenticator(path, &out_value) ||
         contents_->GetSplitPrefAuthenticators(path, &out_values);
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::HasEncryptedHash(
    const std::string& path) const {
  std::string out_value;
  const std::string encrypted_key = GetEncryptedHashKey(path);
  std::map<std::string, std::string> out_values;
  return contents_->GetAtomicPrefAuthenticator(encrypted_key, &out_value) ||
         contents_->GetSplitPrefAuthenticators(encrypted_key, &out_values);
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ImportAuthData(
    const std::string& path,
    const base::Value* auth_data) {
  DCHECK(auth_data);
  bool changed = false;

  if (auth_data->is_string()) {
    // --- Case 1: Input is a string ---
    // Legacy HMAC. Import it and clear any existing encrypted hash.
    contents_->ImportAuthenticator(path, auth_data);
    if (contents_->RemoveAuthenticator(GetEncryptedHashKey(path))) {
      changed = true;
      super_encrypted_hash_dirty_ = true;
    }
    // ImportAuthenticator itself implies a change, so mark dirty regardless of
    // RemoveAuthenticator result.
    changed = true;

  } else if (auth_data->is_dict()) {
    // --- Case 2: Input is a dict ---
    const base::DictValue& dict = auth_data->GetDict();

    // Handle HMAC part
    const std::string* hmac_str_ptr = dict.FindString(kImportHmacKey);
    if (hmac_str_ptr) {
      // Import the HMAC if found in the dictionary
      base::Value hmac_value(*hmac_str_ptr);
      contents_->ImportAuthenticator(path, &hmac_value);
      changed = true;
    } else {
      // If "mac" key is NOT in the dictionary, clear any existing HMAC for this
      // path.
      if (contents_->RemoveAuthenticator(path)) {
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
      contents_->ImportAuthenticator(GetEncryptedHashKey(path),
                                     &encrypted_hash_value);
      changed = true;
      if (outer_->use_super_encrypted_hash_) {
        super_encrypted_hash_dirty_ = true;
      }
    } else {
      // If "encrypted_hash" key is NOT in the dictionary, clear any existing
      // encrypted hash for this path (using the derived key).
      if (contents_->RemoveAuthenticator(GetEncryptedHashKey(path))) {
        changed = true;
        if (outer_->use_super_encrypted_hash_) {
          super_encrypted_hash_dirty_ = true;
        }
      }
    }

  } else {
    return;
  }

  // If any import or removal happened and the store was considered valid, mark
  // super HMAC as dirty.
  if (changed && super_hmac_valid_) {
    super_hmac_dirty_ = true;
  } else if (auth_data->is_string() || auth_data->is_dict()) {
    if (super_hmac_valid_) {
      super_hmac_dirty_ = true;
    }
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearAuthenticators(
    const std::string& path) {
  bool changed = false;
  std::string enc_key = GetEncryptedHashKey(path);  // Get derived key once

  // Remove atomic HMAC entry OR split HMAC dictionary at 'path'
  if (contents_->RemoveAuthenticator(path)) {
    changed = true;
  }

  // Remove atomic Encrypted Hash entry OR split encrypted hash dictionary at
  // derived key
  if (contents_->RemoveAuthenticator(enc_key)) {
    changed = true;
    super_encrypted_hash_dirty_ = true;
  }

  // Mark Super HMAC dirty only if something was actually removed AND if the
  // Super HMAC was considered valid at the start of the transaction.
  if (changed && super_hmac_valid_) {
    super_hmac_dirty_ = true;
  }
}

void PrefHashStoreImpl::PrefHashStoreTransactionImpl::ClearEncryptedHash(
    const std::string& path) {
  // Clear only the Encrypted Hash (atomic and split) using the derived key.
  if (contents_->RemoveAuthenticator(GetEncryptedHashKey(path)) &&
      super_hmac_valid_) {
    super_hmac_dirty_ = true;
  }
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::IsSuperHmacValid() const {
  return super_hmac_valid_;
}

bool PrefHashStoreImpl::PrefHashStoreTransactionImpl::StampSuperHmac() {
  if (!outer_->use_super_hmac_) {
    return false;
  }
  super_hmac_dirty_ = true;
  super_hmac_valid_ = true;
  return true;
}
