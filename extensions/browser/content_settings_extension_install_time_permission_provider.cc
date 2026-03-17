// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_settings_extension_install_time_permission_provider.h"

#include <memory>
#include <optional>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

namespace extensions {
namespace {

std::optional<extensions::mojom::APIPermissionID>
ContentSettingsTypeToApiPermission(ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::GEOLOCATION:
      return extensions::mojom::APIPermissionID::kGeolocation;
    case ContentSettingsType::NOTIFICATIONS:
      return extensions::mojom::APIPermissionID::kNotifications;
    default:
      return std::nullopt;
  }
}

std::unique_ptr<content_settings::Rule> CreateRule(const GURL& extension) {
  return std::make_unique<content_settings::Rule>(
      ContentSettingsPattern::FromURLNoWildcard(extension),
      ContentSettingsPattern::Wildcard(), base::Value(CONTENT_SETTING_ALLOW),
      content_settings::RuleMetaData{});
}

class ApiPermissionRuleIterator : public content_settings::RuleIterator {
 public:
  explicit ApiPermissionRuleIterator(std::vector<GURL> extensions)
      : extensions_(std::move(extensions)) {}

  ApiPermissionRuleIterator(const ApiPermissionRuleIterator&) = delete;
  ApiPermissionRuleIterator& operator=(const ApiPermissionRuleIterator&) =
      delete;
  ~ApiPermissionRuleIterator() override = default;

  bool HasNext() const override { return index_ < extensions_.size(); }

  std::unique_ptr<content_settings::Rule> Next() override {
    DCHECK(HasNext());
    return CreateRule(extensions_[index_++]);
  }

 private:
  std::vector<GURL> extensions_;
  size_t index_ = 0;
};

}  // namespace

ExtensionInstallTimePermissionProvider::ExtensionInstallTimePermissionProvider(
    extensions::ExtensionRegistry* extension_registry)
    : extension_registry_(extension_registry) {}

ExtensionInstallTimePermissionProvider::
    ~ExtensionInstallTimePermissionProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
ExtensionInstallTimePermissionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record) const {
  auto api_permission = ContentSettingsTypeToApiPermission(content_type);
  if (!api_permission) {
    return nullptr;
  }

  if (!extension_registry_) {
    return nullptr;
  }

  std::vector<GURL> extensions;
  for (const scoped_refptr<const Extension>& extension :
       extension_registry_->enabled_extensions()) {
    if (extension->permissions_data()->HasAPIPermission(*api_permission)) {
      extensions.emplace_back(extension->url());
    }
  }

  return std::make_unique<ApiPermissionRuleIterator>(std::move(extensions));
}

std::unique_ptr<content_settings::Rule>
ExtensionInstallTimePermissionProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record) const {
  if (!primary_url.SchemeIs(extensions::kExtensionScheme)) {
    return nullptr;
  }

  auto api_permission = ContentSettingsTypeToApiPermission(content_type);
  if (!api_permission) {
    return nullptr;
  }

  if (!extension_registry_) {
    return nullptr;
  }
  const extensions::Extension* extension =
      extension_registry_->enabled_extensions().GetByID(primary_url.GetHost());

  if (extension &&
      extension->permissions_data()->HasAPIPermission(*api_permission)) {
    return CreateRule(extension->url());
  }

  return nullptr;
}

bool ExtensionInstallTimePermissionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  // Not supported.
  return false;
}

void ExtensionInstallTimePermissionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // Not supported.
}

void ExtensionInstallTimePermissionProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  extension_registry_ = nullptr;
}

}  // namespace extensions
