// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_pref_store.h"

#include "base/values.h"
#include "extensions/browser/extension_pref_value_map.h"

ExtensionPrefStore::ExtensionPrefStore(
    ExtensionPrefValueMap* extension_pref_value_map,
    bool incognito_pref_store)
    : extension_pref_value_map_(extension_pref_value_map),
      incognito_pref_store_(incognito_pref_store) {
  extension_pref_value_map_->AddObserver(this);
}

void ExtensionPrefStore::OnInitializationCompleted() {
  NotifyInitializationCompleted();
}

void ExtensionPrefStore::OnPrefValueChanged(const std::string& key) {
  CHECK(extension_pref_value_map_);
  const base::Value* winner = extension_pref_value_map_->GetEffectivePrefValue(
      key, incognito_pref_store_, nullptr);
  if (winner) {
    SetValue(key, winner->Clone(),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else {
    RemoveValue(key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ExtensionPrefStore::OnExtensionPrefValueMapDestruction() {
  CHECK(extension_pref_value_map_);
  extension_pref_value_map_->RemoveObserver(this);
  extension_pref_value_map_ = nullptr;
}

ExtensionPrefStore::~ExtensionPrefStore() {
  if (extension_pref_value_map_) {
    extension_pref_value_map_->RemoveObserver(this);
  }
}
