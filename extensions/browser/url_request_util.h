// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
#define EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "content/public/common/child_process_id.h"
#include "extensions/common/extension_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/template_expressions.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace network {
struct ResourceRequest;
}

namespace extensions {
class Extension;
class ExtensionSet;
class ProcessMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    content::ChildProcessId child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    const GURL& upstream_url,
    bool* allowed);

// Helper method that is called by both AllowCrossRendererResourceLoad and
// ExtensionNavigationThrottle to share logic.
// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoadHelper(bool is_guest,
                                          const Extension* extension,
                                          const Extension* owner_extension,
                                          const std::string& partition_id,
                                          std::string_view resource_path,
                                          ui::PageTransition page_transition,
                                          bool* allowed);

// Returns the `$i18n{key}` template replacements registered for `extension_id`
// in `browser_context`, or nullptr if none are registered.
const ui::TemplateReplacements* GetTemplateReplacementsForExtension(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context);

// Returns true if `mime_type` supports `$i18n{key}` template replacement
// (`text/html`, `text/css`, `text/javascript`, or `application/javascript`).
bool SupportsTemplateReplacement(std::string_view mime_type);

// Applies `replacements` to `resource_bytes` according to `mime_type`. Returns
// `std::nullopt` if `mime_type` does not support template replacement, if any
// `$i18n{...}` key is missing from `replacements`, or if JS template
// replacement fails.
std::optional<std::string> ReplaceTemplateExpressionsForResource(
    std::string_view resource_bytes,
    std::string_view mime_type,
    const ui::TemplateReplacements& replacements);

}  // namespace url_request_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
