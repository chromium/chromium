// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/fake_suggestions_builder.h"

#import <optional>
#import <vector>

#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "net/base/url_util.h"

namespace {

/// Minimum number of character to autocomplete a shortcut suggestion.
constexpr int kShortcutMinChar = 3;

/// Creates a search autocomplete match.
AutocompleteMatch CreateSearchMatch(std::u16string search_terms) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_SUGGEST;
  match.fill_into_edit = search_terms;
  match.contents = search_terms;
  GURL url = GURL("https://www.google.com/search");
  url = net::AppendOrReplaceQueryParameter(url, "q",
                                           base::UTF16ToUTF8(search_terms));
  match.destination_url = url;
  return match;
}

/// Creates a search verbatim match.
AutocompleteMatch CreateVerbatimMatch(std::u16string search_terms) {
  AutocompleteMatch match = CreateSearchMatch(search_terms);
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.allowed_to_be_default_match = true;
  return match;
}

/// Creates a history URL match.
AutocompleteMatch CreateHistoryURLMatch(std::u16string destination_url) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::HISTORY_URL;
  match.destination_url = GURL(destination_url);
  match.fill_into_edit = destination_url;
  return match;
}

/// Creates an URL shortcut match.
AutocompleteMatch CreateShortcutMatch(std::u16string destination_url,
                                      std::u16string shortcut_text,
                                      std::u16string autocomplete_text,
                                      std::u16string additional_text) {
  AutocompleteMatch match = CreateHistoryURLMatch(destination_url);
  match.allowed_to_be_default_match = true;
  match.shortcut_boosted = true;
  match.swap_contents_and_description = true;
  match.contents = destination_url;
  match.description = shortcut_text;
  match.inline_autocompletion = autocomplete_text;
  match.additional_text = additional_text;
  return match;
}

}  // namespace

FakeSuggestionsBuilder::FakeSuggestionsBuilder() : shortcuts_() {}

FakeSuggestionsBuilder::~FakeSuggestionsBuilder() {
  shortcuts_.clear();
}

void FakeSuggestionsBuilder::AddURLShortcut(
    const std::u16string& shortcut_text,
    const std::u16string& shortcut_url) {
  shortcuts_.push_back({shortcut_text, shortcut_url});
}

std::vector<AutocompleteMatch> FakeSuggestionsBuilder::BuildSuggestionsForInput(
    const AutocompleteInput& input,
    AutocompleteProvider* provider) const {
  auto matches = std::vector<AutocompleteMatch>();
  // Build shortcut matches.
  BuildShortcuts(input, matches);

  // Add a verbatim match.
  if (!input.prevent_inline_autocomplete() && matches.size() &&
      matches.front().allowed_to_be_default_match) {
    matches.insert(matches.begin() + 1, CreateVerbatimMatch(input.text()));
  } else {
    matches.insert(matches.begin(), CreateVerbatimMatch(input.text()));
  }

  // Set a decreasing relevance.
  int relevance = 1500;
  for (auto&& match : matches) {
    match.provider = provider;
    match.relevance = relevance--;
  }
  return matches;
}

#pragma mark - Private

void FakeSuggestionsBuilder::BuildShortcuts(
    const AutocompleteInput& input,
    std::vector<AutocompleteMatch>& matches) const {
  for (auto&& shortcut : shortcuts_) {
    if (input.text().length() >= kShortcutMinChar &&
        base::StartsWith(shortcut.text, input.text())) {
      matches.push_back(CreateShortcutMatch(
          shortcut.url, shortcut.text,
          shortcut.text.substr(input.text().length()), shortcut.url));
    }
  }
}
