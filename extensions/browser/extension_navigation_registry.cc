// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_registry.h"

#include <optional>

#include "base/feature_list.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_features.h"

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
    const GURL& target_url) {
  if (!IsEnabled()) {
    return;
  }

  redirect_metadata_.emplace(navigation_handle_id, target_url);
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

std::optional<GURL> ExtensionNavigationRegistry::GetAndErase(
    int64_t navigation_handle_id) {
  if (!IsEnabled()) {
    return std::nullopt;
  }

  auto it = redirect_metadata_.find(navigation_handle_id);
  if (it == redirect_metadata_.end()) {
    return std::nullopt;
  }
  GURL result = it->second;
  redirect_metadata_.erase(it);
  return result;
}

bool ExtensionNavigationRegistry::IsEnabled() {
  return base::FeatureList::IsEnabled(
      extensions_features::kExtensionWARForRedirect);
}

}  // namespace extensions
