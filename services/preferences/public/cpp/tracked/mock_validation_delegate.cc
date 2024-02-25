// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/tracked/mock_validation_delegate.h"

MockValidationDelegateRecord::MockValidationDelegateRecord() = default;

MockValidationDelegateRecord::~MockValidationDelegateRecord() = default;

size_t MockValidationDelegateRecord::CountValidationsOfState(
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state)
    const {
  size_t count = 0;
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].value_state == value_state)
      ++count;
  }
  return count;
}

size_t MockValidationDelegateRecord::CountExternalValidationsOfState(
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state)
    const {
  size_t count = 0;
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].external_validation_value_state == value_state)
      ++count;
  }
  return count;
}

const MockValidationDelegateRecord::ValidationEvent*
MockValidationDelegateRecord::GetEventForPath(
    const std::string& pref_path) const {
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].pref_path == pref_path)
      return &validations_[i];
  }
  return NULL;
}

void MockValidationDelegateRecord::RecordValidation(
    const std::string& pref_path,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
        external_validation_value_state,
    bool is_personal,
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy strategy) {
  validations_.push_back(ValidationEvent(pref_path, value_state,
                                         external_validation_value_state,
                                         is_personal, strategy));
}

MockValidationDelegate::MockValidationDelegate(
    scoped_refptr<MockValidationDelegateRecord> record)
    : record_(std::move(record)) {}

MockValidationDelegate::~MockValidationDelegate() = default;

void MockValidationDelegate::OnAtomicPreferenceValidation(
    const std::string& pref_path,
    std::optional<base::Value> value,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
        external_validation_value_state,
    bool is_personal) {
  record_->RecordValidation(
      pref_path, value_state, external_validation_value_state, is_personal,
      prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy::ATOMIC);
}

void MockValidationDelegate::OnSplitPreferenceValidation(
    const std::string& pref_path,
    const std::vector<std::string>& invalid_keys,
    const std::vector<std::string>& external_validation_invalid_keys,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
        external_validation_value_state,
    bool is_personal) {
  record_->RecordValidation(
      pref_path, value_state, external_validation_value_state, is_personal,
      prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy::SPLIT);
}
