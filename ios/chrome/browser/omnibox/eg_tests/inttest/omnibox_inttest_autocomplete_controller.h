// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_AUTOCOMPLETE_CONTROLLER_H_

#import "components/omnibox/browser/autocomplete_controller.h"

class FakeAutocompleteProvider;
class FakeSuggestionsBuilder;

/// `AutocompleteController` used in integration tests, results are stubbed in
/// the `Start` method.
class OmniboxInttestAutocompleteController : public AutocompleteController {
 public:
  OmniboxInttestAutocompleteController();
  OmniboxInttestAutocompleteController(
      const OmniboxInttestAutocompleteController&) = delete;
  OmniboxInttestAutocompleteController& operator=(
      const OmniboxInttestAutocompleteController&) = delete;
  ~OmniboxInttestAutocompleteController() override;

  void Start(const AutocompleteInput& input) override;

  bool& fake_suggestion_enabled() { return fake_suggestion_enabled_; }

  FakeSuggestionsBuilder* fake_suggestions_builder() const {
    return suggestions_builder_.get();
  }

 private:
  scoped_refptr<FakeAutocompleteProvider> provider_;
  std::unique_ptr<FakeSuggestionsBuilder> suggestions_builder_;
  /// Whether suggestions are stubbed with fake suggestions.
  bool fake_suggestion_enabled_ = false;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_OMNIBOX_INTTEST_AUTOCOMPLETE_CONTROLLER_H_
