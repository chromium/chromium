// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_

#include "base/observer_list_threadsafe.h"
#include "extensions/browser/value_store/settings_namespace.h"

namespace extensions {

// Interface for classes that listen to changes to extension settings.
class SettingsObserver {
 public:
  // Called when a list of settings have changed for an extension.
  virtual void OnSettingsChanged(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const std::string& changes_json) = 0;

  virtual ~SettingsObserver() {}
};

typedef base::ObserverListThreadSafe<SettingsObserver> SettingsObserverList;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_OBSERVER_H_
