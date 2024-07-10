// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
#define EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/base/page_transition_types.h"

class GURL;

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
    int child_id,
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

}  // namespace url_request_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
