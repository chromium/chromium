// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_split_preference.h"

#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
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

void TrackedSplitPreference::OnNewValue(
    const base::Value* value,
    PrefHashStoreTransaction* transaction) const {
  if (value && !value->is_dict()) {
    NOTREACHED();
    return;
  }
  const base::DictionaryValue* dict_value =
      value ? &base::Value::AsDictionaryValue(*value) : nullptr;
  transaction->StoreSplitHash(pref_path_, dict_value);
}

bool TrackedSplitPreference::EnforceAndReport(
    base::DictionaryValue* pref_store_contents,
    PrefHashStoreTransaction* transaction,
    PrefHashStoreTransaction* external_validation_transaction) const {
  base::DictionaryValue* dict_value = nullptr;
  if (!pref_store_contents->GetDictionary(pref_path_, &dict_value) &&
      pref_store_contents->FindPath(pref_path_)) {
    // There should be a dictionary or nothing at |pref_path_|.
    NOTREACHED();
    return false;
  }

  std::vector<std::string> invalid_keys;
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

  bool was_reset = false;
  if (reset_action == TrackedPreferenceHelper::DO_RESET) {
    if (value_state == ValueState::CHANGED) {
      DCHECK(!invalid_keys.empty());

      for (std::vector<std::string>::const_iterator it = invalid_keys.begin();
           it != invalid_keys.end(); ++it) {
        dict_value->RemoveKey(*it);
      }
    } else {
      pref_store_contents->RemovePath(pref_path_);
    }
    was_reset = true;
  }

  if (value_state != ValueState::UNCHANGED) {
    // Store the hash for the new value (whether it was reset or not).
    const base::DictionaryValue* new_dict_value = NULL;
    pref_store_contents->GetDictionary(pref_path_, &new_dict_value);
    transaction->StoreSplitHash(pref_path_, new_dict_value);
  }

  // Update MACs in the external store if there is one and there either was a
  // reset or external validation failed.
  if (external_validation_transaction &&
      (was_reset || external_validation_value_state != ValueState::UNCHANGED)) {
    const base::DictionaryValue* new_dict_value = nullptr;
    pref_store_contents->GetDictionary(pref_path_, &new_dict_value);
    external_validation_transaction->StoreSplitHash(pref_path_, new_dict_value);
  }

  return was_reset;
}
