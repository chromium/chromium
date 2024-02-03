// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

class ExtensionPrefValueMap;

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionPrefs;

class ExtensionPrefsHelper : public KeyedService {
 public:
  using ChromeSettingScope = extensions::api::types::ChromeSettingScope;

  ExtensionPrefsHelper(ExtensionPrefs* prefs, ExtensionPrefValueMap* value_map);

  ExtensionPrefsHelper(const ExtensionPrefsHelper&) = delete;
  ExtensionPrefsHelper& operator=(const ExtensionPrefsHelper&) = delete;

  ~ExtensionPrefsHelper() override;

  // Convenience function to get the ExtensionPrefshelper for a BrowserContext.
  static ExtensionPrefsHelper* Get(content::BrowserContext* context);

  // Functions for manipulating preference values that are controlled by the
  // extension. In other words, these are not pref values *about* the extension,
  // but rather about something global the extension wants to override.

  // Set a new extension-controlled preference value.
  void SetExtensionControlledPref(const ExtensionId& extension_id,
                                  const std::string& pref_key,
                                  ChromeSettingScope scope,
                                  base::Value value);

  // Remove an extension-controlled preference value.
  void RemoveExtensionControlledPref(const ExtensionId& extension_id,
                                     const std::string& pref_key,
                                     ChromeSettingScope scope);

  // Returns true if currently no extension with higher precedence controls the
  // preference.
  bool CanExtensionControlPref(const ExtensionId& extension_id,
                               const std::string& pref_key,
                               bool incognito);

  // Returns true if extension `extension_id` currently controls the
  // preference. If `from_incognito` is not a nullptr value, looks at incognito
  // preferences first, and |from_incognito| is set to true if the effective
  // pref value is coming from the incognito preferences, false if it is coming
  // from the normal ones.
  bool DoesExtensionControlPref(const ExtensionId& extension_id,
                                const std::string& pref_key,
                                bool* from_incognito);

  ExtensionPrefs* prefs() { return prefs_; }

  const ExtensionPrefs* prefs() const { return prefs_; }

 private:
  const raw_ptr<ExtensionPrefs, DanglingUntriaged> prefs_;
  const raw_ptr<ExtensionPrefValueMap, DanglingUntriaged> value_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_HELPER_H_
