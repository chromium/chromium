// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_atomic_preference.h"

#include "base/values.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

TrackedAtomicPreference::TrackedAtomicPreference(
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

TrackedPreferenceType TrackedAtomicPreference::GetType() const {
  return TrackedPreferenceType::ATOMIC;
}

void TrackedAtomicPreference::OnNewValue(
    const base::Value* value,
    PrefHashStoreTransaction* transaction) const {
  transaction->StoreHash(pref_path_, value);
}

bool TrackedAtomicPreference::EnforceAndReport(
    base::Value::Dict& pref_store_contents,
    PrefHashStoreTransaction* transaction,
    PrefHashStoreTransaction* external_validation_transaction) const {
  const base::Value* value = pref_store_contents.FindByDottedPath(pref_path_);
  ValueState value_state = transaction->CheckValue(pref_path_, value);
  helper_.ReportValidationResult(value_state, transaction->GetStoreUMASuffix());

  ValueState external_validation_value_state = ValueState::UNSUPPORTED;
  if (external_validation_transaction) {
    external_validation_value_state =
        external_validation_transaction->CheckValue(pref_path_, value);
    helper_.ReportValidationResult(
        external_validation_value_state,
        external_validation_transaction->GetStoreUMASuffix());
  }

  if (delegate_) {
    delegate_->OnAtomicPreferenceValidation(
        pref_path_, value ? std::make_optional(value->Clone()) : std::nullopt,
        value_state, external_validation_value_state, helper_.IsPersonal());
  }
  TrackedPreferenceHelper::ResetAction reset_action =
      helper_.GetAction(value_state);
  helper_.ReportAction(reset_action);

  bool was_reset = false;
  if (reset_action == TrackedPreferenceHelper::DO_RESET) {
    pref_store_contents.RemoveByDottedPath(pref_path_);
    was_reset = true;
  }

  if (value_state != ValueState::UNCHANGED) {
    // Store the hash for the new value (whether it was reset or not).
    const base::Value* new_value =
        pref_store_contents.FindByDottedPath(pref_path_);
    transaction->StoreHash(pref_path_, new_value);
  }

  // Update MACs in the external store if there is one and there either was a
  // reset or external validation failed.
  if (external_validation_transaction &&
      (was_reset || external_validation_value_state != ValueState::UNCHANGED)) {
    const base::Value* new_value =
        pref_store_contents.FindByDottedPath(pref_path_);
    external_validation_transaction->StoreHash(pref_path_, new_value);
  }

  return was_reset;
}
