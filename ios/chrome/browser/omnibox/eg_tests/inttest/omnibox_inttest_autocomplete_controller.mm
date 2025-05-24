// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_autocomplete_controller.h"

#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/fake_autocomplete_provider.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/fake_suggestions_builder.h"

OmniboxInttestAutocompleteController::OmniboxInttestAutocompleteController()
    : AutocompleteController(
          std::make_unique<FakeAutocompleteProviderClient>(),
          AutocompleteClassifier::DefaultOmniboxProviders()) {
  provider_ = new FakeAutocompleteProvider(AutocompleteProvider::TYPE_BUILTIN);
  suggestions_builder_ = std::make_unique<FakeSuggestionsBuilder>();
}

OmniboxInttestAutocompleteController::~OmniboxInttestAutocompleteController() {
  provider_ = nullptr;
  suggestions_builder_ = nullptr;
}

void OmniboxInttestAutocompleteController::Start(
    const AutocompleteInput& input) {
  {
    if (!fake_suggestion_enabled_) {
      AutocompleteController::Start(input);
      return;
    }
    AutocompleteResult& results = internal_result_;
    results.ClearMatches();
    results.AppendMatches(
        suggestions_builder_->BuildSuggestionsForInput(input, provider_.get()));

    notify_changed_default_match_ = true;
    NotifyChanged();
  }
}
