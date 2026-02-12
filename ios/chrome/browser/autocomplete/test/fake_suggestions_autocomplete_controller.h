// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_AUTOCOMPLETE_CONTROLLER_H_

#import <memory>

#import "components/omnibox/browser/autocomplete_controller.h"

class FakeAutocompleteProvider;
class FakeSuggestionsBuilder;

/// Fake AutocompleteController that can be configured to return fake
/// suggestions.
class FakeSuggestionsAutocompleteController : public AutocompleteController {
 public:
  FakeSuggestionsAutocompleteController();
  ~FakeSuggestionsAutocompleteController() override;

  // AutocompleteController methods.
  void Start(const AutocompleteInput& input) override;

  /// Returns the fake suggestion build to create fake suggestions.
  FakeSuggestionsBuilder* fake_suggestions_builder() {
    return suggestions_builder_.get();
  }

 private:
  // The builder for fake suggestions.
  std::unique_ptr<FakeSuggestionsBuilder> suggestions_builder_;

  // The fake provider.
  scoped_refptr<FakeAutocompleteProvider> provider_;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_AUTOCOMPLETE_CONTROLLER_H_
