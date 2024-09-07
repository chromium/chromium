// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_ACCESSIBLE_RESOURCES_WEB_ACCESSIBLE_RESOURCES_ROUTER_H_
#define EXTENSIONS_BROWSER_API_WEB_ACCESSIBLE_RESOURCES_WEB_ACCESSIBLE_RESOURCES_ROUTER_H_

#include <optional>

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// `use_dynamic_url` as true requires that web accessible resources be loaded
// from a dynamic URL. Return the dynamic URL for the provided static url if it
// points to resources using `use_dynamic_url`.
std::optional<GURL> TransformToDynamicURLIfNecessary(
    const GURL& url,
    content::BrowserContext* browser_context);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_ACCESSIBLE_RESOURCES_WEB_ACCESSIBLE_RESOURCES_ROUTER_H_
