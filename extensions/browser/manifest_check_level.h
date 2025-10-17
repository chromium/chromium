// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MANIFEST_CHECK_LEVEL_H_
#define EXTENSIONS_BROWSER_MANIFEST_CHECK_LEVEL_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// The amount of manifest checking to perform.
enum class ManifestCheckLevel {
  // Do not check for any manifest equality.
  kNone,

  // Only check that the expected and actual permissions have the same
  // effective permissions.
  kLoose,

  // All data in the expected and actual manifests must match.
  kStrict,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MANIFEST_CHECK_LEVEL_H_
