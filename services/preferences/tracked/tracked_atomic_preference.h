// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "services/preferences/tracked/pref_hash_filter.h"
#include "services/preferences/tracked/tracked_preference.h"
#include "services/preferences/tracked/tracked_preference_helper.h"

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}

// A TrackedAtomicPreference is tracked as a whole. A hash is stored for its
// entire value and it is entirely reset on mismatch. An optional delegate is
// notified of the status of the preference during enforcement.
class TrackedAtomicPreference : public TrackedPreference {
 public:
  TrackedAtomicPreference(
      const std::string& pref_path,
      size_t reporting_id,
      size_t reporting_ids_count,
      prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel
          enforcement_level,
      prefs::mojom::TrackedPreferenceMetadata::ValueType value_type,
      prefs::mojom::TrackedPreferenceValidationDelegate* delegate);

  // TrackedPreference implementation.
  TrackedPreferenceType GetType() const override;
  void OnNewValue(const base::Value* value,
                  PrefHashStoreTransaction* transaction) const override;
  bool EnforceAndReport(
      base::DictionaryValue* pref_store_contents,
      PrefHashStoreTransaction* transaction,
      PrefHashStoreTransaction* external_validation_transaction) const override;

 private:
  const std::string pref_path_;
  const TrackedPreferenceHelper helper_;
  prefs::mojom::TrackedPreferenceValidationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrackedAtomicPreference);
};

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_ATOMIC_PREFERENCE_H_
