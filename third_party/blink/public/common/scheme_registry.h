// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEME_REGISTRY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEME_REGISTRY_H_

#include <string>
#include <unordered_set>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Scheme registry interface that can be used both in the browser and renderer
// processes. See blink::SchemeRegistry for the methods that are only needed
// in renderers. Use url::SchemeRegistry for more generic and lower-layer
// interfaces that can live in //url layer.
//
// "This registry is not thread-safe, registering or removing schemes need to be
// done before multi-thread access to the registry start to happen, or entire
// access to this registry must be done on the single thread. Typically, it is
// expected that Register* methods are called by RegisterContentSchemes during
// the process initialization, and Remove* methods are only called by tests."
//
// TODO (jfernandez): Add DCHECKs to ensure mutable access is done before the
// threads are created.
class BLINK_COMMON_EXPORT CommonSchemeRegistry {
 public:
  // Schemes that represent browser extensions.
  static void RegisterURLSchemeAsExtension(const std::string& scheme);
  static void RegisterURLSchemeAsIsolatedApp(const std::string& scheme);
  static void RemoveURLSchemeAsExtensionForTest(const std::string& scheme);
  static bool IsExtensionScheme(const std::string& scheme);
  static bool IsIsolatedAppScheme(const std::string& scheme);

  // Embedder-defined schemes whose scripts may use the generated code cache,
  // in addition to http(s) and the WebUI schemes. Entries are validated
  // against a source hash like the WebUI schemes.
  static void RegisterURLSchemeAsSupportingCodeCache(const std::string& scheme);
  static void RemoveURLSchemeAsSupportingCodeCacheForTest(
      const std::string& scheme);
  static bool IsCodeCacheAllowedScheme(const std::string& scheme);
  static const std::unordered_set<std::string>& CodeCacheAllowedSchemes();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEME_REGISTRY_H_
