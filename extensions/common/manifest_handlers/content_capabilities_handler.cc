// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/content_capabilities_handler.h"

#include <memory>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

using api::extensions_manifest_types::ContentCapabilities;

ContentCapabilitiesInfo::ContentCapabilitiesInfo() {
}

ContentCapabilitiesInfo::~ContentCapabilitiesInfo() {
}

static base::LazyInstance<ContentCapabilitiesInfo>::DestructorAtExit
    g_empty_content_capabilities_info = LAZY_INSTANCE_INITIALIZER;

// static
const ContentCapabilitiesInfo& ContentCapabilitiesInfo::Get(
    const Extension* extension) {
  ContentCapabilitiesInfo* info = static_cast<ContentCapabilitiesInfo*>(
      extension->GetManifestData(keys::kContentCapabilities));
  return info ? *info : g_empty_content_capabilities_info.Get();
}

ContentCapabilitiesHandler::ContentCapabilitiesHandler() {
}

ContentCapabilitiesHandler::~ContentCapabilitiesHandler() {
}

bool ContentCapabilitiesHandler::Parse(Extension* extension,
                                       std::u16string* error) {
  std::unique_ptr<ContentCapabilitiesInfo> info(new ContentCapabilitiesInfo);

  const base::Value* value =
      extension->manifest()->FindPath(keys::kContentCapabilities);
  if (value == nullptr) {
    *error = errors::kInvalidContentCapabilities;
    return false;
  }

  auto capabilities = ContentCapabilities::FromValue(*value);
  if (!capabilities.has_value()) {
    *error = base::StrCat(
        {errors::kInvalidContentCapabilitiesParsedValue, capabilities.error()});
    return false;
  }

  int supported_schemes = URLPattern::SCHEME_HTTPS;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    // We don't have a suitable HTTPS test server, so this will have to do.
    supported_schemes |= URLPattern::SCHEME_HTTP;
  }

  std::string url_error;
  URLPatternSet potential_url_patterns;
  if (!potential_url_patterns.Populate(capabilities->matches, supported_schemes,
                                       false /* allow_file_access */,
                                       &url_error)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidContentCapabilitiesMatch, url_error);
    return false;
  }

  // Filter URL patterns that imply all hosts or match every host under an
  // eTLD and emit warnings for them.
  // Note: we include private registries because content_capabilities should
  // only be used for a single entity (though possibly multiple domains), rather
  // than blanket-granted.
  const net::registry_controlled_domains::PrivateRegistryFilter private_filter =
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
  std::set<URLPattern> valid_url_patterns;
  for (const URLPattern& pattern : potential_url_patterns) {
    if (pattern.MatchesEffectiveTld(private_filter)) {
      extension->AddInstallWarning(InstallWarning(
          errors::kInvalidContentCapabilitiesMatchOrigin));
    } else {
      valid_url_patterns.insert(pattern);
    }
  }
  info->url_patterns = URLPatternSet(valid_url_patterns);

  // Filter invalid permissions and emit warnings for them.
  for (const std::string& permission_name : capabilities->permissions) {
    const APIPermissionInfo* permission_info = PermissionsInfo::GetInstance()
        ->GetByName(permission_name);
    if (!permission_info || !permission_info->supports_content_capabilities()) {
      extension->AddInstallWarning(InstallWarning(
          errors::kInvalidContentCapabilitiesPermission,
          keys::kContentCapabilities,
          permission_name));
    } else {
      info->permissions.insert(permission_info->CreateAPIPermission());
    }
  }

  extension->SetManifestData(keys::kContentCapabilities, std::move(info));
  return true;
}

base::span<const char* const> ContentCapabilitiesHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kContentCapabilities};
  return kKeys;
}

}  // namespace extensions
