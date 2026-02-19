// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_BUILDER_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_BUILDER_H_

#import <string>
#import <vector>

class AutocompleteInput;
struct AutocompleteMatch;
class AutocompleteProvider;

/// Builds `AutocompleteMatch` used in tests.
class FakeSuggestionsBuilder {
 public:
  FakeSuggestionsBuilder();
  ~FakeSuggestionsBuilder();

// Suggestions that can be autocompleted.
#pragma mark - Autocompleted suggestions

  /// Adds a shortcut URL match.
  void AddURLShortcut(const std::u16string& shortcut_text,
                      const std::u16string& shortcut_url);

// Suggestions that can't be autocompleted.
#pragma mark - Non autocompleted suggestions

  /// Adds a history URL suggestion.
  void AddHistoryURLSuggestion(const std::u16string& title,
                               const std::u16string& destination_url);

  /// Adds a search suggestion.
  void AddSearchSuggestion(const std::u16string& query);

  /// Build the suggestions for the given `input`. A verbatim suggestion is
  /// added by default.
  std::vector<AutocompleteMatch> BuildSuggestionsForInput(
      const AutocompleteInput& input,
      AutocompleteProvider* provider) const;

  /// Clear all the suggestions that have been added.
  void ClearSuggestions();

 private:
  /// Builds the shortcuts matches for the given `input`.
  void BuildShortcuts(const AutocompleteInput& input,
                      std::vector<AutocompleteMatch>& matches) const;

  /// Shortcuts data used to build shortcut suggestions.
  struct ShortcutData {
    /// Autocompleted shortcut text.
    std::u16string text;
    /// Destination URL.
    std::u16string url;
  };

  // Shortcut matches, added if input text is a prefix of a shortcut text.
  std::vector<ShortcutData> shortcuts_;
  // Regular non autocompleted matches, added for every input.
  std::vector<AutocompleteMatch> matches_;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_FAKE_SUGGESTIONS_BUILDER_H_
