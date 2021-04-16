// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_SCOPED_PREF_UPDATE_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_SCOPED_PREF_UPDATE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"

class PrefService;

namespace prefs {

class DictionaryValueUpdate;

// An update to a dictionary value pref.
//
// For example:
//   prefs::ScopedDictionaryPrefUpdate update(pref_service(), kPrefKey);
//   int next_id = 0;
//   update->GetInteger("next_id", &next_id);
//
//   std::unique_ptr<prefs::DictionaryValueUpdate> nested_dictionary;
//   if (!update->GetDictionaryWithoutPathExpansion(url.spec(),
//                                                  &nested_dictionary)) {
//     nested_dictionary = update->SetDictionaryWithoutPathExpansion(
//         url.spec(), std::make_unique<base::DictionaryValue>());
//   }
//
//   nested_dictionary->Set("metadata", std::move(metadata));
//   nested_dictionary->SetInteger("id", next_id++);
//   update->SetInteger("next_id", next_id);
//
class ScopedDictionaryPrefUpdate {
 public:
  ScopedDictionaryPrefUpdate(PrefService* service, base::StringPiece path);

  // Notifies if necessary.
  virtual ~ScopedDictionaryPrefUpdate();

  // The caller should not keep the returned object or any further objects
  // obtained from it around for any longer than the lifetime of the
  // ScopedDictionaryPrefUpdate.
  virtual std::unique_ptr<DictionaryValueUpdate> Get();

  // The caller should not keep the returned object or any further objects
  // obtained from it around for any longer than the lifetime of the
  // ScopedDictionaryPrefUpdate.
  std::unique_ptr<DictionaryValueUpdate> operator->();

 private:
  void RecordPath(const std::vector<std::string>& path);

  // Weak pointer.
  PrefService* const service_;
  // Path of the preference being updated.
  const std::string path_;

  // The paths that have been modified.
  std::set<std::vector<std::string>> updated_paths_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDictionaryPrefUpdate);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_SCOPED_PREF_UPDATE_H_
