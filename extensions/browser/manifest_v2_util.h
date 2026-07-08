// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MANIFEST_V2_UTIL_H_
#define EXTENSIONS_BROWSER_MANIFEST_V2_UTIL_H_

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class Extension;

// Helper methods to determine if an extension is affected by the MV2
// deprecation.
// NOTE: Instead of using this method directly, callers should go through the
// ManifestV2Handler.

namespace manifest_v2_util {

// Returns true if the given `extension` is a manifest v2 extension that should
// be restricted from installation.
bool IsExtensionAffected(const Extension& extension);
// Same as above, but allows for passing in the relevant bits from the
// extension directly in case the `Extension` object doesn't yet exist.
bool IsExtensionAffected(int manifest_version,
                         Manifest::Type manifest_type,
                         mojom::ManifestLocation manifest_location);

}  // namespace manifest_v2_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MANIFEST_V2_UTIL_H_
