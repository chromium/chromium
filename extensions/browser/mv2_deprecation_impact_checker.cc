// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mv2_deprecation_impact_checker.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Creates and returns a singleton instance of the exception list of hashed
// extension IDs.
const std::vector<std::string>& GetHashedExceptionList() {
  static base::NoDestructor<std::vector<std::string>> g_allowlist([] {
    const std::string& string_list =
        extensions_features::kExtensionManifestV2ExceptionListParam.Get();
    return base::SplitString(string_list, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }());

  return *g_allowlist;
}

}  // namespace

MV2DeprecationImpactChecker::MV2DeprecationImpactChecker(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}
MV2DeprecationImpactChecker::~MV2DeprecationImpactChecker() = default;

bool MV2DeprecationImpactChecker::IsExtensionAffected(
    const Extension& extension) {
  return IsExtensionAffected(extension.id(), extension.manifest_version(),
                             extension.GetType(), extension.location(),
                             extension.hashed_id());
}

bool MV2DeprecationImpactChecker::IsExtensionAffected(
    const ExtensionId& extension_id,
    int manifest_version,
    Manifest::Type manifest_type,
    mojom::ManifestLocation manifest_location,
    const HashedExtensionId& hashed_id) {
  // Only extensions < MV3.
  if (manifest_version >= 3) {
    return false;
  }

  // Only extensions (not platform apps, etc). User scripts are treated as
  // extensions for the sake of APIs and in presentation to the user, so we
  // include them here.
  if (manifest_type != Manifest::Type::kExtension &&
      manifest_type != Manifest::Type::kLoginScreenExtension &&
      manifest_type != Manifest::Type::kUserScript) {
    return false;
  }

  // Ignore component extensions (they're implementation details of Chrome).
  if (Manifest::IsComponentLocation(manifest_location)) {
    return false;
  }

  // Extensions with a temporary exception.
  if (std::ranges::contains(GetHashedExceptionList(), hashed_id.value())) {
    return false;
  }

  // The extension is an MV2 (or lower) extension; we should warn the user
  // about it.
  return true;
}

}  // namespace extensions
