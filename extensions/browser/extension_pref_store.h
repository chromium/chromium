// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREF_STORE_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREF_STORE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/prefs/value_map_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"

// A (non-persistent) PrefStore implementation that holds effective preferences
// set by extensions. These preferences are managed by and fetched from an
// ExtensionPrefValueMap.
class ExtensionPrefStore : public ValueMapPrefStore,
                           public ExtensionPrefValueMap::Observer {
 public:
  // Constructs an ExtensionPrefStore for a regular or an incognito profile.
  ExtensionPrefStore(ExtensionPrefValueMap* extension_pref_value_map,
                     bool incognito_pref_store);

  ExtensionPrefStore(const ExtensionPrefStore&) = delete;
  ExtensionPrefStore& operator=(const ExtensionPrefStore&) = delete;

  // Overrides for ExtensionPrefValueMap::Observer:
  void OnInitializationCompleted() override;
  void OnPrefValueChanged(const std::string& key) override;
  void OnExtensionPrefValueMapDestruction() override;

 protected:
  ~ExtensionPrefStore() override;

 private:
  raw_ptr<ExtensionPrefValueMap> extension_pref_value_map_;  // Weak pointer.
  bool incognito_pref_store_;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREF_STORE_H_
