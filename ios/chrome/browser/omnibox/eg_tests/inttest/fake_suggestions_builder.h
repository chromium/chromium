// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_SUGGESTIONS_BUILDER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_SUGGESTIONS_BUILDER_H_

#import <UIKit/UIKit.h>

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

  /// Adds a shortcut URL match.
  void AddURLShortcut(const std::u16string& shortcut_text,
                      const std::u16string& shortcut_url);

  /// Build the suggestions for the given `input`. A verbatim suggestion is
  /// added by default.
  std::vector<AutocompleteMatch> BuildSuggestionsForInput(
      const AutocompleteInput& input,
      AutocompleteProvider* provider) const;

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

  std::vector<ShortcutData> shortcuts_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_SUGGESTIONS_BUILDER_H_
