// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/polyfill_util.h"

#include "base/feature_list.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/devtools_page_handler.h"

namespace extensions {

bool IsExtensionBrowserNamespaceAndPolyfillSupportEnabledForExtension(
    const Extension* extension) {
  bool feature_enabled = base::FeatureList::IsEnabled(
      extensions_features::kExtensionBrowserNamespaceAndPolyfillSupport);

  return feature_enabled &&
         // TODO(crbug.com/401226626): Remove this check when the feature is
         // enabled by default for all APIs.
         // Enable this feature for all extensions except for those that use the
         // devtools API.
         chrome_manifest_urls::GetDevToolsPage(extension).is_empty();
}

}  // namespace extensions
