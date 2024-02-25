// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_CUSTOM_EXTENSION_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_CUSTOM_EXTENSION_PROVIDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "extensions/browser/api/content_settings/content_settings_store.h"
#include "extensions/common/extension_id.h"

namespace content_settings {

// A content settings provider which manages settings defined by extensions.
//
// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
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
      bool incognito,
      const content_settings::PartitionKey& partition_key) const override;
  std::unique_ptr<Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const ContentSettingConstraints& constraints,
      const content_settings::PartitionKey& partition_key) override;

  void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override {}

  void ShutdownOnUIThread() override;

  // extensions::ContentSettingsStore::Observer methods:
  void OnContentSettingChanged(const extensions::ExtensionId& extension_id,
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
