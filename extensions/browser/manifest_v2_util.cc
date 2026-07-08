// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/manifest_v2_util.h"

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::manifest_v2_util {

bool IsExtensionAffected(const Extension& extension) {
  return IsExtensionAffected(extension.manifest_version(), extension.GetType(),
                             extension.location());
}

bool IsExtensionAffected(int manifest_version,
                         Manifest::Type manifest_type,
                         mojom::ManifestLocation manifest_location) {
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

  // The extension is an MV2 (or lower) extension; we should warn the user
  // about it.
  return true;
}

}  // namespace extensions::manifest_v2_util
