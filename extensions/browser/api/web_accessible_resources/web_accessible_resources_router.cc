// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_accessible_resources/web_accessible_resources_router.h"

#include <optional>

#include "base/feature_list.h"
#include "base/types/optional_util.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "url/gurl.h"

namespace extensions {

std::optional<GURL> TransformToDynamicURLIfNecessary(
    const GURL& url,
    content::BrowserContext* browser_context) {
  // Verify that the feature is enabled and the host is a valid extension id.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionDynamicURLRedirection) ||
      !url.SchemeIs(kExtensionScheme) ||
      !crx_file::id_util::IdIsValid(url.host())) {
    return std::nullopt;
  }

  // Verify that the url's path should use a dynamic url.
  auto* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(url.host());
  if (!extension || extension->manifest_version() < 3 ||
      !WebAccessibleResourcesInfo::ShouldUseDynamicUrl(extension, url.path())) {
    return std::nullopt;
  }

  // Return the dynamic url.
  return Extension::GetResourceURL(extension->dynamic_url(), url.path());
}

}  // namespace extensions
