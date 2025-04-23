// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_IOS_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_IOS_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_

#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"

// Provides access to iOS-specific state and utilities to
// `SearchEngineChoiceService`.
class IOSSearchEngineChoiceServiceClient
    : public search_engines::SearchEngineChoiceService::Client {
 public:
  // `SearchEngineChoiceService::Client` implementation.
  country_codes::CountryId GetVariationsCountry() override;
  bool IsProfileEligibleForDseGuestPropagation() override;
  bool IsDeviceRestoreDetectedInCurrentSession() override;
  bool DoesChoicePredateDeviceRestore(
      const search_engines::ChoiceCompletionMetadata& choice_metadata) override;
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_IOS_SEARCH_ENGINE_CHOICE_SERVICE_CLIENT_H_
