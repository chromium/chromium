// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_SPLIT_PREFERENCE_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_SPLIT_PREFERENCE_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "services/preferences/tracked/pref_hash_filter.h"
#include "services/preferences/tracked/tracked_preference.h"
#include "services/preferences/tracked/tracked_preference_helper.h"

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}

// A TrackedSplitPreference must be tracking a dictionary pref. Each top-level
// entry in its dictionary is tracked and enforced independently. An optional
// delegate is notified of the status of the preference during enforcement.
class TrackedSplitPreference : public TrackedPreference {
 public:
  // Constructs a TrackedSplitPreference. |pref_path| must be a dictionary pref.
  TrackedSplitPreference(
      const std::string& pref_path,
      size_t reporting_id,
      size_t reporting_ids_count,
      prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel
          enforcement_level,
      prefs::mojom::TrackedPreferenceMetadata::ValueType value_type,
      prefs::mojom::TrackedPreferenceValidationDelegate* delegate);

  TrackedSplitPreference(const TrackedSplitPreference&) = delete;
  TrackedSplitPreference& operator=(const TrackedSplitPreference&) = delete;

  // TrackedPreference implementation.
  TrackedPreferenceType GetType() const override;
  void OnNewValue(const base::Value* value,
                  PrefHashStoreTransaction* transaction) const override;
  bool EnforceAndReport(
      base::Value::Dict& pref_store_contents,
      PrefHashStoreTransaction* transaction,
      PrefHashStoreTransaction* external_validation_transaction) const override;

 private:
  const std::string pref_path_;
  const TrackedPreferenceHelper helper_;
  raw_ptr<prefs::mojom::TrackedPreferenceValidationDelegate> delegate_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_SPLIT_PREFERENCE_H_
