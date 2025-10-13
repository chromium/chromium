// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/features.h"

#import "components/autofill/core/common/autofill_prefs.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/history/core/common/pref_names.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Returns true if an import is blocked by a "disabling" policy.
bool IsImportBlockedByDisablingPolicy(const PrefService* pref_service,
                                      const char* pref_name) {
  return pref_service->IsManagedPreference(pref_name) &&
         pref_service->GetBoolean(pref_name);
}

// Returns true if an import is blocked by an "enabling" policy.
bool IsImportBlockedByEnablingPolicy(const PrefService* pref_service,
                                     const char* pref_name) {
  return pref_service->IsManagedPreference(pref_name) &&
         !pref_service->GetBoolean(pref_name);
}

}  // namespace

bool ShouldShowSafariDataImportEntryPoint(PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }

  // Safari export is not available on iOS versions earlier than 18.2.
  if (@available(iOS 18.2, *)) {
    if (!base::FeatureList::IsEnabled(kImportPasswordsFromSafari)) {
      return false;
    }
  } else {
    return false;
  }

  bool passwords_blocked = IsImportBlockedByEnablingPolicy(
      pref_service, password_manager::prefs::kCredentialsEnableService);

  bool payments_blocked = IsImportBlockedByEnablingPolicy(
      pref_service, autofill::prefs::kAutofillCreditCardEnabled);

  bool history_blocked = IsImportBlockedByDisablingPolicy(
      pref_service, prefs::kSavingBrowserHistoryDisabled);

  bool bookmarks_blocked = IsImportBlockedByEnablingPolicy(
      pref_service, bookmarks::prefs::kEditBookmarksEnabled);

  if (passwords_blocked && payments_blocked && history_blocked &&
      bookmarks_blocked) {
    return false;
  }

  return true;
}
