// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_SETTINGS_EXTENSION_INSTALL_TIME_PERMISSION_PROVIDER_H_
#define EXTENSIONS_BROWSER_CONTENT_SETTINGS_EXTENSION_INSTALL_TIME_PERMISSION_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

// A provider that returns whether extensions have declared permissions in the
// manifest.
class ExtensionInstallTimePermissionProvider final
    : public content_settings::ObservableProvider {
 public:
  explicit ExtensionInstallTimePermissionProvider(
      extensions::ExtensionRegistry* extension_registry);

  ExtensionInstallTimePermissionProvider(
      const ExtensionInstallTimePermissionProvider&) = delete;
  ExtensionInstallTimePermissionProvider& operator=(
      const ExtensionInstallTimePermissionProvider&) = delete;

  ~ExtensionInstallTimePermissionProvider() override;

  // content_settings::ObservableProvider implementation
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record) const override;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record) const override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;

 private:
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_SETTINGS_EXTENSION_INSTALL_TIME_PERMISSION_PROVIDER_H_
