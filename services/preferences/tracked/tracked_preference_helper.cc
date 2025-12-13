// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_preference_helper.h"

#include "base/check.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"

using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

TrackedPreferenceHelper::TrackedPreferenceHelper(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel enforcement_level,
    prefs::mojom::TrackedPreferenceMetadata::ValueType value_type)
    : pref_path_(pref_path),
      reporting_id_(reporting_id),
      reporting_ids_count_(reporting_ids_count),
      enforce_(enforcement_level == prefs::mojom::TrackedPreferenceMetadata::
                                        EnforcementLevel::ENFORCE_ON_LOAD),
      personal_(value_type ==
                prefs::mojom::TrackedPreferenceMetadata::ValueType::PERSONAL) {}

TrackedPreferenceHelper::ResetAction TrackedPreferenceHelper::GetAction(
    ValueState value_state) const {
  switch (value_state) {
    case ValueState::UNCHANGED:
    case ValueState::UNCHANGED_ENCRYPTED:
    case ValueState::UNCHANGED_VIA_HMAC_FALLBACK:
      // Desired case, nothing to do.
      return DONT_RESET;
    case ValueState::CLEARED:
    case ValueState::CLEARED_ENCRYPTED:
    case ValueState::CLEARED_VIA_HMAC_FALLBACK:
      // Unfortunate case, but there is nothing we can do.
      return DONT_RESET;
    case ValueState::TRUSTED_NULL_VALUE:  // Falls through.
    case ValueState::TRUSTED_UNKNOWN_VALUE:
      // It is okay to seed the hash in this case.
      return DONT_RESET;
    case ValueState::UNSUPPORTED:
      NOTREACHED()
          << "GetAction should not be called with an UNSUPPORTED value state";
    case ValueState::UNTRUSTED_UNKNOWN_VALUE:  // Falls through.
    case ValueState::CHANGED:
      return enforce_ ? DO_RESET : WANTED_RESET;
    case ValueState::CHANGED_ENCRYPTED:
      return enforce_ ? DO_RESET_ENCRYPTED : WANTED_RESET_ENCRYPTED;
    case ValueState::CHANGED_VIA_HMAC_FALLBACK:
      return enforce_ ? DO_RESET_LEGACY : WANTED_RESET_LEGACY;
  }
  NOTREACHED() << "Unexpected ValueState: " << value_state;
}

bool TrackedPreferenceHelper::IsPersonal() const {
  return personal_;
}

void TrackedPreferenceHelper::ReportValidationResult(
    ValueState value_state,
    std::string_view validation_type_suffix) const {
  const char* histogram_name = nullptr;
  switch (value_state) {
    case ValueState::UNCHANGED:
      histogram_name = user_prefs::tracked::kTrackedPrefHistogramUnchanged;
      break;
    case ValueState::CLEARED:
      histogram_name = user_prefs::tracked::kTrackedPrefHistogramCleared;
      break;
    case ValueState::CHANGED:
      histogram_name = user_prefs::tracked::kTrackedPrefHistogramChanged;
      break;
    case ValueState::UNCHANGED_ENCRYPTED:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramUnchangedEncrypted;
      break;
    case ValueState::CLEARED_ENCRYPTED:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramClearedEncrypted;
      break;
    case ValueState::CHANGED_ENCRYPTED:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramChangedEncrypted;
      break;
    case ValueState::UNCHANGED_VIA_HMAC_FALLBACK:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramUnchangedViaHmacFallback;
      break;
    case ValueState::CLEARED_VIA_HMAC_FALLBACK:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramClearedViaHmacFallback;
      break;
    case ValueState::CHANGED_VIA_HMAC_FALLBACK:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramChangedViaHmacFallback;
      break;
    case ValueState::UNTRUSTED_UNKNOWN_VALUE:
      histogram_name = user_prefs::tracked::kTrackedPrefHistogramInitialized;
      break;
    case ValueState::TRUSTED_UNKNOWN_VALUE:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized;
      break;
    case ValueState::TRUSTED_NULL_VALUE:
      histogram_name =
          user_prefs::tracked::kTrackedPrefHistogramNullInitialized;
      break;
    case ValueState::UNSUPPORTED:
      NOTREACHED() << "ReportValidationResult should not be called with an "
                      "UNSUPPORTED value state";
  }
  DCHECK(histogram_name);

  std::string full_histogram_name(histogram_name);
  if (!validation_type_suffix.empty()) {
    base::StrAppend(&full_histogram_name, {".", validation_type_suffix});
  }

  // Using FactoryGet to allow dynamic histogram names. This is equivalent to
  // UMA_HISTOGRAM_ENUMERATION(name, reporting_id_, reporting_ids_count_);
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      full_histogram_name, 1, reporting_ids_count_, reporting_ids_count_ + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(reporting_id_);
}

void TrackedPreferenceHelper::ReportAction(ResetAction reset_action) const {
  switch (reset_action) {
    case DONT_RESET:
      // No report for DONT_RESET.
      break;
    case WANTED_RESET:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramWantedReset, reporting_id_,
          reporting_ids_count_);
      break;
    case WANTED_RESET_LEGACY:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramWantedResetViaHmacFallback,
          reporting_id_, reporting_ids_count_);
      break;
    case WANTED_RESET_ENCRYPTED:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramWantedResetEncrypted,
          reporting_id_, reporting_ids_count_);
      break;
    case DO_RESET:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramReset, reporting_id_,
          reporting_ids_count_);
      break;
    case DO_RESET_LEGACY:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramResetViaHmacFallback,
          reporting_id_, reporting_ids_count_);
      break;
    case DO_RESET_ENCRYPTED:
      UMA_HISTOGRAM_EXACT_LINEAR(
          user_prefs::tracked::kTrackedPrefHistogramResetEncrypted,
          reporting_id_, reporting_ids_count_);
      break;
  }
}

size_t TrackedPreferenceHelper::GetReportingId() const {
  return reporting_id_;
}
