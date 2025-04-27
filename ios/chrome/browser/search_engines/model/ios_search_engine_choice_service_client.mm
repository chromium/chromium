// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/ios_search_engine_choice_service_client.h"

#import "components/country_codes/country_codes.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

country_codes::CountryId
IOSSearchEngineChoiceServiceClient::GetVariationsCountry() {
  return GetVariationsLatestCountry(
      GetApplicationContext()->GetVariationsService());
}

bool IOSSearchEngineChoiceServiceClient::
    IsProfileEligibleForDseGuestPropagation() {
  return false;  // Not eligible since iOS has no Guest profile support.
}

bool IOSSearchEngineChoiceServiceClient::
    IsDeviceRestoreDetectedInCurrentSession() {
  return IsFirstSessionAfterDeviceRestore() == signin::Tribool::kTrue;
}

bool IOSSearchEngineChoiceServiceClient::DoesChoicePredateDeviceRestore(
    const search_engines::ChoiceCompletionMetadata& choice_metadata) {
  if (IsDeviceRestoreDetectedInCurrentSession()) {
    // LastDeviceRestoreTimestamp() returns no date on the current session of
    // restore.
    // TODO(crbug.com/413368496): This can be removed once crbug.com/413368496
    // is fixed. `IsDeviceRestoreDetectedInCurrentSession()` should return a
    // timestamp even during the first session after the restore.
    return true;
  }
  std::optional<base::Time> last_restore_date = LastDeviceRestoreTimestamp();
  return last_restore_date.has_value() &&
         (choice_metadata.timestamp < last_restore_date.value());
}
