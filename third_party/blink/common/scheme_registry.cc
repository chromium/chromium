// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheme_registry.h"

#include <unordered_set>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace blink {

using URLSchemesSet = std::unordered_set<std::string>;

URLSchemesSet& GetMutableExtensionSchemes() {
  static base::NoDestructor<URLSchemesSet> extension_schemes;
  return *extension_schemes;
}

const URLSchemesSet& GetExtensionSchemes() {
  return GetMutableExtensionSchemes();
}

void CommonSchemeRegistry::RegisterURLSchemeAsExtension(
    const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  GetMutableExtensionSchemes().insert(scheme);
}

void CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest(
    const std::string& scheme) {
  GetMutableExtensionSchemes().erase(scheme);
}

bool CommonSchemeRegistry::IsExtensionScheme(const std::string& scheme) {
  if (scheme.empty())
    return false;
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  return base::Contains(GetExtensionSchemes(), scheme);
}

}  // namespace blink
