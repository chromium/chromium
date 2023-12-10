// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/manual_fill/password_list_sorter.h"

#import <algorithm>

#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<PasswordForm>>* list,
    DuplicatesMap* duplicates) {
  std::vector<std::pair<std::string, std::unique_ptr<PasswordForm>>>
      keys_to_forms;
  keys_to_forms.reserve(list->size());
  for (auto& form : *list) {
    std::string key = CreateSortKey(CredentialUIEntry(*form));
    keys_to_forms.emplace_back(std::move(key), std::move(form));
  }

  std::sort(keys_to_forms.begin(), keys_to_forms.end());

  list->clear();
  duplicates->clear();
  std::string previous_key;
  for (auto& key_to_form : keys_to_forms) {
    if (key_to_form.first != previous_key) {
      list->push_back(std::move(key_to_form.second));
      previous_key = key_to_form.first;
    } else {
      duplicates->emplace(previous_key, std::move(key_to_form.second));
    }
  }
}

}  // namespace password_manager
