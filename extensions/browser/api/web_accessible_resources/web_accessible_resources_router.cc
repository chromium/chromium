// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_accessible_resources/web_accessible_resources_router.h"

#include <optional>

#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "url/gurl.h"

namespace extensions {

std::optional<GURL> TransformToDynamicURLIfNecessary(
    const ExtensionId& extension_id,
    const GURL& url,
    content::BrowserContext* browser_context) {
  // Verify that the host matches the extension that initiated the action.
  if (!url.SchemeIs(kExtensionScheme) || url.host() != extension_id) {
    return std::nullopt;
  }

  // Verify that the url's path should use a dynamic url.
  auto* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension || extension->manifest_version() < 3 ||
      !WebAccessibleResourcesInfo::ShouldUseDynamicUrl(extension,
                                                       url.GetPath())) {
    return std::nullopt;
  }

  // Return the dynamic URL host while preserving existing components.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(extension->dynamic_url().host());
  return url.ReplaceComponents(replace_host);
}

}  // namespace extensions
