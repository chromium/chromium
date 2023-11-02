// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CONTENT_CAPABILITIES_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CONTENT_CAPABILITIES_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

// Manifest data describing an extension's set of granted content capabilities.
struct ContentCapabilitiesInfo : public Extension::ManifestData {
  // The set of API permissions to be granted to web content.
  APIPermissionSet permissions;

  // The URL pattern set which should be used to decide which content is granted
  // these capabilities.
  URLPatternSet url_patterns;

  ContentCapabilitiesInfo();
  ~ContentCapabilitiesInfo() override;

  static const ContentCapabilitiesInfo& Get(const Extension* extension);
};

// Parses the "content_capabilities" manifest key.
class ContentCapabilitiesHandler : public ManifestHandler {
 public:
  ContentCapabilitiesHandler();

  ContentCapabilitiesHandler(const ContentCapabilitiesHandler&) = delete;
  ContentCapabilitiesHandler& operator=(const ContentCapabilitiesHandler&) =
      delete;

  ~ContentCapabilitiesHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CONTENT_CAPABILITIES_HANDLER_H_
