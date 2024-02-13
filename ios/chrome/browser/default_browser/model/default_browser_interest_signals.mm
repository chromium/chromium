// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"

#import "ios/chrome/browser/default_browser/model/utils.h"

namespace default_browser {

void NotifyStartWithWidget() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyStartWithURL() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
}

void NotifyCredentialExtensionUsed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyAutofillSuggestionsShown() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

void NotifyPasswordAutofillSuggestionUsed() {
  // TODO(b/322358517): Migrate to FET.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

void NotifyPasswordSavedOrUpdated() {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  LogAutofillUseForCriteriaExperiment();
}

}  // namespace default_browser
