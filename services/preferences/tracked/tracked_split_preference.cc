// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_split_preference.h"

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/values.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

TrackedSplitPreference::TrackedSplitPreference(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel enforcement_level,
    prefs::mojom::TrackedPreferenceMetadata::ValueType value_type,
    prefs::mojom::TrackedPreferenceValidationDelegate* delegate)
    : pref_path_(pref_path),
      helper_(pref_path,
              reporting_id,
              reporting_ids_count,
              enforcement_level,
              value_type),
      delegate_(delegate) {}

TrackedPreferenceType TrackedSplitPreference::GetType() const {
  return TrackedPreferenceType::SPLIT;
}

size_t TrackedSplitPreference::GetReportingId() const {
  return helper_.GetReportingId();
}

void TrackedSplitPreference::OnNewValue(
    const base::Value* value,
    PrefHashStoreTransaction* transaction,
    const os_crypt_async::Encryptor* encryptor) const {
  if (value && !value->is_dict()) {
    NOTREACHED();
  }
  transaction->StoreSplitHash(pref_path_, value ? &value->GetDict() : nullptr);

  if (encryptor) {
    transaction->StoreSplitEncryptedHash(pref_path_,
                                         value ? &value->GetDict() : nullptr);
  }
}

bool TrackedSplitPreference::EnforceAndReport(
    base::Value::Dict& pref_store_contents,
    PrefHashStoreTransaction* transaction,
    PrefHashStoreTransaction* external_validation_transaction,
    const os_crypt_async::Encryptor* encryptor) const {
  bool was_reset = false;
  base::Value* value = pref_store_contents.FindByDottedPath(pref_path_);
  if (value && !value->is_dict()) {
    // There should be a dictionary or nothing at |pref_path_|. If we have a
    // non-dictionary here, reset it as it's an unexpected type. Then treat it
    // as if it was never present. See https://crbug.com/1512724.
    CHECK(pref_store_contents.RemoveByDottedPath(pref_path_));
    was_reset = true;
    value = nullptr;
  }

  base::Value::Dict* dict_value = value ? &value->GetDict() : nullptr;

  std::vector<std::string> invalid_keys;
  // TODO(zackhan@): Currently this function support dual-hash validation.
  // Revisit and double check this function later on when the feature is fully
  // rolled out and the hmac based validation is removed.
  // transaction->CheckValue() (from CL1) is dual-hash aware and uses the
  // encryptor with which `transaction` was initialized by PrefHashFilter.
  ValueState value_state =
      transaction->CheckSplitValue(pref_path_, dict_value, &invalid_keys);

  helper_.ReportValidationResult(value_state, transaction->GetStoreUMASuffix());

  ValueState external_validation_value_state = ValueState::UNSUPPORTED;
  std::vector<std::string> external_validation_invalid_keys;
  if (external_validation_transaction) {
    external_validation_value_state =
        external_validation_transaction->CheckSplitValue(
            pref_path_, dict_value, &external_validation_invalid_keys);
    helper_.ReportValidationResult(
        external_validation_value_state,
        external_validation_transaction->GetStoreUMASuffix());
  }

  if (delegate_) {
    delegate_->OnSplitPreferenceValidation(
        pref_path_, invalid_keys, external_validation_invalid_keys, value_state,
        external_validation_value_state, helper_.IsPersonal());
  }
  TrackedPreferenceHelper::ResetAction reset_action =
      helper_.GetAction(value_state);
  helper_.ReportAction(reset_action);

  if (reset_action == TrackedPreferenceHelper::DO_RESET ||
      reset_action == TrackedPreferenceHelper::DO_RESET_LEGACY ||
      reset_action == TrackedPreferenceHelper::DO_RESET_ENCRYPTED) {
    base::Value::List* reset_prefs_list =
        pref_store_contents.EnsureList(user_prefs::kTrackedPreferencesReset);
    if (value_state == ValueState::CHANGED ||
        value_state == ValueState::CHANGED_VIA_HMAC_FALLBACK ||
        value_state == ValueState::CHANGED_ENCRYPTED) {
      DCHECK(!invalid_keys.empty());

      // `dict_value` can be null here. This happens when the entire preference
      // dictionary is missing from the pref store, but a hash for it still
      // exists in the hash store. As a result of the inconsistency, the
      // function reports this as `CHANGED`. This check prevents a crash when
      // attempting to reset keys on a non-existent dictionary.
      if (dict_value) {
        for (const std::string& key : invalid_keys) {
          base::Value new_path(pref_path_ + "." + key);
          if (!base::Contains(*reset_prefs_list, new_path)) {
            reset_prefs_list->Append(std::move(new_path));
          }
          dict_value->Remove(key);
        }
      }
    } else {
      if (value) {
        base::Value new_path(pref_path_);
        if (!base::Contains(*reset_prefs_list, new_path)) {
          reset_prefs_list->Append(std::move(new_path));
        }
      }
      pref_store_contents.RemoveByDottedPath(pref_path_);
    }
    was_reset = true;
  }

  if (value_state != ValueState::UNCHANGED &&
      value_state != ValueState::UNCHANGED_ENCRYPTED) {
    // Store the hash for the new value (whether it was reset or not).
    transaction->StoreSplitHash(
        pref_path_, pref_store_contents.FindDictByDottedPath(pref_path_));

    if (encryptor) {
      transaction->StoreSplitEncryptedHash(
          pref_path_, pref_store_contents.FindDictByDottedPath(pref_path_));
    }
  }

  // Update MACs in the external store if there is one and there either was a
  // reset or external validation failed.
  if (external_validation_transaction &&
      (was_reset || external_validation_value_state != ValueState::UNCHANGED)) {
    external_validation_transaction->StoreSplitHash(
        pref_path_, pref_store_contents.FindDictByDottedPath(pref_path_));
    if (encryptor) {
      external_validation_transaction->StoreSplitEncryptedHash(
          pref_path_, pref_store_contents.FindDictByDottedPath(pref_path_));
    }
  }

  return was_reset;
}
