// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_custom_extension_provider.h"

#include <memory>

#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "extensions/common/extension_id.h"

namespace content_settings {

CustomExtensionProvider::CustomExtensionProvider(
    const scoped_refptr<extensions::ContentSettingsStore>& extensions_settings,
    bool incognito)
    : incognito_(incognito), extensions_settings_(extensions_settings) {
  extensions_settings_->AddObserver(this);
}

CustomExtensionProvider::~CustomExtensionProvider() = default;

std::unique_ptr<RuleIterator> CustomExtensionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito,
    const content_settings::PartitionKey& partition_key) const {
  return extensions_settings_->GetRuleIterator(content_type,
                                               incognito);
}

std::unique_ptr<content_settings::Rule> CustomExtensionProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  return extensions_settings_->GetRule(primary_url, secondary_url, content_type,
                                       off_the_record);
}

bool CustomExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  return false;
}

void CustomExtensionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
  extensions_settings_->RemoveObserver(this);
}

void CustomExtensionProvider::OnContentSettingChanged(
    const extensions::ExtensionId& extension_id,
    bool incognito) {
  if (incognito_ != incognito) {
    return;
  }
  // TODO(crbug.com/40196354): Be more concise and use the type/pattern that
  // actually changed.
  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsType::DEFAULT, /*partition_key=*/nullptr);
}

}  // namespace content_settings
