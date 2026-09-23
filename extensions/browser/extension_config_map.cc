// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

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

std::string_view ExtensionConfigProvider::GetChromeURLHost() const {
  return {};
}

void ExtensionConfigProvider::SetDefaultResource(
    std::string_view resource_path) {
  AddResourcePath("/", resource_path);
}

void ExtensionConfigProvider::AddResourcePath(std::string_view url_path,
                                              std::string_view resource_path) {
  CHECK(!GetChromeURLHost().empty());
  CHECK(url_path.starts_with('/'));
  CHECK(resource_path.starts_with('/'));
  auto [_, inserted] = path_map_.try_emplace(url_path, resource_path);
  CHECK(inserted) << "URL path '" << url_path << "' is already mapped.";
  // Each `url_path` may only be added once (e.g. "/foo" cannot be mapped
  // twice), whereas multiple `url_path`s may map to the same `resource_path`
  // (e.g. both "/foo" and "/bar" can map to "/main.html"). The first
  // registered `url_path` serves as the canonical reverse path.
  reverse_path_map_.try_emplace(resource_path, url_path);
}

std::string_view ExtensionConfigProvider::GetResourcePathForUrlPath(
    std::string_view url_path) const {
  auto it = path_map_.find(url_path);
  if (it != path_map_.end()) {
    return it->second;
  }
  return url_path;
}

std::string_view ExtensionConfigProvider::GetUrlPathForResourcePath(
    std::string_view resource_path) const {
  auto it = reverse_path_map_.find(resource_path);
  if (it != reverse_path_map_.end()) {
    return it->second;
  }
  return resource_path;
}

bool ExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return false;
}

bool ExtensionConfigProvider::ShouldCrashOnJsErrorInDevelopmentBuild() const {
  return false;
}

bool ExtensionConfigProvider::IsUnboundedElementAllowed() const {
  return false;
}

// static
bool ExtensionConfigMap::HandleChromeURL(GURL* url,
                                         content::BrowserContext* context) {
  if (!url->SchemeIs(content::kChromeUIScheme)) {
    return false;
  }

  auto* config_map = ExtensionConfigMapFactory::GetForBrowserContext(context);
  if (!config_map) {
    return false;
  }

  const auto* provider =
      config_map->GetConfigProviderByChromeURLHost(url->host());
  if (!provider) {
    return false;
  }

  std::string_view resource_path =
      provider->GetResourcePathForUrlPath(url->path());
  if (resource_path.empty() || resource_path == "/") {
    return false;
  }

  GURL::Replacements replacements;
  replacements.SetSchemeStr(kExtensionScheme);
  replacements.SetHostStr(provider->extension_id());
  replacements.SetPathStr(resource_path);
  *url = url->ReplaceComponents(replacements);
  return true;
}

// static
bool ExtensionConfigMap::HandleChromeURLReverse(
    GURL* url,
    content::BrowserContext* context) {
  if (!url->SchemeIs(kExtensionScheme)) {
    return false;
  }

  auto* config_map = ExtensionConfigMapFactory::GetForBrowserContext(context);
  if (!config_map) {
    return false;
  }

  const auto* provider =
      config_map->GetConfigProviderByExtensionId(url->host());
  if (!provider || provider->GetChromeURLHost().empty()) {
    return false;
  }

  GURL::Replacements replacements;
  replacements.SetSchemeStr(content::kChromeUIScheme);
  replacements.SetHostStr(provider->GetChromeURLHost());
  replacements.SetPathStr(provider->GetUrlPathForResourcePath(url->path()));
  *url = url->ReplaceComponents(replacements);
  return true;
}

ExtensionConfigMap::ExtensionConfigMap(content::BrowserContext& browser_context)
    : browser_context_(browser_context) {}

ExtensionConfigMap::~ExtensionConfigMap() = default;

void ExtensionConfigMap::RegisterConfigProvider(
    std::unique_ptr<ExtensionConfigProvider> provider) {
  CHECK(provider);
  std::string_view extension_id = provider->extension_id();
  std::string_view chrome_url_host = provider->GetChromeURLHost();
  if (!chrome_url_host.empty()) {
    auto [_, host_inserted] =
        chrome_url_host_map_.try_emplace(chrome_url_host, extension_id);
    CHECK(host_inserted) << "A config provider for chrome:// host '"
                         << chrome_url_host << "' is already registered.";
  }
  auto [_, inserted] =
      providers_map_.try_emplace(extension_id, std::move(provider));
  CHECK(inserted) << "A config provider for component extension '"
                  << extension_id << "' is already registered.";
}

ExtensionConfigProvider* ExtensionConfigMap::GetConfigProvider(
    const Extension& extension) {
  if (!Manifest::IsComponentLocation(extension.location())) {
    return nullptr;
  }
  return base::FindPtrOrNull(providers_map_, extension.id());
}

ExtensionConfigProvider* ExtensionConfigMap::GetConfigProviderByExtensionId(
    std::string_view extension_id) {
  ExtensionConfigProvider* provider =
      base::FindPtrOrNull(providers_map_, extension_id);
  if (!provider) {
    return nullptr;
  }
  const Extension* extension = ExtensionRegistry::Get(&*browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(provider->extension_id());
  return extension ? GetConfigProvider(*extension) : nullptr;
}

ExtensionConfigProvider* ExtensionConfigMap::GetConfigProviderByChromeURLHost(
    std::string_view chrome_url_host) {
  if (chrome_url_host.empty()) {
    return nullptr;
  }
  const ExtensionId* extension_id =
      base::FindOrNull(chrome_url_host_map_, chrome_url_host);
  return extension_id ? GetConfigProviderByExtensionId(*extension_id) : nullptr;
}

bool ExtensionConfigMap::IsUnboundedElementAllowed(
    const ExtensionId& extension_id) {
  auto* provider = GetConfigProviderByExtensionId(extension_id);
  return provider && provider->IsUnboundedElementAllowed();
}

void ExtensionConfigMap::ClearProvidersForTesting() {
  providers_map_.clear();
  chrome_url_host_map_.clear();
}

}  // namespace extensions
