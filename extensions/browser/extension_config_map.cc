// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

ExtensionConfigProvider::ExtensionConfigProvider(ExtensionId extension_id)
    : extension_id_(std::move(extension_id)) {}

ExtensionConfigProvider::~ExtensionConfigProvider() = default;

base::DictValue ExtensionConfigProvider::GetLoadTimeData(
    content::BrowserContext& context) {
  return base::DictValue();
}

const ui::TemplateReplacements*
ExtensionConfigProvider::GetTemplateReplacements(
    content::BrowserContext& context) {
  if (!template_replacements_.has_value()) {
    base::DictValue dict = GetLoadTimeData(context);
    ui::TemplateReplacements replacements;
    ui::TemplateReplacementsFromDictionaryValue(dict, &replacements);
    template_replacements_ = std::move(replacements);
  }
  return &template_replacements_.value();
}

bool ExtensionConfigProvider::IsDynamicResource(const std::string& path) const {
  return path == kDynamicStringsJsPath;
}

std::string ExtensionConfigProvider::GetDynamicResourceContent(
    const std::string& path,
    content::BrowserContext& context) {
  CHECK_EQ(path, kDynamicStringsJsPath);
  base::DictValue dict = GetLoadTimeData(context);
  return base::StringPrintf(kDynamicStringsModuleTemplate,
                            base::WriteJson(dict).value_or("{}").c_str());
}

bool ExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return false;
}

bool ExtensionConfigProvider::ShouldCrashOnJsErrorInDevelopmentBuild() const {
  return false;
}

ExtensionConfigMap::ExtensionConfigMap() = default;

ExtensionConfigMap::~ExtensionConfigMap() = default;

void ExtensionConfigMap::RegisterConfigProvider(
    std::unique_ptr<ExtensionConfigProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(provider);
  ExtensionId extension_id = provider->extension_id();
  auto [it, inserted] =
      providers_.insert({std::move(extension_id), std::move(provider)});
  CHECK(inserted) << "A config provider for component extension '" << it->first
                  << "' is already registered.";
}

ExtensionConfigProvider* ExtensionConfigMap::GetConfigProvider(
    const Extension& extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Manifest::IsComponentLocation(extension.location())) {
    return nullptr;
  }
  return GetConfigProvider(extension.id());
}

ExtensionConfigProvider* ExtensionConfigMap::GetConfigProvider(
    const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FindPtrOrNull(providers_, extension_id);
}

void ExtensionConfigMap::ClearProvidersForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  providers_.clear();
}

}  // namespace extensions
