// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_registry.h"

#include <cstdint>
#include <optional>

#include "base/feature_list.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"

namespace extensions {

ExtensionNavigationRegistry::ExtensionNavigationRegistry(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ExtensionNavigationRegistry::~ExtensionNavigationRegistry() = default;

// static
ExtensionNavigationRegistry* ExtensionNavigationRegistry::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>::Get(
      context);
}

// static
BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>*
ExtensionNavigationRegistry::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<ExtensionNavigationRegistry>>
      instance;
  return instance.get();
}

void ExtensionNavigationRegistry::RecordExtensionRedirect(
    int64_t navigation_handle_id,
    const GURL& target_url,
    const ExtensionId& extension_id) {
  if (!IsEnabled()) {
    return;
  }

  redirect_metadata_.emplace(navigation_handle_id,
                             Metadata(target_url, extension_id));
}

void ExtensionNavigationRegistry::Erase(int64_t navigation_handle_id) {
  if (!IsEnabled()) {
    return;
  }

  auto it = redirect_metadata_.find(navigation_handle_id);
  if (it == redirect_metadata_.end()) {
    return;
  }
  redirect_metadata_.erase(it);
}

std::optional<ExtensionNavigationRegistry::Metadata>
ExtensionNavigationRegistry::GetAndErase(int64_t navigation_handle_id) {
  if (!IsEnabled()) {
    return std::nullopt;
  }

  auto it = redirect_metadata_.find(navigation_handle_id);
  if (it == redirect_metadata_.end()) {
    return std::nullopt;
  }

  Metadata metadata = it->second;
  redirect_metadata_.erase(it);
  return metadata;
}

bool ExtensionNavigationRegistry::IsEnabled() {
  return base::FeatureList::IsEnabled(
      extensions_features::kExtensionWARForRedirect);
}

bool ExtensionNavigationRegistry::CanRedirect(int64_t navigation_id,
                                              const GURL& gurl,
                                              const Extension& extension) {
  if (!IsEnabled()) {
    return true;
  }

  std::optional<Metadata> extension_redirect_recorded =
      GetAndErase(navigation_id);

  // Block requests for navigations unaltered by webRequest.
  if (!extension_redirect_recorded.has_value()) {
    return false;
  }

  // Block requests if the extension or url are unexpected.
  // TODO(crbug.com/40060076): Verify WAR access for the recorded extension
  // instead of checking for equality with the target extension.
  auto metadata = extension_redirect_recorded.value();
  if (metadata.gurl != gurl) {
    return false;
  }

  if (metadata.extension_id == extension.id()) {
    return true;
  }

  return true;
}

}  // namespace extensions
