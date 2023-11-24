// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/scoped_pref_update.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"

namespace prefs {

ScopedDictionaryPrefUpdate::ScopedDictionaryPrefUpdate(PrefService* service,
                                                       std::string_view path)
    : service_(service), path_(path) {}

ScopedDictionaryPrefUpdate::~ScopedDictionaryPrefUpdate() {
  if (!updated_paths_.empty())
    service_->ReportUserPrefChanged(path_, std::move(updated_paths_));
}

std::unique_ptr<DictionaryValueUpdate> ScopedDictionaryPrefUpdate::Get() {
  base::Value::Dict& dict =
      service_->GetMutableUserPref(path_, base::Value::Type::DICT)->GetDict();
  return std::make_unique<DictionaryValueUpdate>(
      base::BindRepeating(&ScopedDictionaryPrefUpdate::RecordPath,
                          base::Unretained(this)),
      &dict, std::vector<std::string>());
}

std::unique_ptr<DictionaryValueUpdate> ScopedDictionaryPrefUpdate::
operator->() {
  return Get();
}

void ScopedDictionaryPrefUpdate::RecordPath(std::vector<std::string> path) {
  updated_paths_.insert(std::move(path));
}

}  // namespace prefs
