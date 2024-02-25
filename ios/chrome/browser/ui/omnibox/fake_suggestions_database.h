// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_FAKE_SUGGESTIONS_DATABASE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_FAKE_SUGGESTIONS_DATABASE_H_

#include <map>
#include <string>

#import "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

class TemplateURLService;

// Stores and serves fake suggestions for internal debugging tools and tests.
// Returned suggestions can be parsed with `SearchSuggestionParser`.
// Fake suggestions can be generated with chrome://suggest-internals on Desktop.
class FakeSuggestionsDatabase {
 public:
  explicit FakeSuggestionsDatabase(TemplateURLService* template_url_service);
  FakeSuggestionsDatabase(const FakeSuggestionsDatabase&) = delete;
  FakeSuggestionsDatabase& operator=(const FakeSuggestionsDatabase&) = delete;
  ~FakeSuggestionsDatabase();

  // Loads fake suggestions from `file_path`.
  // Suggestions must be stored in a json list format and have to be parseable
  // with `SearchSuggestionParser`.
  // The file should come from a trusted source, it will DCHECK if there is any
  // error when reading the file as this is only used in internal tool.
  void LoadSuggestionsFromFile(const base::FilePath& file_path);

  // Returns `true` if there are fake suggestions associated with the search
  // terms of `url`.
  bool HasFakeSuggestions(const GURL& url) const;
  // Returns fake suggestions associated with the search terms of `url`.
  // Returned string can be parsed with `SearchSuggestionParser`.
  std::string GetFakeSuggestions(const GURL& url) const;
  // Adds `fake_suggestions` and associate it with the search terms of `url`.
  void SetFakeSuggestions(const GURL& url, const std::string& fake_suggestions);

 private:
  // Extracts search terms from `url`.
  std::u16string ExtractSearchTerms(const GURL& url) const;
  // Loads fake suggestions from `file_path`.
  void LoadFakeSuggestions(base::FilePath file_path);

  raw_ptr<TemplateURLService> template_url_service_;
  std::map<std::u16string, std::string> data_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_FAKE_SUGGESTIONS_DATABASE_H_
