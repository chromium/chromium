// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_ACCESSIBLE_RESOURCES_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_ACCESSIBLE_RESOURCES_INFO_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

// A structure to hold the web accessible extension resources
// that may be specified in the manifest of an extension using
// "web_accessible_resources" key.
struct WebAccessibleResourcesInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  WebAccessibleResourcesInfo();
  ~WebAccessibleResourcesInfo() override;

  struct Entry {
    Entry();
    Entry(Entry&& other);
    ~Entry();

    Entry(URLPatternSet resources,
          URLPatternSet matches,
          std::vector<ExtensionId> extension_ids,
          bool use_dynamic_url,
          bool allow_all_extensions);

    // List of web accessible extension resources.
    URLPatternSet resources;

    // List of urls allowed to access resources.
    URLPatternSet matches;

    // List of extension ids allowed to access resources.
    base::flat_set<ExtensionId> extension_ids;

    // Optionally true to require dynamic urls from sites not in |matches|.
    bool use_dynamic_url;

    // True if "*" is defined as an extension id in the manifest.
    bool allow_all_extensions;
  };

  // Returns true if the specified resource is web accessible.
  static bool IsResourceWebAccessible(const Extension* extension,
                                      const std::string& relative_path,
                                      const url::Origin* initiator_origin);

  // Returns true if the specified resource is web accessible. For redirects.
  static bool IsResourceWebAccessibleRedirect(
      const Extension* extension,
      const GURL& target_url,
      const std::optional<url::Origin>& initiator_origin,
      const GURL& upstream_url);

  // Returns true when 'web_accessible_resources' are defined for the extension.
  static bool HasWebAccessibleResources(const Extension* extension);

  // Accessor for use_dynamic_url.
  static bool ShouldUseDynamicUrl(const Extension* extension,
                                  const std::string& resource);

  // The list of entries for the web-accessible resources of the extension.
  std::vector<Entry> web_accessible_resources;
};

// Parses the "web_accessible_resources" manifest key.
class WebAccessibleResourcesHandler : public ManifestHandler {
 public:
  WebAccessibleResourcesHandler();

  WebAccessibleResourcesHandler(const WebAccessibleResourcesHandler&) = delete;
  WebAccessibleResourcesHandler& operator=(
      const WebAccessibleResourcesHandler&) = delete;

  ~WebAccessibleResourcesHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_ACCESSIBLE_RESOURCES_INFO_H_
