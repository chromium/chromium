// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/import_map_error.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ConsoleLogger;
class ImportMapError;
class JSONObject;
class ParsedSpecifier;

// Import maps.
// https://wicg.github.io/import-maps/
// https://github.com/WICG/import-maps/blob/master/spec.md
class CORE_EXPORT ImportMap final : public GarbageCollected<ImportMap> {
 public:
  static ImportMap* Parse(const String& text,
                          const KURL& base_url,
                          ConsoleLogger& logger,
                          absl::optional<ImportMapError>* error_to_rethrow);

  // <spec href="https://wicg.github.io/import-maps/#specifier-map">A specifier
  // map is an ordered map from strings to resolution results.</spec>
  //
  // An invalid KURL corresponds to a null resolution result in the spec.
  //
  // In Blink, we actually use an unordered map here, and related algorithms
  // are implemented differently from the spec.
  using SpecifierMap = HashMap<String, KURL>;

  // <spec href="https://wicg.github.io/import-maps/#import-map-scopes">an
  // ordered map of URLs to specifier maps.</spec>
  using ScopeEntryType = std::pair<String, SpecifierMap>;
  using ScopeType = Vector<ScopeEntryType>;

  // Empty import map.
  ImportMap();

  ImportMap(SpecifierMap&& imports, ScopeType&& scopes);

  // Return values of Resolve(), ResolveImportsMatch() and
  // ResolveImportsMatchInternal():
  // - absl::nullopt: corresponds to returning a null in the spec,
  //   i.e. allowing fallback to a less specific scope etc.
  // - An invalid KURL: corresponds to throwing an error in the spec.
  // - A valid KURL: corresponds to returning a valid URL in the spec.
  absl::optional<KURL> Resolve(const ParsedSpecifier&,
                               const KURL& base_url,
                               String* debug_message) const;

  String ToString() const;

  void Trace(Visitor*) const {}

 private:
  using MatchResult = SpecifierMap::const_iterator;

  // https://wicg.github.io/import-maps/#resolve-an-imports-match
  absl::optional<KURL> ResolveImportsMatch(const ParsedSpecifier&,
                                           const SpecifierMap&,
                                           String* debug_message) const;
  absl::optional<MatchResult> MatchPrefix(const ParsedSpecifier&,
                                          const SpecifierMap&) const;
  static SpecifierMap SortAndNormalizeSpecifierMap(const JSONObject* imports,
                                                   const KURL& base_url,
                                                   ConsoleLogger&);

  KURL ResolveImportsMatchInternal(const String& normalizedSpecifier,
                                   const MatchResult&,
                                   String* debug_message) const;

  // https://wicg.github.io/import-maps/#import-map-imports
  SpecifierMap imports_;

  // https://wicg.github.io/import-maps/#import-map-scopes.
  ScopeType scopes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_
