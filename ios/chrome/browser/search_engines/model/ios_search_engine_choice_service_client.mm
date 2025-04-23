// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/ios_search_engine_choice_service_client.h"

#import "components/country_codes/country_codes.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

country_codes::CountryId
IOSSearchEngineChoiceServiceClient::GetVariationsCountry() {
  return GetVariationsLatestCountry(
      GetApplicationContext()->GetVariationsService());
}

bool IOSSearchEngineChoiceServiceClient::
    IsProfileEligibleForDseGuestPropagation() {
  return false;  // Not eligible since iOS has no Guest profile support.
}
