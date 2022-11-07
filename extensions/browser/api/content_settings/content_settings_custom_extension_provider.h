// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_CUSTOM_EXTENSION_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_CUSTOM_EXTENSION_PROVIDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "extensions/browser/api/content_settings/content_settings_store.h"

namespace content_settings {

// A content settings provider which manages settings defined by extensions.
class CustomExtensionProvider : public ObservableProvider,
                          public extensions::ContentSettingsStore::Observer {
 public:
  CustomExtensionProvider(const scoped_refptr<extensions::ContentSettingsStore>&
                              extensions_settings,
                          bool incognito);

  CustomExtensionProvider(const CustomExtensionProvider&) = delete;
  CustomExtensionProvider& operator=(const CustomExtensionProvider&) = delete;

  ~CustomExtensionProvider() override;

  // ProviderInterface methods:
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const ContentSettingConstraints& constraint = {}) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override {
  }

  void ShutdownOnUIThread() override;

  // extensions::ContentSettingsStore::Observer methods:
  void OnContentSettingChanged(const std::string& extension_id,
                               bool incognito) override;

 private:
  // Specifies whether this provider manages settings for incognito or regular
  // sessions.
  bool incognito_;

  // The backend storing content setting rules defined by extensions.
  scoped_refptr<extensions::ContentSettingsStore> extensions_settings_;
};

}  // namespace content_settings

#endif  // EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_CUSTOM_EXTENSION_PROVIDER_H_
