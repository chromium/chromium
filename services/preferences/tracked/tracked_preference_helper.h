// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_HELPER_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_HELPER_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "services/preferences/tracked/pref_hash_filter.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

// A TrackedPreferenceHelper is a helper class for TrackedPreference which
// handles decision making and reporting for TrackedPreference's
// implementations.
class TrackedPreferenceHelper {
 public:
  enum ResetAction {
    DONT_RESET,
    // WANTED_RESET is reported when DO_RESET would have been reported but the
    // current |enforcement_level| doesn't allow a reset for the detected state.
    WANTED_RESET,
    DO_RESET,
  };

  TrackedPreferenceHelper(
      const std::string& pref_path,
      size_t reporting_id,
      size_t reporting_ids_count,
      prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel
          enforcement_level,
      prefs::mojom::TrackedPreferenceMetadata::ValueType value_type);

  TrackedPreferenceHelper(const TrackedPreferenceHelper&) = delete;
  TrackedPreferenceHelper& operator=(const TrackedPreferenceHelper&) = delete;

  // Returns a ResetAction stating whether a reset is desired (DO_RESET) or not
  // (DONT_RESET) based on observing |value_state|. Can also return WANTED_RESET
  // if a reset would have been desired but the current |enforcement_level|
  // doesn't allow it.
  ResetAction GetAction(
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state)
      const;

  // Returns true if the preference value may contain personal information.
  bool IsPersonal() const;

  // Reports |value_state| via UMA under |reporting_id_|.
  // |validation_type_suffix| is appended to the reported histogram's name.
  void ReportValidationResult(
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
      std::string_view validation_type_suffix) const;

  // Reports |reset_action| via UMA under |reporting_id_|.
  void ReportAction(ResetAction reset_action) const;

 private:
  const std::string pref_path_;

  const size_t reporting_id_;
  const size_t reporting_ids_count_;

  // Deny setting changes and hash seeding/migration.
  const bool enforce_;

  const bool personal_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_HELPER_H_
