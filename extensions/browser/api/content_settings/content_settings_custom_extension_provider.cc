// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_custom_extension_provider.h"

#include <memory>

#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

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
    bool incognito) const {
  return extensions_settings_->GetRuleIterator(content_type,
                                               incognito);
}

bool CustomExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const ContentSettingConstraints& constraints) {
  return false;
}

void CustomExtensionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
  extensions_settings_->RemoveObserver(this);
}

void CustomExtensionProvider::OnContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  if (incognito_ != incognito)
    return;
  // TODO(1245927): Be more concise and use the type/pattern that actually
  // changed.
  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsType::DEFAULT);
}

}  // namespace content_settings
